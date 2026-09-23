#pragma once
#include "playerbot/strategy/Trigger.h"
#include "playerbot/PlayerbotAIConfig.h"
#include "playerbot/ServerFacade.h"
#include "playerbot/strategy/generic/CombatStrategy.h"
#include "playerbot/strategy/values/PossibleAttackTargetsValue.h"
#include "playerbot/strategy/values/Formations.h"
#include "playerbot/strategy/values/Stances.h"
#include "playerbot/strategy/generic/KiteStrategy.h"

namespace ai
{
    class EnemyTooCloseForSpellTrigger : public Trigger 
    {
    public:
        EnemyTooCloseForSpellTrigger(PlayerbotAI* ai) : Trigger(ai, "enemy too close for spell") {}
        
        virtual bool IsActive() override
        {
            Unit* target = AI_VALUE(Unit*, "current target");
            if (target)
            {
                if (ai->HasStrategy("follow", BotState::BOT_STATE_COMBAT) ||
                    ai->HasStrategy("guard", BotState::BOT_STATE_COMBAT) ||
                    ai->HasStrategy("wander", BotState::BOT_STATE_COMBAT))
                    if(bot->GetClass() != CLASS_HUNTER || sServerFacade.GetDistance2d(bot, target) > 5.0f)
                        return false;                   

                const bool canMove = !PossibleAttackTargetsValue::HasBreakableCC(target, bot) && !PossibleAttackTargetsValue::HasUnBreakableCC(target, bot);

                if (target->GetTargetGuid() == bot->GetObjectGuid() && canMove &&
                    (!target->IsPlayer() || target->GetSpeed(MOVE_RUN) > (bot->GetSpeed(MOVE_RUN) * 0.65f)))
                {
                    return false;
                }

                float const combatReach = bot->GetCombatReach() + target->GetCombatReach();
                float const minDistance = ai->GetRange("spell") + combatReach;
                float const targetDistance = sServerFacade.GetDistance2d(bot, target) + combatReach;

                // No need to move if the target is rooted and you can shoot
                if (!canMove && (targetDistance > minDistance))
                {
                    return false;
                }

                bool isBoss = false;
                bool isRaid = false;
                bool isVictim = target->GetVictim() && target->GetVictim()->GetObjectGuid() == bot->GetObjectGuid();

                if (target->IsCreature())
                {
                    Creature* creature = ai->GetCreature(target->GetObjectGuid());
                    if (creature)
                    {
                        isBoss = creature->IsWorldBoss();
                    }
                }

                if (bot->GetMap()->IsRaid())
                    isRaid = true;

                //if (isBoss || isRaid)
                //    return sServerFacade.IsDistanceLessThan(targetDistance, (ai->GetRange("spell") + combatReach) / 2);

                float coeff = 0.5f;
                if (target->IsPlayer())
                {
                    if (!isVictim)
                        coeff = 0.4f;
                    else
                        coeff = 0.6f;
                }
                else
                {
                    if (!isVictim)
                        coeff = 0.4f;
                    else
                        coeff = 0.6f;
                }

                if (isRaid)
                    coeff = 0.7f;

                return sServerFacade.IsDistanceLessOrEqualThan(targetDistance, minDistance * coeff);
            }
            return false;
        }
    };

    class EnemyTooCloseForShootTrigger : public Trigger 
    {
    public:
        EnemyTooCloseForShootTrigger(PlayerbotAI* ai) : Trigger(ai, "enemy too close for shoot") {}
        
