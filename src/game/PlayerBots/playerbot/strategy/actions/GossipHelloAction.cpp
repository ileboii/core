
#include "playerbot/playerbot.h"
#include "GossipHelloAction.h"

#include "playerbot/ServerFacade.h"
#include "ScriptMgr.h"


using namespace ai;

bool GossipHelloAction::Execute(Event& event)
{
	ObjectGuid guid;

    Player* requester = event.getOwner() ? event.getOwner() : GetMaster();
	WorldPacket &p = event.getPacket();
	if (p.empty())
	{
		if (requester)
			guid = requester->GetSelectionGuid();
	}
	else
	{
		p.rpos(0);
		p >> guid;
	}

	if (!guid)
		return false;

	Creature *pCreature = bot->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_NONE);
	if (!pCreature)
	{
		sLog.Out(LOG_BASIC, LOG_LVL_DEBUG, "[PlayerbotMgr]: HandleMasterIncomingPacket - Received  CMSG_GOSSIP_HELLO %s not found or you can't interact with him.", guid.GetString().c_str());
		return false;
	}

	GossipMenuItemsMapBounds pMenuItemBounds = sObjectMgr.GetGossipMenuItemsMapBounds(pCreature->GetDefaultGossipMenuId());
	if (pMenuItemBounds.first == pMenuItemBounds.second)
		return false;

    std::string text = event.getParam();
	int menuToSelect = -1;
    if (event.getSource().find("rpg action") == 0)
    {
        Creature* pCreature = bot->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_NONE);

        if (pCreature)
        {
            if (!sScriptMgr.OnGossipHello(bot, pCreature))
            {
                bot->PrepareGossipMenu(pCreature, pCreature->GetDefaultGossipMenuId());
            }
        }

        ProcessGossip(requester, guid, -1);
    }
	else if (text.empty())
	{
        WorldPacket p1;
        p1 << guid;
        bot->GetSession()->HandleGossipHelloOpcode(MakeTypedPacket<WorldPackets::Npc::GossipHello>(p1));
        sServerFacade.SetFacingTo(bot, pCreature);

        std::ostringstream out; out << "--- " << pCreature->GetName() << " ---";
        ai->TellPlayerNoFacing(requester, out.str());

        TellGossipMenus(requester);
	}
	else if (!bot->PlayerTalkClass)
	{
	    ai->TellPlayerNoFacing(requester, "I need to talk first");
	    return false;
	}
	else
	{
	    menuToSelect = atoi(text.c_str());
	    if (menuToSelect > 0) menuToSelect--;
        ProcessGossip(requester, guid, menuToSelect);
	}

	bot->TalkedToCreature(pCreature->GetEntry(), pCreature->GetObjectGuid());
	return true;
}

void GossipHelloAction::TellGossipText(Player* requester, uint32 textId)
{
    if (!textId)
        return;

    NpcText const* text = sObjectMgr.GetNpcText(textId);
    if (text)
    {
        for (int i = 0; i < 8; i++)
        {
            std::string text0 = "" /* Text_0 not in vmangos NpcTextOption */;
            if (!text0.empty()) ai->TellPlayerNoFacing(requester, text0);
            std::string text1 = "" /* Text_1 not in vmangos NpcTextOption */;
            if (!text1.empty()) ai->TellPlayerNoFacing(requester, text1);
        }
    }
}

void GossipHelloAction::TellGossipMenus(Player* requester)
{
    if (!bot->PlayerTalkClass)
        return;

     GossipMenu& menu = bot->PlayerTalkClass->GetGossipMenu();

     if (requester)
     {
         Creature* pCreature = bot->GetNPCIfCanInteractWith(requester->GetSelectionGuid(), UNIT_NPC_FLAG_NONE);

         if (pCreature)
         {
             uint32 textId = bot->GetGossipTextId(menu.GetMenuId(), pCreature);
             TellGossipText(requester, textId);
         }
     }

    for (unsigned int i = 0; i < menu.MenuItemCount(); i++)
    {
        GossipMenuItem const& item = menu.GetItem(i);
        std::ostringstream out; out << "[" << (i+1) << "] " << item.m_gMessage;
        ai->TellPlayerNoFacing(requester, out.str());
    }
}

bool GossipHelloAction::ProcessGossip(Player* requester, ObjectGuid creatureGuid, int menuToSelect)
{
    GossipMenu& menu = bot->PlayerTalkClass->GetGossipMenu();

    bool noFeedback = (menuToSelect == -1);

    int actualMenuToSelect = menuToSelect;

    if (actualMenuToSelect == -1)
    {
        actualMenuToSelect = urand(0, menu.MenuItemCount() - 1);
    }

    if (actualMenuToSelect >= 0 && (unsigned int)actualMenuToSelect >= menu.MenuItemCount())
    {
        ai->TellError(requester, "Unknown gossip option");
        return false;
    }
    GossipMenuItem const& item = menu.GetItem(actualMenuToSelect);
    WorldPacket p;
    std::string code;
    p << creatureGuid;
#ifdef MANGOSBOT_ZERO
    p << actualMenuToSelect;
#else
    p << menu.GetMenuId() << actualMenuToSelect;
#endif
    p << code;
    bot->GetSession()->HandleGossipSelectOptionOpcode(MakeTypedPacket<WorldPackets::Npc::GossipSelectOption>(p));

    if(!noFeedback)
        TellGossipMenus(requester);
    return true;
}
