
#include "playerbot/playerbot.h"
#include "CcTargetValue.h"
#include "playerbot/PlayerbotAIConfig.h"
#include "playerbot/ServerFacade.h"
#include "playerbot/strategy/Action.h"
#include "RtiTargetValue.h"

using namespace ai;

class FindTargetForCcStrategy : public FindTargetStrategy
{
public:
    FindTargetForCcStrategy(PlayerbotAI* ai, std::string spell) : FindTargetStrategy(ai)
    {
        this->spell = spell;
        maxDistance = 0;
        Group* group = ai->GetBot()->GetGroup();
        if (group)
        {
            AiObjectContext* context = ai->GetAiObjectContext();
            for (int index : RtiTargetValue::GetRtiIndices(AI_VALUE(std::string, "rti cc")))
                markedTargets.push_back(group->GetTargetWithIcon((RaidTargetIcon)index));
        }
        bestMark = markedTargets.size();
    }

public:
    virtual void CheckAttacker(Unit* creature, ThreatManager* threatManager)
    {
        Player* bot = ai->GetBot();

        AiObjectContext* context = ai->GetAiObjectContext();

        if (!ai->CanCastSpell(spell, creature, true, nullptr, false, true))
            return;

        if (markedTargets.size() > 1 &&
            (ai->HasAura(spell, creature) ||
             (spell == "polymorph" && (ai->HasAura("polymorph: pig", creature) || ai->HasAura("polymorph: turtle", creature)))))
            return;

        for (size_t rank = 0; rank < markedTargets.size(); ++rank)
        {
            if (markedTargets[rank] && markedTargets[rank] == creature->GetObjectGuid())
            {
                if (rank < bestMark)
                {
                    result = creature;
                    bestMark = rank;
                }
                return;
            }
        }

        if (bestMark < markedTargets.size())
            return;

        if (AI_VALUE(Unit*,"current target") == creature)
            return;

        if (!markedTargets.empty() && AI_VALUE(Unit*,"rti target") == creature)
            return;

        uint8 health = creature->GetHealthPercent();
        if (health < sPlayerbotAIConfig.mediumHealth)
            return;

        float minDistance = ai->GetRange("spell");
        Group* group = bot->GetGroup();
        if (!group)
            return;

        if (AI_VALUE(uint8,"aoe count") > 2)
        {
            WorldLocation aoe = AI_VALUE(WorldLocation,"aoe position");
            if (sServerFacade.IsDistanceLessOrEqualThan(sServerFacade.GetDistance2d(creature, aoe.x, aoe.y), sPlayerbotAIConfig.aoeRadius))
                return;
        }

        if (creature->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE) && !(spell == "fear" || spell == "banish"))
            return;

        if (!creature->IsPlayer())
        {
            int tankCount, dpsCount;
            GetPlayerCount(creature, &tankCount, &dpsCount);
            if (!tankCount || !dpsCount)
            {
                result = creature;
                return;
            }
        }

        Group::MemberSlotList const& groupSlot = group->GetMemberSlots();
        for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); itr++)
        {
            Player *member = sObjectMgr.GetPlayer(itr->guid);
            if(!member || !sServerFacade.IsAlive(member) || member == bot || bot->GetMapId() != member->GetMapId())
                continue;

            if (!ai->IsTank(member))
                continue;

            float distance = sServerFacade.GetDistance2d(member, creature);
            if (distance < minDistance)
                minDistance = distance;
        }

        if ((!result && !creature->IsPlayer()) || minDistance > maxDistance)
        {
            result = creature;
            maxDistance = minDistance;
        }
    }

private:
    std::string spell;
    float maxDistance;
    std::vector<ObjectGuid> markedTargets;
    size_t bestMark;
};

Unit* CcTargetValue::Calculate()
{
    if (RtiTargetValue::GetRtiIndices(AI_VALUE(std::string, "rti cc")).size() < 2)
    {
        std::list<ObjectGuid> possible = AI_VALUE(std::list<ObjectGuid>,"possible targets no los");

        for (std::list<ObjectGuid>::iterator i = possible.begin(); i != possible.end(); ++i)
        {
            ObjectGuid guid = *i;
            Unit* add = ai->GetUnit(guid);
            if (!add)
                continue;

            if (!ai->IsSafe(add))
                continue;

            if (ai->HasMyAura(qualifier, add))
                return NULL;

            if (qualifier == "polymorph")
            {
                if (ai->HasMyAura("polymorph: pig", add))
                    return NULL;
                if (ai->HasMyAura("polymorph: turtle", add))
                    return NULL;
            }
        }
    }

    FindTargetForCcStrategy strategy(ai, qualifier);
    return FindTarget(&strategy);
}
