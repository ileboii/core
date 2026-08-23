#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Policies/SingletonImp.h"
#include "Item.h"
#include "AuctionHouseMgr.h"
#include "ObjectMgr.h"
#include "AuctionHouseBotMgr.h"
#include "AuctionHouseBot/AuctionHouseBot.h"
#include "Config/Config.h"
#include "Chat.h"
#include "Utilities/Random.h"

#include <sstream>

INSTANTIATE_SINGLETON_1(AuctionHouseBotMgr);

AuctionHouseBotMgr::~AuctionHouseBotMgr()
{
    m_items.clear();

    if (m_config)
        m_config.reset();
}

void AuctionHouseBotMgr::Load()
{
    /* 1 - DELETE */
    m_items.clear();
    m_loaded = false;

    if (m_config)
        m_config.reset();

    /*2 - LOAD */
    std::unique_ptr<QueryResult> result(WorldDatabase.Query("SELECT `item`, `stack`, `bid`, `buyout` FROM `auctionhousebot`"));

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();

        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "");
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> Loaded 0 AuctionHouseBot items");
        return;
    }

    uint32 count = 0;
    BarGoLink bar(result->GetRowCount());

    Field* fields;
    do
    {
        bar.step();
        AuctionHouseBotEntry e;
        fields    = result->Fetch();
        e.item    = fields[0].GetUInt32();
        e.stack   = fields[1].GetUInt32();
        e.bid     = fields[2].GetUInt32();
        e.buyout  = fields[3].GetUInt32();

        m_items.push_back(e);

        ++count;
    }
    while (result->NextRow());

    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "");
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, ">> Loaded %u AuctionHouseBot items", count);

    /* CONFIG */
    m_config                 = std::make_unique<AuctionHouseBotConfig>();
    m_config->enable         = sConfig.GetBoolDefault("AHBot.Enable", false);
    m_config->ahfid          = sConfig.GetIntDefault("AHBot.ah.fid", 120);
    m_config->itemcount      = sConfig.GetIntDefault("AHBot.itemcount", 2);

    m_auctionHouseEntry = sAuctionMgr.GetAuctionHouseEntry(m_config->ahfid);
    if (!m_auctionHouseEntry)
    {
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "AHBot::Load() : No auction house for faction %u.", m_config->ahfid);
        return;
    }
    m_loaded = true;
}

void AuctionHouseBotMgr::Update(bool force /* = false */)
{
    if (!m_loaded)
        return;

    ASSERT(m_config);
    ASSERT(m_auctionHouseEntry);

    if (!(m_config->enable || force))
        return;

    if (m_items.empty())
    {
        sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "AHBot::Update() : Bad config or empty table.");
        return;
    }

    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(m_auctionHouseEntry);
    if (!auctionHouse)
    {
        sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "AHBot::Update() : No auction house for faction %u.", m_config->ahfid);
        return;
    }

    uint32 auctions     = auctionHouse->GetCount();
    uint32 items        = m_config->itemcount;
    uint32 entriesCount = m_items.size();

    while (auctions < items)
    {
        AuctionHouseBotEntry item = m_items[urand(0, entriesCount - 1)];
        AddItem(item, auctionHouse);
        auctions++;
    }
}

void AuctionHouseBotMgr::AddItem(AuctionHouseBotEntry e, AuctionHouseObject *auctionHouse)
{
    ASSERT(m_auctionHouseEntry);

    ItemPrototype const* prototype = sObjectMgr.GetItemPrototype(e.item);
    if (prototype == nullptr)
    {
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "AHBot::AddItem() : Item %u does not exist.", e.item);
        return;
    }

    Item* item = Item::CreateItem(e.item, 1);
    if (!item)
    {
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "AHBot::AddItem() : Cannot create item.");
        return;
    }

    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "AHBot::AddItem() : Adding item %u.", e.item);

    uint32 randomPropertyId = Item::GenerateItemRandomPropertyId(e.item);
    if (randomPropertyId != 0)
        item->SetItemRandomProperties(randomPropertyId);

    uint32 etime = urand(1, 3);
    switch (etime)
    {
        case 1:
            etime = 43200;
            break;
        case 2:
            etime = 86400;
            break;
        case 3:
            etime = 172800;
            break;
        default:
            etime = 86400;
            break;
    }
    item->SetCount(e.stack);

    uint32 dep = sAuctionMgr.GetAuctionDeposit(m_auctionHouseEntry, etime, item);

    AuctionEntry* auctionEntry       = new AuctionEntry;
    auctionEntry->Id                 = sObjectMgr.GenerateAuctionID();
    auctionEntry->auctionHouseEntry  = m_auctionHouseEntry;
    auctionEntry->itemGuidLow        = item->GetGUIDLow();
    auctionEntry->itemTemplate       = item->GetEntry();
    auctionEntry->owner              = 0;
    auctionEntry->startbid           = e.bid;
    auctionEntry->buyout             = e.buyout;
    auctionEntry->bidder             = 0;
    auctionEntry->bid                = 0;
    auctionEntry->deposit            = dep;
    auctionEntry->depositTime        = time(nullptr);
    auctionEntry->expireTime         = (time_t) etime + time(nullptr);

    item->SaveToDB();

    sAuctionMgr.AddAItem(item);
    auctionHouse->AddAuction(auctionEntry);
    auctionEntry->SaveToDB();
}

