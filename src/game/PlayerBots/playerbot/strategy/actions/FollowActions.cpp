
#include "playerbot/playerbot.h"
#include "FollowActions.h"
#include "playerbot/PlayerbotAIConfig.h"
#include "playerbot/ServerFacade.h"
#include "playerbot/strategy/values/Formations.h"
#include "playerbot/strategy/values/FreeMoveValues.h"
#include "playerbot/TravelMgr.h"
#include "playerbot/LootObjectStack.h"
#include "MoveSpline.h"
#include "PathFinder.h"
#include "PointMovementGenerator.h"
#include "TargetedMovementGenerator.h"

using namespace ai;

namespace
{
    bool IsOnFoot(Unit const* unit)
    {
        return unit->IsAlive() && unit->IsInWorld() && !unit->IsTaxiFlying() && !unit->IsFlying() &&
            !unit->GetTransport() && !unit->IsInWater() && !unit->IsSwimming();
    }
}

bool FollowAction::NeedsLowerFloorPath(PlayerbotAI* ai, Unit* target)
{
    Player* bot = ai->GetBot();
    return target && target->IsPlayer() && bot->GetPositionZ() - target->GetPositionZ() > 2.0f &&
        ai->IsSafe(target) && sServerFacade.GetDistance2d(bot, target) <= sPlayerbotAIConfig.sightDistance &&
        IsOnFoot(bot) && IsOnFoot(target);
}

bool FollowAction::IsFollowingLowerFloor(PlayerbotAI* ai)
{
    Player const* bot = ai->GetBot();
    MotionMaster const& motion = *bot->GetMotionMaster();
    if (motion.GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE || bot->movespline->Finalized())
        return false;

    auto const* follow = static_cast<FollowAction const*>(ai->GetAiObjectContext()->GetAction("follow"));
    if (!follow || !follow->lowerFloorTarget)
        return false;

    auto const* point = static_cast<PointMovementGenerator<Player> const*>(motion.GetCurrent());
    Vector3 destination;
    point->GetDestination(destination.x, destination.y, destination.z);
    return destination == follow->lowerFloorDestination;
}

bool FollowAction::IsCurrentLowerFloorTarget(Unit* target) const
{
    return ai->IsSafe(target) && target->GetObjectGuid() == lowerFloorTarget && IsOnFoot(target) &&
        (WorldPosition(target).getVector3() - lowerFloorDestination).squaredLength() < 25.0f;
}

bool FollowAction::Execute(Event& event)
{
    bool moved = false;
    Unit* followTarget = AI_VALUE(Unit*, "follow target");

    bool const followingLowerFloor = IsFollowingLowerFloor(ai);
    if (followingLowerFloor && IsCurrentLowerFloorTarget(followTarget))
        return false;

    if (NeedsLowerFloorPath(ai, followTarget) && time(nullptr) >= retryLowerFloorPath &&
        ai->CanMove() && !bot->movespline->IsUninterruptible())
    {
        Vector3 const destination = WorldPosition(followTarget).getVector3();
        PathFinder path(bot);
        // Use the same ground query as .mmap path. Formation offsets and the slope
        // filter can end the route on the upper floor of a WMO.
        path.calculate(destination.x, destination.y, destination.z);
        PointsArray const& points = path.getPath();
        if (path.getPathType() == PATHFIND_NORMAL && points.size() >= 2 &&
            (points.front() - path.getStartPosition()).squaredLength() <= 4.0f &&
            (points.back() - destination).squaredLength() <= 4.0f)
        {
            bot->HandleEmoteState(0);
            if (bot->GetStandState() != UNIT_STAND_STATE_STAND)
                bot->SetStandState(UNIT_STAND_STATE_STAND);
            if (bot->IsNonMeleeSpellCasted(true))
            {
                bot->InterruptNonMeleeSpells(false);
                ai->InterruptSpell();
            }

            MotionMaster& motion = *bot->GetMotionMaster();
            motion.Clear(false);
            motion.MovePoint(0, destination.x, destination.y, destination.z, MOVE_PATHFINDING | MOVE_RUN_MODE);
            lowerFloorTarget = followTarget->GetObjectGuid();
            lowerFloorDestination = destination;
            AI_VALUE(LastMovement&, "last movement").Set(followTarget);
            RESET_AI_VALUE(GuidPosition, "rpg target");
            ClearIdleState();
            WaitForReach(points);
            return true;
        }

        retryLowerFloorPath = time(nullptr) + 1;
    }

    if (followingLowerFloor)
        ai->StopMoving();
    lowerFloorTarget.Clear();

    Formation* formation = AI_VALUE(Formation*, "formation");

    bool useFollowGenerator = ai->IsSafe(followTarget);
    if (useFollowGenerator && followTarget)
    {
        float distance = sServerFacade.GetDistance2d(bot, followTarget);
        useFollowGenerator = !sServerFacade.IsDistanceGreaterThan(distance, sPlayerbotAIConfig.sightDistance);
    }

    if (useFollowGenerator)
    {
        if (formation)
        {
            WorldLocation loc = formation->GetLocation();
            if (!Formation::IsNullLocation(loc) && loc.mapId != -1)
            {
                moved = Follow(followTarget, formation->GetOffset(), formation->GetAngle());
            }
        }
    }
    else if (followTarget)
    {
        moved = MoveTo(followTarget->GetMapId(),
                       followTarget->GetPositionX(),
                       followTarget->GetPositionY(),
                       followTarget->GetPositionZ(),
                       false, false, false, false, true);
    }

    return moved;
}

