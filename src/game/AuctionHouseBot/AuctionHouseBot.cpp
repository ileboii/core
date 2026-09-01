// Ported from CMaNGOS AuctionHouseBot to VMaNGOS.

#include "AuctionHouseBot.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Policies/Singleton.h"
#include "ProgressBar.h"
#include "SystemConfig.h"
#include "World.h"
#include "Database/DatabaseEnv.h"

#define AUCTIONHOUSEBOT_CONF_VERSION    2021011201

INSTANTIATE_SINGLETON_1(AuctionHouseBot);

// VMaNGOS: Map house type index to DBC houseId
// Alliance=1, Horde=6, Neutral=7
uint32 AuctionHouseBot::GetHouseIdForType(uint32 houseType)
{
    switch (houseType)
    {
        case AHBOT_HOUSE_ALLIANCE: return 1;
        case AHBOT_HOUSE_HORDE:    return 6;
        case AHBOT_HOUSE_NEUTRAL:  return 7;
        default: return 7;
    }
}

AuctionHouseEntry const* AuctionHouseBot::GetAHEntryForType(uint32 houseType) const
{
    return sAuctionHouseStore.LookupEntry(GetHouseIdForType(houseType));
}

AuctionHouseBot::AuctionHouseBot() : m_configFileName(_AUCTIONHOUSEBOT_CONFIG), m_houseAction(-1)
{
}

AuctionHouseBot::~AuctionHouseBot()
{
}

