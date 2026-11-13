
#include "playerbot/playerbot.h"
#include "GuildTurnInQuestOrderAction.h"
#include "playerbot/ServerFacade.h"

using namespace ai;

bool GuildTurnInQuestOrderAction::isUseful()
{
    if (!GuildOrdersAllowed(ai))
        return false;

    if (bot->IsInCombat())
        return false;

    return AI_VALUE(bool, "needs guild quest order turn in");
}

bool GuildTurnInQuestOrderAction::Execute(Event& /*event*/)
{
    if (!GuildOrdersAllowed(ai))
        return false;

    GuildOrder order = AI_VALUE(GuildOrder, "guild order");
    if (!order.IsQuestRewardOrder() || !order.NeedsMoreOfReward(bot))
        return false;

    Quest const* quest = sObjectMgr.GetQuestTemplate(order.questId);
    if (!HasGuildQuestItems(bot, quest))
        return false;

    uint32 const rewardIndex = order.rewardChoiceIndex >= 0 ? uint32(order.rewardChoiceIndex) : 0;
    auto turnIn = [&](WorldObject* questGiver)
    {
        if (!questGiver || !questGiver->HasInvolvedQuest(order.questId) || !bot->CanInteractWithQuestGiver(questGiver))
            return false;

        if (CanCompleteGuildQuest(bot, quest))
            bot->CompleteQuest(order.questId);

        if (!bot->CanRewardQuest(quest, rewardIndex, false))
            return false;

        if (Unit* unit = questGiver->ToUnit())
            if (!sServerFacade.IsInFront(bot, unit, sPlayerbotAIConfig.sightDistance, CAST_ANGLE_IN_FRONT))
                sServerFacade.SetFacingTo(bot, unit);

        bot->RewardQuest(quest, rewardIndex, questGiver, true);
        ai->TellDebug(ai->GetMaster(), "Guild quest order: turned in quest " + order.target, "debug travel");
        if (sPlayerbotAIConfig.globalSoundEffects)
            bot->PlayDistanceSound(621);

        ResetGuildQuestOrders(ai);
        return true;
    };

    // Search nearby NPCs for one that takes this quest.
    for (auto const& guid : AI_VALUE(std::list<ObjectGuid>, "nearest npcs"))
        if (turnIn(ai->GetUnit(guid)))
            return true;

    // Also check nearby game objects (some quests are turned in at objects).
    for (auto const& guid : AI_VALUE(std::list<ObjectGuid>, "nearest game objects no los"))
        if (turnIn(ai->GetGameObject(guid)))
            return true;

    return false;
}
