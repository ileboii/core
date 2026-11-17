
#include "playerbot/playerbot.h"
#include "playerbot/ServerFacade.h"
#include "playerbot/strategy/actions/ChooseTargetActions.h"
#include "BotStateTriggers.h"

using namespace ai;

bool CombatStartTrigger::IsActive()
{
    if (!ai->IsStateActive(BotState::BOT_STATE_COMBAT) && !ai->IsStateActive(BotState::BOT_STATE_DEAD))
    {
        return bot->IsInCombat() || SelectNewTargetAction::GetAttackActionName(ai);
    }

    return false;
}

bool CombatEndTrigger::IsActive()
{
    if (!ai->IsStateActive(BotState::BOT_STATE_COMBAT) || bot->IsInCombat() || AI_VALUE(Unit*, "pull target"))
        return false;

    if (bot->GetVictim() && !AI_VALUE2(bool, "invalid target", "current target"))
        return false;

    // A nearby attacker can still be out of range or unavailable to assist against.
    return !SelectNewTargetAction::GetAttackActionName(ai);
}

bool DeathTrigger::IsActive()
{
    return !ai->IsStateActive(BotState::BOT_STATE_DEAD) && !sServerFacade.IsAlive(bot);
}

bool ResurrectTrigger::IsActive()
{
    return ai->IsStateActive(BotState::BOT_STATE_DEAD) && sServerFacade.IsAlive(bot);
}