void AuctionHouseBot::Initialize()
{
    if (!m_ahBotCfg.LoadFromFile(m_configFileName))
    {
        m_chanceBuy = 0;
        m_chanceSell = 0;
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "AHBot is disabled. Unable to open configuration file(%s).", m_configFileName.c_str());
        return;
    }
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "AHBot using configuration file %s", m_configFileName.c_str());

    // Ensure the ahbot_items table exists in the character database
    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS `ahbot_items` ("
        " `item` int(11) unsigned NOT NULL,"
        " `value` int(11) unsigned NOT NULL DEFAULT '0',"
        " `add_chance` int(11) unsigned NOT NULL DEFAULT '0',"
        " `min_amount` int(11) unsigned NOT NULL DEFAULT '0',"
        " `max_amount` int(11) unsigned NOT NULL DEFAULT '0',"
        " PRIMARY KEY (`item`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8");

    m_chanceSell = GetMinMaxConfig("AuctionHouseBot.Chance.Sell", 0, 100, 10);
    m_chanceBuy = GetMinMaxConfig("AuctionHouseBot.Chance.Buy", 0, 100, 10);

    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "AHBot selling items: %s", m_chanceSell > 0 ? "Enabled" : "Disabled");
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "AHBot buying items: %s", m_chanceBuy > 0 ? "Enabled" : "Disabled");

    if (m_chanceSell > 0 || m_chanceBuy > 0)
    {
        ParseLootConfig("AuctionHouseBot.Loot.Creature.Normal", m_creatureLootNormalConfig);
        ParseLootConfig("AuctionHouseBot.Loot.Creature.Elite", m_creatureLootEliteConfig);
        ParseLootConfig("AuctionHouseBot.Loot.Creature.RareElite", m_creatureLootRareEliteConfig);
        ParseLootConfig("AuctionHouseBot.Loot.Creature.WorldBoss", m_creatureLootWorldBossConfig);
        ParseLootConfig("AuctionHouseBot.Loot.Creature.Rare", m_creatureLootRareConfig);
        FillUintVectorFromQuery("SELECT entry FROM creature_template WHERE `rank` = 0 AND entry IN (SELECT entry FROM creature_loot_template)", m_creatureLootNormalTemplates);
        FillUintVectorFromQuery("SELECT entry FROM creature_template WHERE `rank` = 1 AND entry IN (SELECT entry FROM creature_loot_template)", m_creatureLootEliteTemplates);
        FillUintVectorFromQuery("SELECT entry FROM creature_template WHERE `rank` = 2 AND entry IN (SELECT entry FROM creature_loot_template)", m_creatureLootRareEliteTemplates);
        FillUintVectorFromQuery("SELECT entry FROM creature_template WHERE `rank` = 3 AND entry IN (SELECT entry FROM creature_loot_template)", m_creatureLootWorldBossTemplates);
        FillUintVectorFromQuery("SELECT entry FROM creature_template WHERE `rank` = 4 AND entry IN (SELECT entry FROM creature_loot_template)", m_creatureLootRareTemplates);

        ParseLootConfig("AuctionHouseBot.Loot.Disenchant", m_disenchantLootConfig);
        FillUintVectorFromQuery("SELECT DISTINCT entry FROM disenchant_loot_template", m_disenchantLootTemplates);

        ParseLootConfig("AuctionHouseBot.Loot.Fishing", m_fishingLootConfig);
        FillUintVectorFromQuery("SELECT DISTINCT entry FROM fishing_loot_template", m_fishingLootTemplates);

        ParseLootConfig("AuctionHouseBot.Loot.Gameobject", m_gameobjectLootConfig);
        FillUintVectorFromQuery("SELECT DISTINCT entry FROM gameobject_loot_template WHERE entry IN (SELECT data1 FROM gameobject_template WHERE entry IN (SELECT id FROM gameobject WHERE spawntimesecsmax > 0))", m_gameobjectLootTemplates);

        ParseLootConfig("AuctionHouseBot.Loot.Skinning", m_skinningLootConfig);
        FillUintVectorFromQuery("SELECT DISTINCT entry FROM skinning_loot_template", m_skinningLootTemplates);

        ParseLootConfig("AuctionHouseBot.Items.Profession", m_professionItemsConfig);
        FillUintVectorFromQuery("SELECT entry FROM item_template WHERE entry IN (SELECT EffectItemType1 FROM spell_template WHERE attributes & 32 AND attributes & 65536)", m_professionItems);

        std::vector<uint32> tmpVector;
        FillUintVectorFromQuery("SELECT item FROM npc_vendor", tmpVector);
        std::copy(tmpVector.begin(), tmpVector.end(), std::inserter(m_vendorItems, m_vendorItems.end()));

        ParseItemValueConfig("AuctionHouseBot.Value.Poor", m_itemValue[ITEM_QUALITY_POOR]);
        ParseItemValueConfig("AuctionHouseBot.Value.Normal", m_itemValue[ITEM_QUALITY_NORMAL]);
        ParseItemValueConfig("AuctionHouseBot.Value.Uncommon", m_itemValue[ITEM_QUALITY_UNCOMMON]);
        ParseItemValueConfig("AuctionHouseBot.Value.Rare", m_itemValue[ITEM_QUALITY_RARE]);
        ParseItemValueConfig("AuctionHouseBot.Value.Epic", m_itemValue[ITEM_QUALITY_EPIC]);
        ParseItemValueConfig("AuctionHouseBot.Value.Legendary", m_itemValue[ITEM_QUALITY_LEGENDARY]);
        ParseItemValueConfig("AuctionHouseBot.Value.Artifact", m_itemValue[ITEM_QUALITY_ARTIFACT]);

        m_vendorValue = m_ahBotCfg.GetBoolDefault("AuctionHouseBot.Value.Vendor", true);
        m_valueVariance = GetMinMaxConfig("AuctionHouseBot.Value.Variance", 0, 100, 10);

        m_auctionBidMin = GetMinMaxConfig("AuctionHouseBot.Bid.Min", 0, 100, 75);
        m_auctionBidMax = GetMinMaxConfig("AuctionHouseBot.Bid.Max", 0, 100, 90);
        if (m_auctionBidMin > m_auctionBidMax)
        {
            sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "AHBot error: AuctionHouseBot.Bid.Min must be less or equal to AuctionHouseBot.Bid.Max. Setting Bid.Min equal to Bid.Max.");
            m_auctionBidMin = m_auctionBidMax;
        }

        m_auctionTimeMin = GetMinMaxConfig("AuctionHouseBot.Time.Min", 1, 72, 2);
        m_auctionTimeMax = GetMinMaxConfig("AuctionHouseBot.Time.Max", 1, 72, 24);
        if (m_auctionTimeMin > m_auctionTimeMax)
        {
            sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "AHBot error: AuctionHouseBot.Time.Min must be less or equal to AuctionHouseBot.Time.Max. Setting Time.Min equal to Time.Max.");
            m_auctionTimeMin = m_auctionTimeMax;
        }

        m_buyValue = GetMinMaxConfig("AuctionHouseBot.Buy.Value", 0, 200, 90);

        auto queryResult = CharacterDatabase.PQuery("SELECT item, value, add_chance, min_amount, max_amount FROM ahbot_items");
        if (queryResult)
        {
            do
            {
                Field* fields = queryResult->Fetch();
                uint32 itemId = fields[0].GetUInt32();
                AuctionHouseBotItemData itemData;
                itemData.Value = fields[1].GetUInt32();
                itemData.AddChance = fields[2].GetUInt32();
                itemData.MinAmount = fields[3].GetUInt32();
                itemData.MaxAmount = fields[4].GetUInt32();
                m_itemData[itemId] = itemData;
            }
            while (queryResult->NextRow());
        }
    }
}