        virtual bool IsActive() override
        {
            Unit* target = AI_VALUE(Unit*, "current target");
            if (target)
            {
                // Don't move if the target is targeting you and you can't add distance between you and the target
                if (target->GetTargetGuid() == bot->GetObjectGuid() && !target->IsRooted() && target->GetSpeed(MOVE_RUN) > (bot->GetSpeed(MOVE_RUN) * 0.65))
                {
                    return false;
                }

                float const combatReach = bot->GetCombatReach() + target->GetCombatReach();
                float const minShootDistance = ai->GetRange("shoot") + combatReach;
                float const targetDistance = sServerFacade.GetDistance2d(bot, target) + combatReach;

                // No need to move if the target is rooted and you can shoot
                if (target->IsRooted() && (targetDistance > minShootDistance))
                {
                    return false;
                }

                bool isBoss = false;
                bool isRaid = false;
                bool isVictim = target->GetVictim() && target->GetVictim()->GetObjectGuid() == bot->GetObjectGuid();

                if (target->IsCreature())
                {
                    Creature* creature = ai->GetCreature(target->GetObjectGuid());
                    if (creature)
                    {
                        isBoss = creature->IsWorldBoss();
                    }
                }

                if (bot->GetMap()->IsRaid())
                    isRaid = true;

                //if (isBoss || isRaid)
                //    return sServerFacade.IsDistanceLessThan(targetDistance, (ai->GetRange("spell") + combatReach));

                float coeff = 0.5f;
                if (target->IsPlayer())
                {
                    if (!isVictim)
                        coeff = 0.7f;
                    else
                        coeff = 1.0f;
                }
                else
                {
                    if (!isVictim)
                        coeff = 0.4f;
                    else
                        coeff = 0.6f;
                }

                if (isRaid)
                    coeff = 1.0f;

                return sServerFacade.IsDistanceLessOrEqualThan(targetDistance, minShootDistance * coeff);
            }

            return false;
        }
    };

    class EnemyTooCloseForMeleeTrigger : public Trigger 
    {
    public:
        EnemyTooCloseForMeleeTrigger(PlayerbotAI* ai) : Trigger(ai, "enemy too close for melee", 3) {}
        
        virtual bool IsActive() override
        {
            Unit* target = AI_VALUE(Unit*, "current target");
            if (target && target->IsPlayer())
                return false;

            return target && AI_VALUE2(bool, "inside target", "current target");
        }
    };

    class EnemyInRangeTrigger : public Trigger 
    {
    public:
        EnemyInRangeTrigger(PlayerbotAI* ai, std::string name, float distance, bool enemyMustBePlayer = false, bool enemyTargetsBot = false)
        : Trigger(ai, name)
        , distance(distance)
        , enemyMustBePlayer(enemyMustBePlayer)
        , enemyTargetsBot(enemyTargetsBot) {}
        
        virtual bool IsActive() override
        {
            Unit* target = AI_VALUE(Unit*, "current target");
            if (target)
            {
                if (enemyMustBePlayer && !target->IsPlayer())
                {
                    return false;
                }

                if (enemyTargetsBot && target->GetTargetGuid() != bot->GetObjectGuid())
                {
                    return false;
                }

                return sServerFacade.IsDistanceLessOrEqualThan(AI_VALUE2(float, "distance", "current target"), distance);
            }

            return false;
        }

    protected:
        float distance;
        bool enemyMustBePlayer;
        bool enemyTargetsBot;
    };

    class EnemyIsCloseTrigger : public EnemyInRangeTrigger
    {
    public:
        EnemyIsCloseTrigger(PlayerbotAI* ai) : EnemyInRangeTrigger(ai, "enemy is close", sPlayerbotAIConfig.tooCloseDistance) {}
    };

    class OutOfRangeTrigger : public Trigger 
    {
    public:
        OutOfRangeTrigger(PlayerbotAI* ai, std::string name, float distance) : Trigger(ai, name)
        {
            this->distance = distance;
        }

        virtual bool IsActive() override
        {
            Unit* target = AI_VALUE(Unit*, GetTargetName());
            return target &&
                sServerFacade.IsDistanceGreaterThan(AI_VALUE2(float, "distance", GetTargetName()), distance);
        }

        virtual std::string GetTargetName() override { return "current target"; }

    protected:
        float distance;
    };

    class KitePositionTrigger : public Trigger
    {
    public:
        KitePositionTrigger(PlayerbotAI* ai) : Trigger(ai, "kite position", 1) {}