bool ChatHandler::HandleAHBotUpdateCommand(char *args)
{
    sAuctionHouseBotMgr.Update(true);
    SendSysMessage("[AHBot] Update finished.");
    return true;
}

bool ChatHandler::HandleAHBotReloadCommand(char *args)
{
    sAuctionHouseBotMgr.Load();
    sAuctionHouseBot.ReloadAllConfig();
    SendSysMessage("[AHBot] Reload finished.");
    return true;
}

bool ChatHandler::HandleAHBotRebuildCommand(char *args)
{
    bool all = args && std::string(args) == "all";
    sAuctionHouseBot.Rebuild(all);
    if (all)
        SendSysMessage("[AHBot] Rebuild (all) started.");
    else
        SendSysMessage("[AHBot] Rebuild started.");
    return true;
}

bool ChatHandler::HandleAHBotStatusCommand(char *args)
{
    AuctionHouseBotStatusInfo statusInfo;
    sAuctionHouseBot.PrepareStatusInfos(statusInfo);
    char const* houseNames[MAX_AHBOT_HOUSE_TYPE] = { "Alliance", "Horde", "Neutral" };
    for (uint32 i = 0; i < MAX_AHBOT_HOUSE_TYPE; ++i)
    {
        PSendSysMessage("[AHBot] %s: %u items (Gray %u, White %u, Green %u, Blue %u, Purple %u, Orange %u)",
            houseNames[i], statusInfo[i].ItemsCount,
            statusInfo[i].QualityInfo[0], statusInfo[i].QualityInfo[1],
            statusInfo[i].QualityInfo[2], statusInfo[i].QualityInfo[3],
            statusInfo[i].QualityInfo[4], statusInfo[i].QualityInfo[5]);
    }
    return true;
}

bool ChatHandler::HandleAHBotItemCommand(char *args)
{
    if (!args || !*args)
    {
        SendSysMessage("Syntax: .ahbot item #itemid [$itemvalue [$addchance [$minamount [$maxamount]]]] [reset]");
        return true;
    }

    std::string argsStr(args);
    bool reset = false;
    // Check for trailing "reset"
    size_t resetPos = argsStr.find("reset");
    if (resetPos != std::string::npos)
    {
        reset = true;
        argsStr = argsStr.substr(0, resetPos);
    }

    std::istringstream iss(argsStr);
    uint32 itemId = 0;
    iss >> itemId;
    if (!itemId)
    {
        SendSysMessage("Invalid item id.");
        return true;
    }

    ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
    if (!proto)
    {
        PSendSysMessage("Item %u does not exist.", itemId);
        return true;
    }

    if (reset)
    {
        AuctionHouseBotItemData itemData;
        sAuctionHouseBot.SetItemData(itemId, itemData, true);
        PSendSysMessage("[AHBot] Item %u (%s) configuration reset.", itemId, proto->Name1);
        return true;
    }

    AuctionHouseBotItemData itemData = sAuctionHouseBot.GetItemData(itemId);

    uint32 value = 0, addChance = 0, minAmount = 0, maxAmount = 0;
    bool hasValue = false;
    if (iss >> value)
    {
        hasValue = true;
        itemData.Value = value;
        if (iss >> addChance)
            itemData.AddChance = addChance;
        if (iss >> minAmount)
            itemData.MinAmount = minAmount;
        if (iss >> maxAmount)
            itemData.MaxAmount = maxAmount;
    }

    if (hasValue)
    {
        sAuctionHouseBot.SetItemData(itemId, itemData);
        PSendSysMessage("[AHBot] Item %u (%s) set: value=%u, addChance=%u, min=%u, max=%u",
            itemId, proto->Name1, itemData.Value, itemData.AddChance, itemData.MinAmount, itemData.MaxAmount);
    }
    else
    {
        PSendSysMessage("[AHBot] Item %u (%s): value=%u, addChance=%u, min=%u, max=%u",
            itemId, proto->Name1, itemData.Value, itemData.AddChance, itemData.MinAmount, itemData.MaxAmount);
    }
    return true;
}