void AuctionHouseBot::Update()
{
    if (++m_houseAction >= (int32)MAX_AHBOT_HOUSE_TYPE * 2)
        m_houseAction = 0;

    uint32 houseType = m_houseAction % MAX_AHBOT_HOUSE_TYPE;
    AuctionHouseEntry const* ahEntry = GetAHEntryForType(houseType);
    if (!ahEntry)
        return;
    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(ahEntry);
    if (!auctionHouse)
        return;

    if (m_houseAction < (int32)MAX_AHBOT_HOUSE_TYPE && urand(0, 99) < m_chanceSell)
    {
        std::unordered_map<uint32, uint32> itemMap;

        AddLootToItemMap(&LootTemplates_Creature, m_creatureLootNormalConfig, m_creatureLootNormalTemplates, itemMap);
        AddLootToItemMap(&LootTemplates_Creature, m_creatureLootEliteConfig, m_creatureLootEliteTemplates, itemMap);
        AddLootToItemMap(&LootTemplates_Creature, m_creatureLootRareEliteConfig, m_creatureLootRareEliteTemplates, itemMap);
        AddLootToItemMap(&LootTemplates_Creature, m_creatureLootWorldBossConfig, m_creatureLootWorldBossTemplates, itemMap);
        AddLootToItemMap(&LootTemplates_Creature, m_creatureLootRareConfig, m_creatureLootRareTemplates, itemMap);

        AddLootToItemMap(&LootTemplates_Disenchant, m_disenchantLootConfig, m_disenchantLootTemplates, itemMap);
        AddLootToItemMap(&LootTemplates_Fishing, m_fishingLootConfig, m_fishingLootTemplates, itemMap);
        AddLootToItemMap(&LootTemplates_Gameobject, m_gameobjectLootConfig, m_gameobjectLootTemplates, itemMap);
        AddLootToItemMap(&LootTemplates_Skinning, m_skinningLootConfig, m_skinningLootTemplates, itemMap);

        if (m_professionItemsConfig.size() >= 4 && m_professionItemsConfig[1] > 0 && m_professionItemsConfig[3] > 0 && m_professionItems.size() > 0)
        {
            int32 maxTemplates = m_professionItemsConfig[0] < 0 ? urand(0, m_professionItemsConfig[1] - m_professionItemsConfig[0]) + m_professionItemsConfig[0] : urand(m_professionItemsConfig[0], m_professionItemsConfig[1]);
            if (maxTemplates > 0)
            {
                for (int32 templateCounter = 0; templateCounter < maxTemplates; ++templateCounter)
                {
                    uint32 item = m_professionItems[urand(0, m_professionItems.size() - 1)];
                    ItemPrototype const* prototype = sObjectMgr.GetItemPrototype(item);
                    if (!prototype || prototype->Quality == 0 || urand(0, (1 << (prototype->Quality - 1)) - 1) > 0)
                        continue;
                    uint32 count = (uint32) round((uint64)prototype->GetMaxStackSize() * urand(m_professionItemsConfig[2], m_professionItemsConfig[3]) / 100.0);
                    if (count <= 0)
                        count = 1;
                    itemMap[item] += count;
                }
            }
        }

        for (auto& itemData : m_itemData)
        {
            if (itemData.second.AddChance > 0)
                itemMap[itemData.first] = urand(0, 99) < itemData.second.AddChance ? urand(itemData.second.MinAmount, itemData.second.MaxAmount) : 0;
        }

        for (auto& itemEntry : itemMap)
        {
            ItemPrototype const* prototype = sObjectMgr.GetItemPrototype(itemEntry.first);
            if (!prototype || prototype->GetMaxStackSize() == 0)
                continue;
            auto iterator = m_itemData.find(prototype->ItemId);
            if (iterator != m_itemData.end() && iterator->second.Value == 0)
                continue;
            if (iterator == m_itemData.end() || iterator->second.AddChance == 0)
            {
                if (prototype->Bonding == BIND_WHEN_PICKED_UP || prototype->Bonding == BIND_QUEST_ITEM)
                    continue;
                if (prototype->Flags & ITEM_FLAG_LOOTABLE)
                    continue;
                if (m_itemValue[prototype->Quality][prototype->Class] == 0)
                    continue;
            }

            uint32 itemValue = ValueWithVariance(iterator != m_itemData.end() ? iterator->second.Value : CalculateBuyoutPrice(prototype));
            for (uint32 stackCounter = 0; stackCounter < itemEntry.second; stackCounter += prototype->GetMaxStackSize())
            {
                uint32 count = itemEntry.second - stackCounter > prototype->GetMaxStackSize() ? prototype->GetMaxStackSize() : itemEntry.second - stackCounter;
                uint32 buyoutPrice = itemValue * count;
                Item* item = Item::CreateItem(itemEntry.first, count);
                if (buyoutPrice == 0 || !item)
                    continue;
                uint32 bidPrice = buyoutPrice * (urand(m_auctionBidMin, m_auctionBidMax)) / 100;

                uint32 auctionTime = urand(m_auctionTimeMin, m_auctionTimeMax) * HOUR;

                uint32 randomPropertyId = Item::GenerateItemRandomPropertyId(itemEntry.first);
                if (randomPropertyId)
                    item->SetItemRandomProperties(randomPropertyId);

                AuctionEntry* auctionEntry = new AuctionEntry;
                auctionEntry->Id = sObjectMgr.GenerateAuctionID();
                auctionEntry->itemGuidLow = item->GetObjectGuid().GetCounter();
                auctionEntry->itemTemplate = item->GetEntry();
                auctionEntry->owner = 0;
                auctionEntry->ownerAccount = 0;
                auctionEntry->startbid = bidPrice;
                auctionEntry->bidder = 0;
                auctionEntry->bid = 0;
                auctionEntry->buyout = buyoutPrice;
                auctionEntry->depositTime = time(nullptr);
                auctionEntry->expireTime = time(nullptr) + auctionTime;
                auctionEntry->deposit = 0;
                auctionEntry->auctionHouseEntry = ahEntry;

                sAuctionMgr.AddAItem(item);
                auctionHouse->AddAuction(auctionEntry);
                item->SaveToDB();
                auctionEntry->SaveToDB();
            }
        }
    }
    else if (m_houseAction >= (int32)MAX_AHBOT_HOUSE_TYPE && urand(0, 99) < m_chanceBuy)
    {
        AuctionHouseObject::AuctionEntryMap const& auctionEntryMap = *auctionHouse->GetAuctions();
        std::vector<AuctionEntry*> buyoutAuctions;
        for (AuctionHouseObject::AuctionEntryMap::const_iterator itr = auctionEntryMap.begin(); itr != auctionEntryMap.end(); ++itr)
        {
            AuctionEntry* auction = itr->second;
            if (auction->owner == 0 && auction->bid == 0)
                continue;
            Item* item = sAuctionMgr.GetAItem(auction->itemGuidLow);
            if (!item)
                continue;
            auto prototype = item->GetProto();
            if (!prototype)
                continue;
            auto iterator = m_itemData.find(prototype->ItemId);
            if (iterator != m_itemData.end() && iterator->second.Value == 0)
                continue;

            uint32 buyItemCheck = ValueWithVariance(iterator != m_itemData.end() ? iterator->second.Value : CalculateBuyoutPrice(prototype));
            buyItemCheck *= item->GetCount();
            uint32 bidPrice = auction->bid + auction->GetAuctionOutBid();
            if (auction->startbid > bidPrice)
                bidPrice = auction->startbid;
            if (auction->buyout > 0 && buyItemCheck > auction->buyout)
            {
                buyoutAuctions.push_back(auction);
            }
            else if (buyItemCheck > bidPrice)
            {
                auction->bidder = 0;
                auction->bid = bidPrice;
                CharacterDatabase.PExecute("UPDATE auction SET buyer_guid = '%u', last_bid = '%u' WHERE id = '%u'",
                    auction->bidder, auction->bid, auction->Id);
            }
        }
        for (auto auction : buyoutAuctions)
        {
            auction->bidder = 0;
            auction->bid = auction->buyout;
            CharacterDatabase.PExecute("UPDATE auction SET buyer_guid = '%u', last_bid = '%u' WHERE id = '%u'",
                auction->bidder, auction->bid, auction->Id);
        }
    }
}