        bool IsActive() override
        {
            if (!ai->IsStateActive(BotState::BOT_STATE_COMBAT))
                return false;

            if (!ai->HasStrategy("kite", BotState::BOT_STATE_COMBAT))
                return false;

            const std::list<Unit*> hostiles = KiteStrategy::GetNearbyHostiles(ai);

            for (Unit* hostile : hostiles)
            {
                if (!IsValidHostile(hostile))
                    continue;

                if (sServerFacade.GetDistance2d(bot, hostile) <= KiteStrategy::GetMinDistance())
                {
                    return true;
                }
            }

            Unit* target = AI_VALUE(Unit*, "current target");

            if (!IsValidHostile(target))
                return false;

            if (!target->IsWithinLOSInMap(bot))
                return true;

            const float targetDistance = sServerFacade.GetDistance2d(bot, target);

            const time_t combatStart = ai->GetAiObjectContext()->GetValue<time_t>("combat start time")->Get();

            if (!combatStart)
                return false;

            const time_t settledCombatStart = ai->GetAiObjectContext()->GetValue<time_t>("manual time", "kite settled combat start")->Get();

            if (settledCombatStart != combatStart)
            {
                if (targetDistance >= KiteStrategy::GetSettleDistance() && targetDistance <= KiteStrategy::GetMaxDistance())
                {
                    ai->GetAiObjectContext()->GetValue<time_t>("manual time", "kite settled combat start")->Set(combatStart);

                    return false;
                }

                return true;
            }

            return targetDistance > KiteStrategy::GetMaxDistance();
        }

    private:
        bool IsValidHostile(Unit* unit) const { return unit && unit->IsInWorld() && unit->IsAlive() && unit->GetMapId() == bot->GetMapId() && sServerFacade.IsHostileTo(bot, unit); }
    };

    class KiteStackPositionTrigger : public Trigger
    {
    public:
        KiteStackPositionTrigger(PlayerbotAI* ai) : Trigger(ai, "kite stack position", 1) {}

        bool IsActive() override
        {
            // Let the stack action inspect the whole cohort and decide whether it needs to reposition.
            return ai->IsStateActive(BotState::BOT_STATE_COMBAT) &&
                   ai->HasStrategy("kite stack", BotState::BOT_STATE_COMBAT);
        }
    };

    class CombatStancePositionTrigger : public Trigger
    {
    public:
        CombatStancePositionTrigger(PlayerbotAI* ai) : Trigger(ai, "combat stance position", 1) {}

        virtual bool IsActive() override
        {
            Unit* target = AI_VALUE(Unit*, "current target");
            if (!target || !target->IsInWorld())
                return false;

            if (!sServerFacade.IsHostileTo(bot, target))
                return false;

            return sServerFacade.IsDistanceGreaterThan(AI_VALUE2(float, "distance", "current target"), sPlayerbotAIConfig.targetPosRecalcDistance);
        }
    };

    class SpreadPositionTrigger : public Trigger
    {
    public:
        SpreadPositionTrigger(PlayerbotAI* ai) : Trigger(ai, "spread position", 1) {}

        bool IsActive() override
        {
            Stance* stance = AI_VALUE(Stance*, "stance");

            if (!stance || stance->getName() != "spread")
                return false;

            Unit* target = AI_VALUE(Unit*, "current target");

            if (!target || !target->IsInWorld())
                return false;

            if (!sServerFacade.IsHostileTo(bot, target))
                return false;

            WorldLocation loc = stance->GetLocation();

            if (Formation::IsNullLocation(loc) || loc.mapId == uint32(-1))
            {
                return false;
            }

            const float distanceToSpreadPosition = sServerFacade.GetDistance2d(bot, loc.x, loc.y);

            return sServerFacade.IsDistanceGreaterThan(distanceToSpreadPosition, sPlayerbotAIConfig.targetPosRecalcDistance);
        }
    };

    class EnemyOutOfMeleeTrigger : public OutOfRangeTrigger
    {
    public:
        EnemyOutOfMeleeTrigger(PlayerbotAI* ai) : OutOfRangeTrigger(ai, "enemy out of melee range", sPlayerbotAIConfig.meleeDistance) {}
        
        virtual bool IsActive() override
        {
            Unit* target = AI_VALUE(Unit*, GetTargetName());
            if (!target)
                return false;

            return !bot->CanReachWithMeleeAutoAttack(target) || !bot->IsWithinLOSInMap(target, true) || OutOfRangeTrigger::IsActive();
        }
    };

    class EnemyOutOfSpellRangeTrigger : public OutOfRangeTrigger
    {
    public:
        EnemyOutOfSpellRangeTrigger(PlayerbotAI* ai) : OutOfRangeTrigger(ai, "enemy out of spell range", ai->GetRange("spell")) {}
        
        virtual bool IsActive() override
        {
            Unit* target = AI_VALUE(Unit*, GetTargetName());
            if (!target)
                return false;

            return target && (bot->GetDistance(target) > (distance - sPlayerbotAIConfig.contactDistance)) || !bot->IsWithinLOSInMap(target, true);
        }
    };