bool FollowAction::isUseful()
{
    if (!ai->CanMove())
        return false;

    Unit* followTarget = AI_VALUE(Unit*, "follow target");

    if (IsFollowingLowerFloor(ai))
        return !IsCurrentLowerFloorTarget(followTarget);

    if (NeedsLowerFloorPath(ai, followTarget) && time(nullptr) >= retryLowerFloorPath)
        return true;

    Formation* formation = AI_VALUE(Formation*, "formation");

    if (followTarget && followTarget->IsPlayer())
    {
        if (AI_VALUE(GuidPosition, "rpg target") && CanFreeMoveValue::CanFreeMoveTo(ai, AI_VALUE(GuidPosition, "rpg target")))
        {
            return false;
        }
    }

    if (followTarget)
    {
        if (followTarget->IsTaxiFlying() || !CanDeadFollow(followTarget) || followTarget->GetGUIDLow() == bot->GetGUIDLow())
        {
            return false;
        }
    }

    float distance;
    if (followTarget)
    {
        distance = sServerFacade.GetDistance2d(bot, followTarget);
    }
    else
    {
        WorldLocation loc = formation->GetLocation();
        if (Formation::IsNullLocation(loc) || bot->GetMapId() != loc.mapId)
        {
            return false;
        }

        distance = sServerFacade.GetDistance2d(bot, loc.x, loc.y);
    }

    if (sServerFacade.IsDistanceGreaterThan(distance, sPlayerbotAIConfig.sightDistance))
    {
        return true;
    }

    // maybe jump is faster?
    if (ai->HasStrategy("follow jump", BotState::BOT_STATE_NON_COMBAT) && ai->AllowActivity())
    {
        return true;
    }

    if (followTarget)
    {
        MotionMaster* motionMaster = bot->GetMotionMaster();

        if (motionMaster && motionMaster->GetCurrent() && motionMaster->GetCurrent()->GetMovementGeneratorType() == FOLLOW_MOTION_TYPE)
        {
            FollowMovementGenerator<Player> const* follow = static_cast<FollowMovementGenerator<Player> const*>(motionMaster->GetCurrent());

            Unit* currentTarget = follow->GetTarget();

            if (currentTarget && currentTarget->GetObjectGuid() == followTarget->GetObjectGuid())
            {
                return false;
            }
        }
    }

    return true;
}

bool StopFollowAction::Execute(Event& event)
{
    if (event.getSource() == "wander near" && FollowAction::IsFollowingLowerFloor(ai))
        return false;

    ai->StopMoving();
    return true;
}

bool FollowAction::CanDeadFollow(Unit* target)
{
    //Move to corpse when dead and player is alive or not a ghost.
    if (!sServerFacade.IsAlive(bot) && (sServerFacade.IsAlive(target) || !target->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST)))
    {
        return false;
    }
    
    return true;
}

bool FleeToMasterAction::Execute(Event& event)
{
    Player* requester = event.getOwner() ? event.getOwner() : GetMaster();
    Unit* fTarget = AI_VALUE(Unit*, "master target");
    bool canFollow = Follow(fTarget);
    if (!canFollow)
    {
        //SetDuration(5000);
        return false;
    }

    WorldPosition targetPos(fTarget);
    WorldPosition bosPos(bot);
    float distance = bosPos.fDist(targetPos);


    uint32 scale = 5;
    if (bot->GetGroup() && bot->GetGroup()->GetMembersCount() > 5)
        scale = bot->GetGroup()->GetMembersCount();
        

    if (distance > sPlayerbotAIConfig.reactDistance && bot->IsInCombat())
    {
        if (!urand(0, scale))
            ai->TellPlayerNoFacing(requester, "I'm heading to your location but I'm in combat", PlayerbotSecurityLevel::PLAYERBOT_SECURITY_ALLOW_ALL, false);
            //ai->TellPlayer(BOT_TEXT("wait_travel_combat"), PlayerbotSecurityLevel::PLAYERBOT_SECURITY_ALLOW_ALL, false);
    }
    else if (distance < sPlayerbotAIConfig.reactDistance * 3)
    {
        if (!urand(0, scale))
            ai->TellPlayerNoFacing(requester, BOT_TEXT("wait_travel_close"), PlayerbotSecurityLevel::PLAYERBOT_SECURITY_ALLOW_ALL, false);
    }
    else if (distance < 1000)
    {
        if (!urand(0, scale*4))
            ai->TellPlayerNoFacing(requester, BOT_TEXT("wait_travel_medium"), PlayerbotSecurityLevel::PLAYERBOT_SECURITY_ALLOW_ALL, false);
    }
    else
        if (!urand(0, scale*6))
            ai->TellPlayerNoFacing(requester, BOT_TEXT("wait_travel_medium"), PlayerbotSecurityLevel::PLAYERBOT_SECURITY_ALLOW_ALL, false);
           
    SetDuration(3000U);
    return true;
}

bool FleeToMasterAction::isUseful()
{
    if (!ai->CanMove())
        return false;

    if (!ai->GetGroupMaster())
        return false;

    if (ai->GetGroupMaster() == bot)
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");

    if (target && ai->GetGroupMaster()->GetSelectionGuid() == target->GetObjectGuid())
        return false;

    if (!(ai->HasStrategy("follow", BotState::BOT_STATE_NON_COMBAT) ||
      ai->HasStrategy("wander", BotState::BOT_STATE_NON_COMBAT)))
        return false;

    Unit* fTarget = AI_VALUE(Unit*, "master target");
    
    if (!CanDeadFollow(fTarget))
        return false;

    if (fTarget && fTarget->IsPlayer())
    {
        if (AI_VALUE(GuidPosition, "rpg target") && CanFreeMoveValue::CanFreeMoveTo(ai, AI_VALUE(GuidPosition, "rpg target")))
            return false;
    }

    LootObject loot = AI_VALUE(LootObject, "loot target");
    if (loot.IsLootPossible(bot))
        return false;

    return true;
}