bool AuctionHouseBot::ReloadAllConfig()
{
    Initialize();
    return true;
}

void AuctionHouseBot::Rebuild(bool all)
{
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "AHBot: Rebuilding auction house items");
    for (uint32 i = 0; i < MAX_AHBOT_HOUSE_TYPE; ++i)
    {
        AuctionHouseEntry const* ahEntry = GetAHEntryForType(i);
        if (!ahEntry)
            continue;
        AuctionHouseObject* ah = sAuctionMgr.GetAuctionsMap(ahEntry);
        if (!ah)
            continue;
        AuctionHouseObject::AuctionEntryMap const& auctions = *ah->GetAuctions();
        for (AuctionHouseObject::AuctionEntryMap::const_iterator itr = auctions.begin(); itr != auctions.end(); ++itr)
        {
            AuctionEntry* entry = itr->second;
            if (!entry->owner)
            {
                if (all || entry->bid == 0)
                    entry->expireTime = sWorld.GetGameTime();
            }
        }
    }
    uint32 updateCounter = ((m_auctionTimeMax - m_auctionTimeMin) / 2 + m_auctionTimeMin) * 90;
    for (uint32 i = 0; i < updateCounter; ++i)
    {
        if (m_houseAction >= (int32)MAX_AHBOT_HOUSE_TYPE - 1)
            m_houseAction = -1;
        Update();
    }
}