    class PartyMemberToHealOutOfSpellRangeTrigger : public OutOfRangeTrigger
    {
    public:
        PartyMemberToHealOutOfSpellRangeTrigger(PlayerbotAI* ai) : OutOfRangeTrigger(ai, "party member to heal out of spell range", ai->GetRange("heal")) {}
        virtual std::string GetTargetName() override { return "party member to heal"; }
        
        virtual bool IsActive() override
        {
            Unit* target = AI_VALUE(Unit*, GetTargetName());
            if (!target)
                return false;

            return target && (bot->GetDistance(target) > (distance - sPlayerbotAIConfig.contactDistance)) || !bot->IsWithinLOSInMap(target, true);
        }
    };

    class FarFromMasterTrigger : public Trigger 
    {
    public:
        FarFromMasterTrigger(PlayerbotAI* ai, std::string name = "far from master", float distance = 12.0f, int checkInterval = 50) : Trigger(ai, name, checkInterval), distance(distance) {}

        virtual bool IsActive() override
        {
            Unit* master = AI_VALUE(Unit*, "master target");
            if (master && sServerFacade.IsFriendlyTo(bot, master))
            {
                if (master->GetTransport() && master->GetTransport() == bot->GetTransport())
                    return false;

                return sServerFacade.IsDistanceGreaterThan(AI_VALUE2(float, "distance", "master target"), distance);
            }

            return false;
        }

    private:
        float distance;
    };

    class OutOfReactRangeTrigger : public FarFromMasterTrigger
    {
    public:
        OutOfReactRangeTrigger(PlayerbotAI* ai, std::string name = "out of react range", float distance = sPlayerbotAIConfig.reactDistance, int checkInterval = 2) : FarFromMasterTrigger(ai, name, distance, checkInterval) {}
    };

    class NotNearMasterTrigger : public OutOfReactRangeTrigger
    {
    public:
        NotNearMasterTrigger(PlayerbotAI* ai, std::string name = "not near master", int checkInterval = 2) : OutOfReactRangeTrigger(ai, name, 5.0f, checkInterval) {}

        virtual bool IsActive() override
        {
            return FarFromMasterTrigger::IsActive() && !sServerFacade.IsDistanceGreaterThan(AI_VALUE2(float, "distance", "master target"), sPlayerbotAIConfig.reactDistance);
        }
    };

    class UpdateFollowTrigger : public NotNearMasterTrigger
    {
    public:
        UpdateFollowTrigger(PlayerbotAI* ai) : NotNearMasterTrigger(ai, "update follow", 3) {}

        virtual bool IsActive() override
        {
            Stance* stance = AI_VALUE(Stance*, "stance");

            if (ai->IsStateActive(BotState::BOT_STATE_COMBAT) && stance && stance->getName() == "spread")
            {
                return false;
            }

            Unit* followTarget = AI_VALUE(Unit*, "follow target");

            if (!followTarget || !ai->IsSafe(followTarget))
                return false;

            if (followTarget->IsFlying() != bot->IsFlying() || followTarget->IsTaxiFlying())
                return true;

            Formation* formation = AI_VALUE(Formation*, "formation");

            if (sServerFacade.GetChaseTarget(bot) && sServerFacade.GetChaseTarget(bot)->GetObjectGuid() == followTarget->GetObjectGuid() && formation->GetAngle() == sServerFacade.GetChaseAngle(bot) && formation->GetOffset() == sServerFacade.GetChaseOffset(bot))
            {
                return false;
            }

            if (!ai->IsStateActive(BotState::BOT_STATE_COMBAT))
                return true;

            Unit* target = AI_VALUE(Unit*, "current target");
            if (!target)
                return true;

            if (target->GetTargetGuid() == bot->GetObjectGuid())
                return true;

            if (!ai->IsRanged(bot))
                return false;

            WorldPosition formationPosition = AI_VALUE(WorldPosition, "formation position");

            if (formationPosition.sqDistance2d(target) > ai->GetRange("spell"))
                return false;

            return true;
        }
    };

    class StopFollowTrigger : public Trigger
    {
    public:
        StopFollowTrigger(PlayerbotAI* ai) : Trigger(ai, "stop follow", 1) {}

