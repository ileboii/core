#include "playerbot/strategy/Action.h"
#include "ChooseTargetActions.h"
#include "MovementGenerator.h"
#include "CreatureAI.h"
#include "playerbot/TravelMgr.h"
#include "playerbot/strategy/generic/PullStrategy.h"
#include "playerbot/strategy/values/FreeMoveValues.h"
#include "BattleGroundAV.h"

bool DpsAssistAction::isUseful()
{
    // if carry flag, do not start fight
    if (bot->HasAura(23333) || bot->HasAura(23335) || bot->HasAura(34976))
        return false;

    return true;
}

bool AttackAnythingAction::isUseful()
{
    if (bot->InBattleGround() && bot->GetBattleGroundTypeId() == BATTLEGROUND_AV && !ai->IsAvQuester())
    {
        return false;
    }

    if (!ai->AllowActivity(GRIND_ACTIVITY))
        return false;

    if (!AI_VALUE(bool, "can move around"))
        return false;

    Unit* target = GetTarget();

    if (!target || !ai->IsSafe(target))
        return false;

    if (ai->ContainsStrategy(STRATEGY_TYPE_HEAL) && !ai->HasStrategy("offdps", BotState::BOT_STATE_COMBAT))
        return false;

    if (!target->IsPlayer() && (bot->IsWithinDist(target, target->GetMeleeReach() * 1.5f) && bot->HasInArc(target, M_PI_F * 0.5f)) && target->IsHostileTo(bot) && target->GetLevel() < bot->GetLevel() + 3.0)
        return true;

    if (AI_VALUE(bool, "travel target traveling") && CanFreeMoveValue::CanFreeMoveTo(ai, *AI_VALUE(TravelTarget*, "travel target")->GetPosition()))
        return false;

    return true;
}

bool ai::AttackAnythingAction::isPossible()
{
    return AttackAction::isPossible() && GetTarget();
}

bool ai::AttackAnythingAction::Execute(Event& event)
{
    bool result = AttackAction::Execute(event);
    if (result)
    {
        Unit* grindTarget = GetTarget();
        if (grindTarget)
        {
            std::string grindName = grindTarget->GetName();
            if (!grindName.empty())
            {
                sPlayerbotAIConfig.logEvent(ai, "AttackAnythingAction", grindName, std::to_string(grindTarget->GetEntry()));

                if (ai->HasStrategy("pull", BotState::BOT_STATE_COMBAT))
                {
                    if (PullStrategy* strategy = PullStrategy::Get(ai))
                    {
                        if (strategy->CanDoPullAction(grindTarget) && (ai->GetBot()->GetClass() == CLASS_DRUID || ai->GetBot()->GetClass() == CLASS_PALADIN || AI_VALUE2(uint32, "item count", "ammo")))
                        {
                            Event pullEvent("attack anything", grindTarget->GetObjectGuid());
                            bool doAction = ai->DoSpecificAction("pull my target", pullEvent, true);

                            if (doAction)
                            {
                                return true;
                            }
                        }
                    }
                }

                context->GetValue<ObjectGuid>("attack target")->Set(grindTarget->GetObjectGuid());
                ai->StopMoving();
            }
        }
    }

    return result;
}

bool AttackEnemyPlayerAction::isUseful()
{
    return !sPlayerbotAIConfig.IsInPvpProhibitedZone(sServerFacade.GetAreaId(bot));
}

bool AttackEnemyFlagCarrierAction::isUseful()
{
    Unit* target = context->GetValue<Unit*>("enemy flag carrier")->Get();
    return target && sServerFacade.IsDistanceLessOrEqualThan(sServerFacade.GetDistance2d(bot, target), 75.0f) && (bot->HasAura(23333) || bot->HasAura(23335) || bot->HasAura(34976));
}

namespace
{
    bool HasTargetForAction(PlayerbotAI* ai, const char* name)
    {
        Action* action = ai->GetAiObjectContext()->GetAction(name);
        if (!action || !action->isUseful() || !action->isPossible())
            return false;

        Player* bot = ai->GetBot();
        Unit* target = action->GetTarget();
        if (!ai->IsSafe(target) || !target->IsTargetableBy(bot) ||
            sServerFacade.IsFriendlyTo(bot, target) ||
            !bot->IsWithinDistInMap(target, sPlayerbotAIConfig.sightDistance))
            return false;

        Creature* creature = target->ToCreature();
        return !creature || !creature->IsInEvadeMode();
    }
}

const char* SelectNewTargetAction::GetAttackActionName(PlayerbotAI* ai)
{
    AiObjectContext* context = ai->GetAiObjectContext();
    if (!AI_VALUE(bool, "has attackers"))
        return nullptr;

    if ((ai->HasStrategy("pvp", BotState::BOT_STATE_COMBAT) || ai->HasStrategy("duel", BotState::BOT_STATE_COMBAT)) &&
        AI_VALUE(bool, "has enemy player targets") && HasTargetForAction(ai, "attack enemy player"))
        return "attack enemy player";

    if (ai->HasStrategy("dps assist", BotState::BOT_STATE_NON_COMBAT) && HasTargetForAction(ai, "dps assist"))
        return "dps assist";

    if (ai->HasStrategy("tank assist", BotState::BOT_STATE_NON_COMBAT) && HasTargetForAction(ai, "tank assist"))
        return "tank assist";

    return nullptr;
}

void SelectNewTargetAction::ClearCurrentTarget(PlayerbotAI* ai)
{
    AiObjectContext* context = ai->GetAiObjectContext();
    Player* bot = ai->GetBot();

    if (ObjectGuid attackTarget = AI_VALUE(ObjectGuid, "attack target"))
    {
        const auto possible = AI_VALUE(std::list<ObjectGuid>, "possible targets no los");
        if (std::find(possible.begin(), possible.end(), attackTarget) == possible.end())
            SET_AI_VALUE(ObjectGuid, "attack target", ObjectGuid());
    }

    if (Unit* target = AI_VALUE(Unit*, "current target"))
    {
        if (sServerFacade.UnitIsDead(target))
            AI_VALUE(LootObjectStack*, "available loot")->Add(target->GetObjectGuid());

        SET_AI_VALUE(Unit*, "old target", target);
    }

    SET_AI_VALUE(Unit*, "current target", nullptr);
    bot->SetSelectionGuid(ObjectGuid());
    bot->AttackStop();
}

bool SelectNewTargetAction::isUseful()
{
    Pet* pet = bot->GetPet();
    return bot->GetSelectionGuid() || bot->GetVictim() || AI_VALUE(Unit*, "current target") ||
        (pet && pet->GetVictim()) || GetAttackActionName(ai);
}

bool SelectNewTargetAction::Execute(Event& event)
{
    if (bot->GetSelectionGuid() || bot->GetVictim() || AI_VALUE(Unit*, "current target"))
        ai->InterruptSpell();
    ClearCurrentTarget(ai);

    if (const char* action = GetAttackActionName(ai); action && ai->DoSpecificAction(action, event, true))
        return true;

    if (Pet* pet = bot->GetPet(); pet && pet->GetVictim() && pet->AI())
    {
        const uint32 command = (uint32(ACT_COMMAND) << 24) | COMMAND_FOLLOW;
        WorldPacket data(CMSG_PET_ACTION);
        data << pet->GetObjectGuid() << command << ObjectGuid();
        bot->GetSession()->HandlePetAction(MakeTypedPacket<WorldPackets::Pet::PetAction>(data));
    }

    return false;
}
