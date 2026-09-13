
#include "playerbot/playerbot.h"
#include "WaitForAttackAction.h"
#include "playerbot/strategy/generic/CombatStrategy.h"
#include "playerbot/strategy/generic/KiteStrategy.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using namespace ai;

bool WaitForAttackKeepSafeDistanceAction::Execute(Event& event)
{
    Unit* target = AI_VALUE(Unit*, "current target");

    if (target && !target->IsStopped() && ObjectAccessor::GetUnit(*target, target->GetTargetGuid()) && ObjectAccessor::GetUnit(*target, target->GetTargetGuid())->IsStopped())
        target = ObjectAccessor::GetUnit(*target, target->GetTargetGuid());


    if (target && target->IsAlive())
    {
        const float safeDistance = std::max(float(target->GetMeleeReach() + ATTACK_DISTANCE), WaitForAttackStrategy::GetSafeDistance());
        const float safeDistanceThreshold = WaitForAttackStrategy::GetSafeDistanceThreshold();

        // Find the best point around the target.
        const WorldPosition bestPoint = GetBestPoint(target, (safeDistance - safeDistanceThreshold), safeDistance);
        if (bestPoint)
        {
            // Move to the best point
            return MoveTo(bestPoint.getMapId(), bestPoint.getX(), bestPoint.getY(), bestPoint.getZ(), false, false, false, true);
        }
    }

    return false;
}

const ai::WorldPosition WaitForAttackKeepSafeDistanceAction::GetBestPoint(Unit* target, float minDistance, float maxDistance) const
{
    const Map* map = target->GetMap();
    const WorldPosition botPosition(bot);
    const WorldPosition targetPosition(target);
    const int8 startDir = urand(0, 1) * 2 - 1;
    const float radiansIncrement = (5.0f / 180.0f) * (M_PI_F);
    const float startAngle = targetPosition.getAngleTo(botPosition) + urand(0.f,radiansIncrement) * startDir;
    const float distance = frand(minDistance, maxDistance);
    const std::list<ObjectGuid> enemies = AI_VALUE(std::list<ObjectGuid>, "possible targets no los");

    if (ai->HasStrategy("debug move", BotState::BOT_STATE_COMBAT))
    {
        for (uint32 dist = 0; dist < distance; dist++)
        {
            WorldPosition point = targetPosition + WorldPosition(0, dist * cos(startAngle), dist * sin(startAngle), 1.0f);
            Creature* wpCreature = bot->SummonCreature(1, point.getX(), point.getY(), point.getZ(), 0.0f, TEMPSUMMON_TIMED_DESPAWN, 1000.0f + dist * 100.0f);
        }
    }

    for (float tryAngle = 0.0f; tryAngle < M_PI_F; tryAngle += radiansIncrement)
    {
        for (int8 tryDir = -1; tryAngle && tryDir < 1; tryDir += 2)
        {
            float pointAngle = startAngle;
            pointAngle += tryAngle * startDir * tryDir;

            WorldPosition point = targetPosition + WorldPosition(0, distance * cos(pointAngle), distance * sin(pointAngle), 1.0f);

            point.setZ(point.getHeight());

            if (ai->HasStrategy("debug move", BotState::BOT_STATE_COMBAT))
            {
                Creature* wpCreature = bot->SummonCreature(1, point.getX(), point.getY(), point.getZ(), 0.0f, TEMPSUMMON_TIMED_DESPAWN, 5000.0f + tryAngle * 1000.0f);
            }

            // Check if the target is visible from the point
            if (!target->IsWithinLOS(point.getX(), point.getY(), point.getZ() + bot->GetCollisionHeight()))
                continue;

            // Check if the point is not surrounded by other enemies
            if (IsEnemyClose(point, enemies))
                continue;

            // Check if the bot can move to this point.
            if (!botPosition.canPathTo(point,bot))
                continue;

            if (ai->HasStrategy("debug move", BotState::BOT_STATE_COMBAT))
            {
                Creature* wpCreature = bot->SummonCreature(15631, point.getX(), point.getY(), point.getZ(), 0.0f, TEMPSUMMON_TIMED_DESPAWN, 5000.0f + tryAngle * 1000.0f);
            }

            return point;
        }
    }



    return botPosition;
}

