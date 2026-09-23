
#include "playerbot/playerbot.h"
#include "Stances.h"

#include "playerbot/ServerFacade.h"
#include "Arrow.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace ai;

Unit* Stance::GetTarget()
{
    Unit* target = AI_VALUE(Unit*, GetTargetName());
    if (target)
        return target;

    ObjectGuid attackTarget = context->GetValue<ObjectGuid>("attack target")->Get();
    if (attackTarget)
        return ai->GetUnit(attackTarget);

    return NULL;
}

WorldLocation Stance::GetLocation()
{
    Unit* target = GetTarget();
    if (!target)
        return Formation::NullLocation;

    return GetLocationInternal();
}

WorldLocation Stance::GetNearLocation(float angle, float distance)
{
    Unit* target = GetTarget();

    float x = target->GetPositionX() + cos(angle) * distance,
         y = target->GetPositionY()+ sin(angle) * distance,
         z = target->GetPositionZ();

    if (bot->IsWithinLOS(x, y, z + bot->GetCollisionHeight(), true))
        return WorldLocation(bot->GetMapId(), x, y, z);

    return Formation::NullLocation;
}

WorldLocation MoveStance::GetLocationInternal()
{
    Unit* target = GetTarget();
    float distance = std::max(sPlayerbotAIConfig.meleeDistance, target->GetObjectBoundingRadius());

    float angle = GetAngle();
    return GetNearLocation(angle, distance);
}

namespace ai
{
    class NearStance : public MoveStance
    {
    public:
        NearStance(PlayerbotAI* ai) : MoveStance(ai, "near") {}

        virtual float GetAngle() override
        {
            Unit* target = GetTarget();

if (target->GetVictim() && target->GetVictim()->GetObjectGuid() == bot->GetObjectGuid())
    return target->GetOrientation();

            if (ai->HasStrategy("behind", BotState::BOT_STATE_COMBAT))
            {
                Unit* target = GetTarget();
                Group* group = bot->GetGroup();
                int index = 0, count = 0;
                if (group)
                {
                    for (GroupReference *ref = group->GetFirstMember(); ref; ref = ref->next())
                    {
                        Player* member = ref->getSource();
                        if (!ai->IsSafe(member))
                            continue;

                        if (member == bot) index = count;
                        if (member && !ai->IsRanged(member) && !ai->IsTank(member)) count++;
                    }
                }

                float angle = target->GetOrientation() + M_PI;
                if (!count) return angle;

                return round((angle - M_PI / 4 + (M_PI / 2 / count) * (index + 0.5f)) * 10.0f) / 10.0f;
            }

            float angle = GetFollowAngle() + target->GetOrientation();

            Player* master = GetMaster();
            if (master)
                angle -= master->GetOrientation();

            return angle;
        }
    };

    class TankStance : public MoveStance
    {
    public:
        TankStance(PlayerbotAI* ai) : MoveStance(ai, "tank") {}

        virtual float GetAngle() override
        {
            Unit* target = GetTarget();
            return target->GetOrientation();
        }
    };

    class TurnBackStance : public MoveStance
    {
    public:
        TurnBackStance(PlayerbotAI* ai) : MoveStance(ai, "turnback") {}

        virtual float GetAngle() override
        {
            Unit* target = GetTarget();
            Group* group = bot->GetGroup();
            if (!group)
                return target->GetOrientation();

            float sumX = 0.0f, sumY = 0.0f;
            int count = 0;
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->getSource();
                if (!member || !ai->IsSafe(member) || member == bot)
                    continue;
                if (!ai->IsRanged(member))
                    continue;
                sumX += member->GetPositionX();
                sumY += member->GetPositionY();
                count++;
            }

            if (!count)
                return target->GetOrientation();

            float centerX = sumX / count;
            float centerY = sumY / count;

            float angleToRaid = atan2(centerY - target->GetPositionY(),
                                      centerX - target->GetPositionX());