        virtual bool IsActive() override
        {
            Stance* stance = AI_VALUE(Stance*, "stance");

            if (ai->IsStateActive(BotState::BOT_STATE_COMBAT) && stance && stance->getName() == "spread")
            {
                return bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == FOLLOW_MOTION_TYPE;
            }

            if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
                return false;

            if (sServerFacade.GetChaseTarget(bot) && !sServerFacade.GetChaseTarget(bot)->IsPlayer() && sServerFacade.GetChaseTarget(bot)->IsMoving())
            {
                return false;
            }

            Unit* followTarget = AI_VALUE(Unit*, "follow target");

            if (!followTarget)
                return true;

            if (bot->GetTransport() != followTarget->GetTransport())
                return true;

            if (followTarget->IsTaxiFlying())
                return true;

            return false;
        }
    };

    class WanderFarTrigger : public Trigger
    {
    public:
        WanderFarTrigger(PlayerbotAI * ai, std::string name = "wander far", int checkInterval = 2)
            : Trigger(ai, name, checkInterval) {
        }

        bool IsActive() override
        {
            Stance* stance = AI_VALUE(Stance*, "stance");

            if (ai->IsStateActive(BotState::BOT_STATE_COMBAT) && stance && stance->getName() == "spread")
            {
                return false;
            }

            return !AI_VALUE2(bool, "can free move", "wandermax");
        }
    };

    class WanderMediumTrigger : public Trigger
    {
    public:
        WanderMediumTrigger(PlayerbotAI* ai, std::string name = "wander medium", int checkInterval = 2) : Trigger(ai, name, checkInterval) {}

        bool IsActive() override
        {
            Stance* stance = AI_VALUE(Stance*, "stance");

            if (ai->IsStateActive(BotState::BOT_STATE_COMBAT) && stance && stance->getName() == "spread")
            {
                return false;
            }

            return !AI_VALUE2(bool, "can free move", "wandermin") && AI_VALUE2(bool, "can free move", "wandermax");
        }
    };

    class WanderNearTrigger : public Trigger
    {
    public:
        WanderNearTrigger(PlayerbotAI* ai, std::string name = "wander near", int checkInterval = 2) : Trigger(ai, name, checkInterval) {}

        bool IsActive() override
        {
            Stance* stance = AI_VALUE(Stance*, "stance");

            if (ai->IsStateActive(BotState::BOT_STATE_COMBAT) && stance && stance->getName() == "spread")
            {
                return false;
            }

            return AI_VALUE2(bool, "can free move", "wandermin");
        }
    };

    class WaitForAttackSafeDistanceTrigger : public Trigger
    {
    public:
        WaitForAttackSafeDistanceTrigger(PlayerbotAI* ai, std::string name = "wait for attack safe distance") : Trigger(ai, name) {}

        virtual bool IsActive() override
        {
            if (WaitForAttackStrategy::ShouldWait(ai))
            {
                // Do not move if stay strategy is set
                if (!ai->HasStrategy("stay", ai->GetState()))
                {
                    // Do not move if currently being targeted
                    const bool isBeingTargeted = bot->IsInCombat();
                    if (!isBeingTargeted)
                    {
                        Unit* target = AI_VALUE(Unit*, "current target");
                        if (target)
                        {
                            const float safeDistance = WaitForAttackStrategy::GetSafeDistance();
                            const float safeDistanceThreshold = WaitForAttackStrategy::GetSafeDistanceThreshold();
                            const float distanceToTarget = sServerFacade.GetDistance2d(bot, target);
                            return (distanceToTarget > (safeDistance + safeDistanceThreshold)) ||
                                   (distanceToTarget < (safeDistance - safeDistanceThreshold));
                        }
                    }
                }
            }

            return false;
        }
    };

    class OutOfFreeMoveRangeTrigger : public Trigger
    {
    public:
        OutOfFreeMoveRangeTrigger(PlayerbotAI* ai, std::string name = "out of free move range") : Trigger(ai, name) {}

        virtual bool IsActive() override
        {
            Stance* stance = AI_VALUE(Stance*, "stance");

            if (ai->IsStateActive(BotState::BOT_STATE_COMBAT) && stance && stance->getName() == "spread")
            {
                return false;
            }

            return !AI_VALUE(bool, "can free move");
        };
    };
}