bool WaitForAttackKeepSafeDistanceAction::IsEnemyClose(const WorldPosition& point, const std::list<ObjectGuid>& enemies) const
{
    for (const ObjectGuid& enemyGUID : enemies)
    {
        Unit* enemy = ai->GetUnit(enemyGUID);
        if (enemy)
        {
            // If the enemy is visible in the same map
            if (enemy->IsWithinLOSInMap(bot))
            {
                // If the enemy is not neutral
                if (enemy->IsHostileTo(bot))
                {
                    const float enemyAttackRange = enemy->GetMeleeReach() + ATTACK_DISTANCE;
                    const float distanceToPoint = WorldPosition(enemy).sqDistance(point);
                    if (distanceToPoint <= (enemyAttackRange * enemyAttackRange))
                    {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

bool KitePositionAction::isUseful()
{
    //
    // Deliberately do NOT call MovementAction::isUseful().
    //
    // Normal movement actions are blocked by "stay".
    // Kite positioning must override that because this is combat survival
    // positioning, not ordinary movement behavior.
    //
    return ai->IsStateActive(BotState::BOT_STATE_COMBAT) && ai->HasStrategy("kite", BotState::BOT_STATE_COMBAT);
}

bool KitePositionAction::IsValidHostile(Unit* unit) const { return unit && unit->IsInWorld() && unit->IsAlive() && unit->GetMapId() == bot->GetMapId() && sServerFacade.IsHostileTo(bot, unit); }

Unit* KitePositionAction::GetClosestHostile(const std::list<Unit*>& hostiles) const
{
    Unit* closest = nullptr;
    float closestDistance = 999999.0f;

    for (Unit* hostile : hostiles)
    {
        if (!IsValidHostile(hostile))
            continue;

        const float distance = sServerFacade.GetDistance2d(bot, hostile);

        if (distance < closestDistance)
        {
            closest = hostile;
            closestDistance = distance;
        }
    }

    return closest;
}

bool KitePositionAction::IsSafePoint(const WorldPosition& point, const std::list<Unit*>& hostiles, bool keepCurrentTargetInRange, bool requireCurrentTargetLos, Unit* rangeTarget) const
{
    const float minDistanceSq = KiteStrategy::GetMinDistance() * KiteStrategy::GetMinDistance();

    const float maxDistanceSq = KiteStrategy::GetMaxDistance() * KiteStrategy::GetMaxDistance();

    Unit* currentTarget = rangeTarget ? rangeTarget : AI_VALUE(Unit*, "current target");

    if (IsValidHostile(currentTarget))
    {
        const float distanceSq = WorldPosition(currentTarget).sqDistance2d(point);

        if (distanceSq <= minDistanceSq)
            return false;

        if (keepCurrentTargetInRange && distanceSq > maxDistanceSq)
        {
            return false;
        }

        if (requireCurrentTargetLos && !currentTarget->IsWithinLOS(point.getX(), point.getY(), point.getZ() + bot->GetCollisionHeight()))
        {
            return false;
        }
    }

    //
    // Never choose a destination that leaves us within 10 yards
    // of ANY combat hostile.
    //
    for (Unit* hostile : hostiles)
    {
        if (!IsValidHostile(hostile))
            continue;

        const float distanceSq = WorldPosition(hostile).sqDistance2d(point);

        if (distanceSq <= minDistanceSq)
            return false;
    }

    return true;
}

const WorldPosition KitePositionAction::GetBestPoint(Unit* anchor, const std::list<Unit*>& hostiles, bool outerOnly,
                                                     const WorldPosition* referencePosition, bool checkPath, Unit* rangeTarget) const
{
    if (!IsValidHostile(anchor))
        return WorldPosition();

    const WorldPosition botPosition(bot);
    const WorldPosition anchorPosition(anchor);
    const WorldPosition& directionOrigin = referencePosition ? *referencePosition : botPosition;

    const float startAngle = anchorPosition.getAngleTo(directionOrigin);

    const float radiansIncrement = (15.0f / 180.0f) * M_PI_F;

    const float distances[] = {32.0f, 31.0f, 30.0f, 28.0f, 26.0f, 24.0f, 21.0f, 18.0f, 16.0f};

    const uint8 distanceCount = outerOnly ? 3 : 9;

    for (uint8 pass = 0; pass < 3; ++pass)
    {
        const bool keepCurrentTargetInRange = pass < 2;

        const bool requireCurrentTargetLos = pass == 0;

        for (uint8 d = 0; d < distanceCount; ++d)
        {
            const float distance = distances[d];

            for (uint8 step = 0; step <= 12; ++step)
            {
                const float offset = step * radiansIncrement;

                for (int8 dir = -1; dir <= 1; dir += 2)
                {
                    if (step == 0 && dir == 1)
                        continue;

                    const float pointAngle = startAngle + offset * dir;

                    WorldPosition point = anchorPosition + WorldPosition(0, distance * cos(pointAngle), distance * sin(pointAngle), 1.0f);

                    point.setZ(point.getHeight());

                    if (!anchor->IsWithinLOS(point.getX(), point.getY(), point.getZ() + bot->GetCollisionHeight()))
                    {
                        continue;
                    }

                    if (!IsSafePoint(point, hostiles, keepCurrentTargetInRange, requireCurrentTargetLos, rangeTarget))
                    {
                        continue;
                    }

                    if (checkPath && !IsSafePath(point, hostiles))
                        continue;

                    return point;
                }
            }
        }
    }

    return WorldPosition();
}

bool KitePositionAction::Execute(Event& event)
{
    const std::list<Unit*> hostiles = KiteStrategy::GetNearbyHostiles(ai);

    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    Unit* closest = GetClosestHostile(hostiles);

    const bool tooClose = IsValidHostile(closest) && sServerFacade.GetDistance2d(bot, closest) <= KiteStrategy::GetMinDistance();

    const time_t combatStart = ai->GetAiObjectContext()->GetValue<time_t>("combat start time")->Get();

    const time_t settledCombatStart = ai->GetAiObjectContext()->GetValue<time_t>("manual time", "kite settled combat start")->Get();

    const bool initialPositioning = combatStart && settledCombatStart != combatStart;

    Unit* anchor = nullptr;
    bool outerOnly = false;
    if (tooClose)
    {
        anchor = closest;
        outerOnly = false;
    }
    else if (initialPositioning && IsValidHostile(currentTarget))
    {
        anchor = currentTarget;
        outerOnly = true;
    }
    else if (IsValidHostile(currentTarget) && sServerFacade.GetDistance2d(bot, currentTarget) > KiteStrategy::GetMaxDistance())
    {
        anchor = currentTarget;
        outerOnly = true;
    }

    if (!anchor)
        return false;

    const WorldPosition bestPoint = GetBestPoint(anchor, hostiles, outerOnly);

    if (!bestPoint)
        return false;

    return MoveTo(bestPoint.getMapId(), bestPoint.getX(), bestPoint.getY(), bestPoint.getZ(), false, IsReaction(), false, true);
}

bool KiteStackPositionAction::isUseful()
{
    // Like ordinary kiting, stacking is combat movement and must bypass stay's movement block.
    return ai->IsStateActive(BotState::BOT_STATE_COMBAT) && ai->HasStrategy("kite stack", BotState::BOT_STATE_COMBAT);
}

bool KiteStackPositionAction::Execute(Event& event)
{
    Group* group = bot->GetGroup();
    if (!group)
        return KitePositionAction::Execute(event);

    std::vector<Player*> members;
    uint32 botIndex = 0;
    bool foundBot = false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->getSource();
        if (!member || !member->IsInWorld() || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
            continue;

        PlayerbotAI* memberAI = member->GetPlayerbotAI();
        if (!memberAI || !memberAI->HasStrategy("kite stack", BotState::BOT_STATE_COMBAT))
            continue;

        if (member == bot)
        {
            botIndex = static_cast<uint32>(members.size());
            foundBot = true;
        }

        members.push_back(member);
    }

    // A bot kiting alone keeps the existing kite behavior.
    if (!foundBot)
        return false;

    if (members.size() < 2)
        return KitePositionAction::Execute(event);

    float sumX = 0.0f;
    float sumY = 0.0f;
    float sumZ = 0.0f;

    for (Player* member : members)
    {
        sumX += member->GetPositionX();
        sumY += member->GetPositionY();
        sumZ += member->GetPositionZ();
    }

    const float memberCount = static_cast<float>(members.size());
    const WorldPosition groupCenter(bot->GetMapId(), sumX / memberCount, sumY / memberCount, sumZ / memberCount);

    // Scan from the enrolled bot nearest the cohort center and add every enrolled bot's current target.
    // This keeps the candidate point consistent across the selected cohort.
    Player* representative = members.front();
    float representativeDistanceSq = std::numeric_limits<float>::max();

    for (Player* member : members)
    {
        const float dx = member->GetPositionX() - groupCenter.getX();
        const float dy = member->GetPositionY() - groupCenter.getY();
        const float distanceSq = dx * dx + dy * dy;

        if (distanceSq < representativeDistanceSq)
        {
            representative = member;
            representativeDistanceSq = distanceSq;
        }
    }

    PlayerbotAI* representativeAI = representative->GetPlayerbotAI();
    std::list<Unit*> hostiles = KiteStrategy::GetNearbyHostiles(representativeAI);

    for (Player* member : members)
    {
        PlayerbotAI* memberAI = member->GetPlayerbotAI();
        Unit* target = memberAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();

        if (IsValidHostile(target) && std::find(hostiles.begin(), hostiles.end(), target) == hostiles.end())
            hostiles.push_back(target);
    }

    Unit* anchor = nullptr;
    float closestDistanceSq = std::numeric_limits<float>::max();

    for (Unit* hostile : hostiles)
    {
        if (!IsValidHostile(hostile))
            continue;

        const float dx = groupCenter.getX() - hostile->GetPositionX();
        const float dy = groupCenter.getY() - hostile->GetPositionY();
        const float distanceSq = dx * dx + dy * dy;

        if (distanceSq < closestDistanceSq)
        {
            anchor = hostile;
            closestDistanceSq = distanceSq;
        }
    }

    if (!anchor)
        return false;

    const float minDistance = KiteStrategy::GetMinDistance();
    const float minDistanceSq = minDistance * minDistance;
    bool tooClose = false;

    for (Player* member : members)
    {
        const WorldPosition memberPosition(member);

        for (Unit* hostile : hostiles)
        {
            if (IsValidHostile(hostile) && WorldPosition(hostile).sqDistance2d(memberPosition) <= minDistanceSq)
            {
                tooClose = true;
                break;
            }
        }

        if (tooClose)
            break;
    }

    // Keep the whole assigned cohort in a compact disk around a shared destination.
    const float stackRadius = std::min(3.0f, std::max(1.0f, std::sqrt(memberCount) * 0.5f));
    const float cohesionDistance = stackRadius + 3.0f;
    const float cohesionDistanceSq = cohesionDistance * cohesionDistance;
    bool cohesionNeeded = false;

    for (Player* member : members)
    {
        const float dx = member->GetPositionX() - groupCenter.getX();
        const float dy = member->GetPositionY() - groupCenter.getY();

        if (dx * dx + dy * dy > cohesionDistanceSq)
        {
            cohesionNeeded = true;
            break;
        }
    }

    const time_t combatStart = ai->GetAiObjectContext()->GetValue<time_t>("combat start time")->Get();
    const time_t settledCombatStart = ai->GetAiObjectContext()->GetValue<time_t>("manual time", "kite settled combat start")->Get();
    const bool initialPositioning = combatStart && settledCombatStart != combatStart;

    const float maxDistance = KiteStrategy::GetMaxDistance();
    const bool targetOutOfRange = closestDistanceSq > maxDistance * maxDistance;

    if (!tooClose && !initialPositioning && !targetOutOfRange && !cohesionNeeded)
        return false;

    const bool outerOnly = !tooClose;
    const WorldPosition stackCenter = GetBestPoint(anchor, hostiles, outerOnly, &groupCenter, false, anchor);

    if (!stackCenter)
        return false;

    // Shrink the shared formation if the selected safe point has little room between hostile distance limits.
    float safeStackRadius = stackRadius;

    for (Unit* hostile : hostiles)
    {
        if (!IsValidHostile(hostile))
            continue;

        const float dx = stackCenter.getX() - hostile->GetPositionX();
        const float dy = stackCenter.getY() - hostile->GetPositionY();
        const float distance = std::sqrt(dx * dx + dy * dy);
        const float safeInnerClearance = distance - minDistance - 0.25f;
        safeStackRadius = std::min(safeStackRadius, safeInnerClearance);

        if (hostile == anchor)
        {
            const float safeOuterClearance = KiteStrategy::GetMaxDistance() - distance - 0.25f;
            safeStackRadius = std::min(safeStackRadius, safeOuterClearance);
        }
    }

    safeStackRadius = std::max(0.0f, safeStackRadius);

    const float angle = atan2(stackCenter.getY() - anchor->GetPositionY(), stackCenter.getX() - anchor->GetPositionX()) +
        static_cast<float>(botIndex) * 2.39996322972865332f;
    const float radius = safeStackRadius * std::sqrt((static_cast<float>(botIndex) + 0.5f) / memberCount);

    WorldPosition destination = stackCenter + WorldPosition(0, radius * cos(angle), radius * sin(angle), 0.0f);
    destination.setZ(destination.getHeight());

    if (!IsSafePoint(destination, hostiles, false, false, anchor) || !IsSafePath(destination, hostiles))
        return false;

    return MoveTo(destination.getMapId(), destination.getX(), destination.getY(), destination.getZ(), false, IsReaction(), false, true);
}

bool KitePositionAction::IsSafePath(const WorldPosition& point, const std::list<Unit*>& hostiles) const
{
    const WorldPosition start(bot);

    std::vector<WorldPosition> path = start.getPathTo(point, bot);

    if (path.empty() || !point.isPathTo(path))
        return false;

    const float safeDistance = KiteStrategy::GetMinDistance();

    const float safeDistanceSq = safeDistance * safeDistance;

    for (Unit* hostile : hostiles)
    {
        if (!IsValidHostile(hostile))
            continue;

        const WorldPosition hostilePos(hostile);

        WorldPosition previous = start;

        float previousDistanceSq = hostilePos.sqDistance2d(previous);

        bool escaping = previousDistanceSq <= safeDistanceSq;

        for (const WorldPosition& next : path)
        {
            const float dx = next.getX() - previous.getX();

            const float dy = next.getY() - previous.getY();

            const float lengthSq = dx * dx + dy * dy;

            float t = 0.0f;

            if (lengthSq > 0.001f)
            {
                t = ((hostilePos.getX() - previous.getX()) * dx + (hostilePos.getY() - previous.getY()) * dy) / lengthSq;

                if (t < 0.0f)
                    t = 0.0f;
                else if (t > 1.0f)
                    t = 1.0f;
            }

            const float closestX = previous.getX() + t * dx;

            const float closestY = previous.getY() + t * dy;

            const float enemyDx = hostilePos.getX() - closestX;

            const float enemyDy = hostilePos.getY() - closestY;

            const float segmentDistanceSq = enemyDx * enemyDx + enemyDy * enemyDy;

            const float nextDistanceSq = hostilePos.sqDistance2d(next);

            if (escaping)
            {
                if (segmentDistanceSq + 0.01f < previousDistanceSq)
                {
                    return false;
                }

                if (nextDistanceSq > safeDistanceSq)
                    escaping = false;
            }
            else
            {
                if (segmentDistanceSq <= safeDistanceSq)
                    return false;
            }

            previousDistanceSq = nextDistanceSq;
            previous = next;
        }
    }

    return true;
}
