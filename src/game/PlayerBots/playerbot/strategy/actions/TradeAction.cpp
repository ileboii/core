
#include "playerbot/playerbot.h"
#include "Objects/TradeData.h"
#include "TradeAction.h"
#include "playerbot/strategy/ItemVisitors.h"
#include "playerbot/strategy/values/ItemCountValue.h"
#include "playerbot/RandomPlayerbotMgr.h"
#include "playerbot/strategy/values/TradeValues.h"

using namespace ai;

bool TradeAction::Execute(Event& event)
{
    std::string text = event.getParam();
    IterateItemsMask mask = IterateItemsMask((uint8)IterateItemsMask::ITERATE_ITEMS_IN_EQUIP | (uint8)IterateItemsMask::ITERATE_ITEMS_IN_BAGS);
    bool isSaleCommand = event.getSource() == "t" || event.getSource() == "nt";

    if (!bot->GetTrader())
    {
        RESET_AI_VALUE2(bool, "manual bool", "player sale trade active");
        RESET_AI_VALUE2(std::string, "manual string", "pending sale item request");
        RESET_AI_VALUE2(std::string, "manual string", "pending sale buyer");

        std::list<ObjectGuid> guids = chat->parseGameobjects(text);
        Player* player = nullptr;

        for(auto& guid: guids)
        {
            if (guid.IsPlayer())
            {
                player = sObjectMgr.GetPlayer(guid);
            }
        }

        if (!player)
        {
            player = event.getOwner() ? event.getOwner() : GetMaster();
        }

        if (!player) 
        {
            return false;
        }

        if (!player->GetTrader())
        {
            std::list<Item*> requestedItems = ai->InventoryParseItems(text, mask);
            bool wantsSaleItem = isSaleCommand && sRandomPlayerbotMgr.IsRandomBot(bot) &&
                std::any_of(requestedItems.begin(), requestedItems.end(), [this](Item* item) { return ItemsForSaleValue::IsItemForSale(ai, item); });

            if (wantsSaleItem)
            {
                SET_AI_VALUE2(bool, "manual bool", "player sale trade active", true);
                SET_AI_VALUE2(std::string, "manual string", "pending sale item request", text);
                SET_AI_VALUE2(std::string, "manual string", "pending sale buyer", std::to_string(player->GetGUIDLow()));
            }

            WorldPacket packet(CMSG_INITIATE_TRADE);
            packet << player->GetObjectGuid();
            bot->GetSession()->HandleInitiateTradeOpcode(MakeTypedPacket<WorldPackets::Trade::InitiateTrade>(packet));
            return true;
        }
        else if (player->GetTrader() != bot)
        {
            return false;
        }
    }
    
    uint32 copper = chat->parseMoney(text);
    if (copper > 0)
    {
        WorldPacket packet(CMSG_SET_TRADE_GOLD, 4);
        packet << copper;
        bot->GetSession()->HandleSetTradeGoldOpcode(MakeTypedPacket<WorldPackets::Trade::SetTradeGold>(packet));
    }

    size_t pos = text.rfind(" ");
    int count = pos!= std::string::npos ? atoi(text.substr(pos + 1).c_str()) : 1;

    std::list<Item*> found = ai->InventoryParseItems(text, mask);

    if (event.getSource() == "sale offer")
    {
        found.remove_if([this](Item* item) { return !ItemsForSaleValue::IsItemForSale(ai, item); });
    }

    if (found.empty())
        return false;

    if (isSaleCommand && sRandomPlayerbotMgr.IsRandomBot(bot) &&
        std::any_of(found.begin(), found.end(), [this](Item* item) { return ItemsForSaleValue::IsItemForSale(ai, item); }))
    {
        SET_AI_VALUE2(bool, "manual bool", "player sale trade active", true);
    }

    int traded = 0;
    for (std::list<Item*>::iterator i = found.begin(); i != found.end(); i++)
    {
        Item* item = *i;

        if (!bot->GetTrader() || item->IsInTrade())
            continue;

        int8 slot = item->CanBeTraded() ? -1 : 6;
        if (TradeItem(*item, slot) && slot != 6 && ++traded >= count)
            break;
    }

    if (event.getSource() == "sale offer")
        return traded > 0;

    return true;
}

bool TradeAction::TradeItem(const Item& item, int8 slot)
{
    int8 tradeSlot = -1;
    Item* itemPtr = const_cast<Item*>(&item);

    TradeData* pTrade = bot->GetTradeData();
    if ((slot >= 0 && slot < 7) && pTrade->GetItem((TradeSlots)slot) == NULL)
        tradeSlot = slot;

    if (slot == 6)
        pTrade->SetItem(TradeSlots(6), itemPtr);
    else
    {
        for (uint8 i = 0; i < 6 && tradeSlot == -1; i++)
        {
            if (nullptr /* TradeData not fully available */ == itemPtr)
            {
                tradeSlot = i;

                WorldPacket packet(CMSG_CLEAR_TRADE_ITEM, 1);
                packet << (uint8) tradeSlot;
                bot->GetSession()->HandleClearTradeItemOpcode(MakeTypedPacket<WorldPackets::Trade::ClearTradeItem>(packet));
                pTrade->SetItem((TradeSlots)i, NULL);
                return true;
            }
        }

        for (uint8 i = 0; i < 6 && tradeSlot == -1; i++)
        {
            if (nullptr /* TradeData not fully available */ == NULL)
            {
                tradeSlot = i;
            }
        }
    }

    if (tradeSlot == -1) return false;

    WorldPacket packet(CMSG_SET_TRADE_ITEM, 3);
    packet << (uint8) tradeSlot << (uint8) item.GetBagSlot()
        << (uint8) item.GetSlot();
    bot->GetSession()->HandleSetTradeItemOpcode(MakeTypedPacket<WorldPackets::Trade::SetTradeItem>(packet));
    return true;
}

