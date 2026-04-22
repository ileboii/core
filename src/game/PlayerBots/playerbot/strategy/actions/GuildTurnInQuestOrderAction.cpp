
#include "playerbot/playerbot.h"
#include "GuildTurnInQuestOrderAction.h"
#include "playerbot/ServerFacade.h"

using namespace ai;

bool GuildTurnInQuestOrderAction::isUseful()
{
    if (!bot->GetGuildId())
        return false;

    if (bot->IsInCombat())
        return false;

    return AI_VALUE(bool, "needs guild quest order turn in");
}

static uint32 PickShareRewardChoice(Quest const* quest, uint32 preferredItemId)
{
    uint32 choices = quest->GetRewChoiceItemsCount();
    if (!choices)
        return 0;

    if (preferredItemId)
    {
        for (uint32 i = 0; i < choices; ++i)
        {
            if (quest->RewChoiceItemId[i] == preferredItemId)
                return i;
        }
    }

    return 0;
}

bool GuildTurnInQuestOrderAction::Execute(Event& /*event*/)
{
    GuildOrder order = AI_VALUE(GuildOrder, "guild order");
    if (!order.IsQuestRewardOrder() || !order.questId)
        return false;

    Quest const* quest = sObjectMgr.GetQuestTemplate(order.questId);
    if (!quest)
        return false;

    if (bot->GetQuestStatus(order.questId) != QUEST_STATUS_COMPLETE)
        return false;

    if (!bot->CanRewardQuest(quest, false))
        return false;

    uint32 rewardIndex = PickShareRewardChoice(quest, order.rewardItemId);

    // Search nearby NPCs for one that takes this quest.
    std::list<ObjectGuid> npcs = AI_VALUE(std::list<ObjectGuid>, "nearest npcs");
    for (auto& guid : npcs)
    {
        Unit* unit = ai->GetUnit(guid);
        if (!unit || bot->GetDistance(unit) > INTERACTION_DISTANCE)
            continue;

        Creature* creature = unit->ToCreature();
        if (!creature || !creature->HasInvolvedQuest(order.questId))
            continue;

        if (!bot->CanRewardQuest(quest, rewardIndex, false))
            return false;

        if (!sServerFacade.IsInFront(bot, unit, sPlayerbotAIConfig.sightDistance, CAST_ANGLE_IN_FRONT))
            sServerFacade.SetFacingTo(bot, unit);

        bot->RewardQuest(quest, rewardIndex, static_cast<WorldObject*>(creature), true);

        if (bot->GetQuestRewardStatus(order.questId))
        {
            ai->TellDebug(ai->GetMaster(), "Guild quest order: turned in quest " + order.target, "debug travel");

            if (sPlayerbotAIConfig.globalSoundEffects)
                bot->PlayDistanceSound(621);

            return true;
        }
    }

    // Also check nearby game objects (some quests are turned in at objects).
    std::list<ObjectGuid> gos = AI_VALUE(std::list<ObjectGuid>, "nearest game objects no los");
    for (auto& guid : gos)
    {
        GameObject* go = ai->GetGameObject(guid);
        if (!go || bot->GetDistance(go) > INTERACTION_DISTANCE)
            continue;

        if (!go->HasInvolvedQuest(order.questId))
            continue;

        if (!bot->CanRewardQuest(quest, rewardIndex, false))
            return false;

        bot->RewardQuest(quest, rewardIndex, static_cast<WorldObject*>(go), true);

        if (bot->GetQuestRewardStatus(order.questId))
        {
            ai->TellDebug(ai->GetMaster(), "Guild quest order: turned in quest " + order.target, "debug travel");

            if (sPlayerbotAIConfig.globalSoundEffects)
                bot->PlayDistanceSound(621);

            return true;
        }
    }

    return false;
}