void AuctionHouseBot::PrepareStatusInfos(AuctionHouseBotStatusInfo& statusInfo) const
{
    for (uint32 i = 0; i < MAX_AHBOT_HOUSE_TYPE; ++i)
    {
        statusInfo[i].ItemsCount = 0;

        for (unsigned int& j : statusInfo[i].QualityInfo)
            j = 0;

        AuctionHouseEntry const* ahEntry = GetAHEntryForType(i);
        if (!ahEntry)
            continue;
        AuctionHouseObject* ah = sAuctionMgr.GetAuctionsMap(ahEntry);
        if (!ah)
            continue;
        AuctionHouseObject::AuctionEntryMap const& auctions = *ah->GetAuctions();
        for (AuctionHouseObject::AuctionEntryMap::const_iterator itr = auctions.begin(); itr != auctions.end(); ++itr)
        {
            AuctionEntry* entry = itr->second;
            if (Item* item = sAuctionMgr.GetAItem(entry->itemGuidLow))
            {
                ItemPrototype const* prototype = item->GetProto();
                if (!entry->owner)
                {
                    if (prototype->Quality < MAX_ITEM_QUALITY)
                        ++statusInfo[i].QualityInfo[prototype->Quality];

                    ++statusInfo[i].ItemsCount;
                }
            }
        }
    }
}