            return angleToRaid + (float)M_PI;
        }
    };

    class BehindStance : public MoveStance
    {
    public:
        BehindStance(PlayerbotAI* ai) : MoveStance(ai, "behind") {}

        virtual float GetAngle() override
        {
            Unit* target = GetTarget();

            if (target && target->GetTypeId() != TYPEID_PLAYER && target->GetVictim() &&
                target->GetVictim()->GetObjectGuid() == bot->GetObjectGuid())
            {
                return target->GetOrientation();
            }

            Group* group = bot->GetGroup();
            int index = 0, count = 0;
            if (group)
            {
                for (GroupReference *ref = group->GetFirstMember(); ref; ref = ref->next())
                {
                    Player* member = ref->getSource();
                    if (!ai->IsSafe(member))
                        continue;
                    if (member == bot) index = count;
                    if (member && !ai->IsRanged(member) && !ai->IsTank(member)) count++;
                }
            }

            float angle = target->GetOrientation() + M_PI;
            if (!count) return angle;

            return round((angle - M_PI / 4 + (M_PI / 2 / count) * (index + 0.5f)) * 10.0f) / 10.0f;
        }
    };

    class AutoStance : public MoveStance
    {
    public:
        AutoStance(PlayerbotAI* ai) : MoveStance(ai, "auto") {}

        virtual float GetAngle() override
        {
            if (ai->IsTank(bot))
            {
                TankStance stance(ai);
                return stance.GetAngle();
            }

            if (ai->IsHeal(bot))
            {
                NearStance stance(ai);
                return stance.GetAngle();
            }

            if (ai->IsRanged(bot))
            {
                NearStance stance(ai);
                return stance.GetAngle();
            }

            BehindStance stance(ai);
            return stance.GetAngle();
        }
    };

    class SpreadStance : public Stance
    {
    public:
        SpreadStance(PlayerbotAI* ai) : Stance(ai, "spread") {}

    protected:
        WorldLocation GetLocationInternal() override
        {
            if (ai->IsTank(bot))
            {
                Unit* target = GetTarget();

                if (!target || target->GetVictim() != bot)
                {
                    return WorldLocation(bot->GetMapId(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
                }
            }

            const float spreadDistance = 10.0f;
            const float spreadPadding = 0.5f;
            const float epsilon = 0.01f;

            const float botX = bot->GetPositionX();
            const float botY = bot->GetPositionY();
            const float botZ = bot->GetPositionZ();

            Group* group = bot->GetGroup();
            if (!group)
                return WorldLocation(bot->GetMapId(), botX, botY, botZ);

            std::vector<Player*> closeMembers;

            float pushX = 0.0f;
            float pushY = 0.0f;

            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->getSource();

                if (!member || member == bot || !ai->IsSafe(member) || !sServerFacade.IsAlive(member) || member->GetMapId() != bot->GetMapId())
                {
                    continue;
                }

                const float dx = botX - member->GetPositionX();
                const float dy = botY - member->GetPositionY();
                const float distanceSq = dx * dx + dy * dy;

                if (distanceSq >= spreadDistance * spreadDistance)
                    continue;

                closeMembers.push_back(member);

                if (distanceSq > epsilon * epsilon)
                {
                    const float distance = sqrt(distanceSq);
                    const float weight = (spreadDistance - distance) / distance;

                    pushX += dx * weight;
                    pushY += dy * weight;
                }
            }

            if (closeMembers.empty())
                return WorldLocation(bot->GetMapId(), botX, botY, botZ);

            float angle;

            if ((pushX * pushX + pushY * pushY) > epsilon * epsilon)
            {
                angle = atan2(pushY, pushX);
            }
            else
            {
                angle = GetFollowAngle();
            }

            const float dirX = cos(angle);
            const float dirY = sin(angle);

            float moveDistance = 0.0f;

            for (Player* member : closeMembers)
            {
                const float dx = botX - member->GetPositionX();
                const float dy = botY - member->GetPositionY();

                const float dot = dx * dirX + dy * dirY;

                const float c = dx * dx + dy * dy - spreadDistance * spreadDistance;

                const float discriminant = dot * dot - c;

                if (discriminant < 0.0f)
                    continue;

                const float exitDistance = -dot + sqrt(discriminant);

                moveDistance = std::max(moveDistance, exitDistance);
            }

            moveDistance += spreadPadding;

            float x = botX + dirX * moveDistance;
            float y = botY + dirY * moveDistance;
            float z = botZ;

            if (!bot->IsFlying() && !bot->IsSwimming())
            {
                z += CONTACT_DISTANCE;
                bot->UpdateAllowedPositionZ(x, y, z);
            }

            return WorldLocation(bot->GetMapId(), x, y, z);
        }
    };
};

StanceValue::StanceValue(PlayerbotAI* ai) : ManualSetValue<Stance*>(ai, new AutoStance(ai), "stance")
{
}

void StanceValue::Reset()
{
    if (value)
        delete value;
    value = new AutoStance(ai);
}

std::string StanceValue::Save()
{
    return value ? value->getName() : "?";
}

bool StanceValue::Load(std::string name)
{
    if (name == "auto" || name == "default")
    {
        if (value)
            delete value;
        value = new AutoStance(ai);
    }
    else if (name == "behind")
    {
        if (value)
            delete value;
        value = new BehindStance(ai);
    }
    else if (name == "near")
    {
        if (value)
            delete value;
        value = new NearStance(ai);
    }
    else if (name == "tank")
    {
        if (value) delete value;
        value = new TankStance(ai);
    }
    else if (name == "turnback" || name == "turn")
    {
        if (value) delete value;
        value = new TurnBackStance(ai);
    }
    else if (name == "spread")
    {
        if (value)
            delete value;
        value = new SpreadStance(ai);
    }
    else return false;

    return true;
}

bool SetStanceAction::Execute(Event& event)
{
    std::string stance = event.getParam();
    Player* requester = event.getOwner() ? event.getOwner() : GetMaster();

    StanceValue* value = (StanceValue*)context->GetValue<Stance*>("stance");
    if (stance == "?" || stance.empty())
    {
        std::ostringstream str; str << "Stance: |cff00ff00" << value->Get()->getName();
        ai->TellPlayer(requester, str);
        return true;
    }

    if (stance == "show")
    {
        WorldLocation loc = value->Get()->GetLocation();
        if (!Formation::IsNullLocation(loc))
            ai->Ping(loc.x, loc.y);

        return true;
    }

    if (!value->Load(stance))
    {
        std::ostringstream str; str << "Invalid stance: |cffff0000" << stance;
        ai->TellPlayer(requester, str);
        ai->TellPlayer(requester, "Please set to any of:|cffffffff auto (default), near, tank, turnback, behind, spread");
        return false;
    }

    std::ostringstream str; str << "Stance set to: " << stance;
    ai->TellPlayer(requester, str);
    return true;
}