void AuctionHouseBot::SetItemData(uint32 item, AuctionHouseBotItemData& itemData, bool reset)
{
    static SqlStatementID delItem;
    SqlStatement stmt = CharacterDatabase.CreateStatement(delItem, "DELETE FROM ahbot_items WHERE item = ?");
    stmt.PExecute(item);

    if (reset)
    {
        m_itemData.erase(item);
        return;
    }
    ItemPrototype const* prototype = sObjectMgr.GetItemPrototype(item);
    if (!prototype)
        return;

    if (itemData.AddChance > 100)
        itemData.AddChance = 100;

    if (itemData.MinAmount == 0)
        itemData.MinAmount = prototype->GetMaxStackSize();
    if (itemData.MaxAmount == 0)
        itemData.MaxAmount = prototype->GetMaxStackSize();
    if (itemData.MaxAmount < itemData.MinAmount)
        itemData.MaxAmount = itemData.MinAmount;

    m_itemData[item] = itemData;

    static SqlStatementID addItem;
    stmt = CharacterDatabase.CreateStatement(addItem, "INSERT INTO ahbot_items (item, value, add_chance, min_amount, max_amount) VALUES (?, ?, ?, ?, ?)");
    stmt.addUInt32(item);
    stmt.addUInt32(itemData.Value);
    stmt.addUInt32(itemData.AddChance);
    stmt.addUInt32(itemData.MinAmount);
    stmt.addUInt32(itemData.MaxAmount);
    stmt.Execute();
}

AuctionHouseBotItemData AuctionHouseBot::GetItemData(uint32 item)
{
    auto iterator = m_itemData.find(item);
    if (iterator != m_itemData.end())
        return iterator->second;

    AuctionHouseBotItemData itemData;
    ItemPrototype const* prototype = sObjectMgr.GetItemPrototype(item);
    itemData.Value = prototype ? CalculateBuyoutPrice(prototype) : 0;
    return itemData;
}

uint32 AuctionHouseBot::GetMinMaxConfig(const char* config, uint32 minValue, uint32 maxValue, uint32 defaultValue)
{
    uint32 field = (uint32)m_ahBotCfg.GetIntDefault(config, (int32)defaultValue);
    if (field < minValue)
    {
        sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "AHBot error: %s must be between %u and %u. Setting value to %u.", config, minValue, maxValue, defaultValue);
        field = defaultValue;
    }
    else if (field > maxValue)
    {
        sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "AHBot error: %s must be between %u and %u. Setting value to %u.", config, minValue, maxValue, defaultValue);
        field = defaultValue;
    }
    return field;
}

void AuctionHouseBot::ParseLootConfig(char const* fieldname, std::vector<int32>& lootConfig)
{
    std::stringstream includeStream(m_ahBotCfg.GetStringDefault(fieldname, ""));
    std::string temp;
    lootConfig.clear();
    while (getline(includeStream, temp, ','))
        lootConfig.push_back(atoi(temp.c_str()));

    while (lootConfig.size() < 4)
        lootConfig.push_back(0);

    for (uint32 index = 1; index < lootConfig.size(); ++index)
    {
        if (lootConfig[index] < 0)
        {
            sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "AHBot error: %s value (%d) for field %s should not be a negative number, setting value to 0.",
                (index == 1 ? "Second" : (index == 2 ? "Third" : "Fourth")), lootConfig[index], fieldname);
            lootConfig[index] = 0;
        }
    }
    if (lootConfig[0] > lootConfig[1])
    {
        sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "AHBot error: First value (%d) must be less than or equal to second value (%d) for field %s. Setting first value to second value.", lootConfig[0], lootConfig[1], fieldname);
        lootConfig[0] = lootConfig[1];
    }
    if (lootConfig[2] > lootConfig[3])
    {
        sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "AHBot error: Third value (%d) must be less than or equal to fourth value (%d) for field %s. Setting third value to fourth value.", lootConfig[2], lootConfig[3], fieldname);
        lootConfig[2] = lootConfig[3];
    }
}

void AuctionHouseBot::FillUintVectorFromQuery(char const* query, std::vector<uint32>& lootTemplates)
{
    lootTemplates.clear();

    uint32 start = WorldTimer::getMSTime();

    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "AHBot: Starting query: %s", query);

    if (auto queryResult = WorldDatabase.PQuery("%s", query))
    {
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "AHBot: Query finished in %u ms, %u rows", WorldTimer::getMSTimeDiff(start, WorldTimer::getMSTime()), queryResult->GetRowCount());

        BarGoLink bar(queryResult->GetRowCount());

        do
        {
            bar.step();

            Field* fields = queryResult->Fetch();
            uint32 entry = fields[0].GetUInt32();

            if (!entry)
                continue;

            lootTemplates.push_back(entry);
        }
        while (queryResult->NextRow());
    }
}

void AuctionHouseBot::ParseItemValueConfig(char const* fieldname, std::vector<uint32>& itemValues)
{
    std::stringstream includeStream(m_ahBotCfg.GetStringDefault(fieldname, ""));
    std::string temp;
    for (uint32 index = 0; getline(includeStream, temp, ','); ++index)
    {
        if (index < itemValues.size())
            itemValues[index] = atoi(temp.c_str());
    }
}

void AuctionHouseBot::AddLootToItemMap(LootStore* store, std::vector<int32>& lootConfig, std::vector<uint32>& lootTemplates, std::unordered_map<uint32, uint32>& itemMap)
{
    if (lootConfig.size() < 4 || lootConfig[1] <= 0 || lootConfig[3] <= 0 || lootTemplates.size() <= 0)
        return;
    int32 maxTemplates = lootConfig[0] < 0 ? urand(0, lootConfig[1] - lootConfig[0]) + lootConfig[0] : urand(lootConfig[0], lootConfig[1]);
    if (maxTemplates <= 0)
        return;
    for (int32 templateCounter = 0; templateCounter < maxTemplates; ++templateCounter)
    {
        uint32 lootTemplate = urand(0, lootTemplates.size() - 1);
        LootTemplate const* lootTable = store->GetLootFor(lootTemplates[lootTemplate]);
        if (!lootTable)
            continue;
        // VMaNGOS Loot ctor takes WorldObject const* - use nullptr for synthetic loot generation
        Loot loot(nullptr);
        for (uint32 repeat = urand(lootConfig[2], lootConfig[3]); repeat > 0; --repeat)
            lootTable->Process(loot, *store, nullptr, store->IsRatesAllowed());

        // VMaNGOS: iterate the public items vector directly
        for (auto const& lootItem : loot.items)
            itemMap[lootItem.itemid] += lootItem.count;
    }
}

uint32 AuctionHouseBot::CalculateBuyoutPrice(ItemPrototype const* prototype)
{
    uint32 buyoutPrice = prototype->BuyPrice;
    if (buyoutPrice == 0 || (prototype->SellPrice > 0 && buyoutPrice / prototype->SellPrice > 5))
        buyoutPrice = prototype->SellPrice * (prototype->Quality <= ITEM_QUALITY_NORMAL ? 4 : 5);
    buyoutPrice *= (m_vendorValue && m_vendorItems.find(prototype->ItemId) != m_vendorItems.end() ? 100 : m_itemValue[prototype->Quality][prototype->Class]);
    buyoutPrice /= 100;
    return buyoutPrice;
}
