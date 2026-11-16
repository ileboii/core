
#include "playerbot/playerbot.h"
#include "AttackersValue.h"
#include "PossibleTargetsValue.h"
#include "EnemyPlayerValue.h"
#include "RtiTargetValue.h"
#include "BattleGroundAV.h"

#include <algorithm>
#include <functional>
#include <vector>

using namespace ai;
using namespace MaNGOS;

std::list<ObjectGuid> AttackersValue::Calculate()
{
    std::list<ObjectGuid> result;
    if (!ai->AllowActivity(ALL_ACTIVITY))
        return result;

    if (!bot->IsInWorld())
        return result;

    if (bot->IsBeingTeleported())
        return result;

    if (bot->IsFlying() && WorldPosition(bot).currentHeight() > 10.0f)
        return result;

    // lost control, e.g. BG ended
    if (bot->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_CONFUSED))
        return result;

    if (bot->InBattleGround() && bot->GetBattleGroundTypeId() == BATTLEGROUND_AV && !ai->IsAvQuester())
    {
        BattleGround* bg = bot->GetBattleGround();

        if (bg)
        {
            uint32 captainSlot = bot->GetTeam() == ALLIANCE ? BG_AV_CAPTAIN_H : BG_AV_CAPTAIN_A;

            Creature* captain = bot->GetMap()->GetCreature(bg->GetSingleCreatureGuid(captainSlot, 0));

            if (captain && captain->IsInWorld() && captain->GetHealth() > 0 && sServerFacade.IsInCombat(captain))
            {
                bool committed = sServerFacade.GetDistance2d(bot, captain) <= 60.0f || captain->GetThreatManager().getThreat(bot) > 0.0f || captain->GetVictim() == bot || bot->GetVictim() == captain;

                if (!committed)
                {
                    if (Pet* pet = bot->GetPet())
                    {
                        committed = captain->GetThreatManager().getThreat(pet) > 0.0f || captain->GetVictim() == pet || pet->GetVictim() == captain;
                    }
                }

                if (committed)
                    return {captain->GetObjectGuid()};
            }
        }
    }

    if (ai->HasStrategy("focus rti targets", BotState::BOT_STATE_COMBAT) &&
        !RtiTargetValue::GetRtiIndices(AI_VALUE(std::string, "rti")).empty())
    {
        Unit* rtiTarget = AI_VALUE(Unit*, "rti target");

        if (rtiTarget && rtiTarget->IsInWorld() && rtiTarget->GetMapId() == bot->GetMapId())
        {
            return { rtiTarget->GetObjectGuid() };
        }

        return result;
    }

    if (sPlayerbotAIConfig.shareTargets)
    {
        // Try to get the value from nearby friendly bots.
        std::list<ObjectGuid> nearGuids = ai->GetAiObjectContext()->GetValue<std::list<ObjectGuid> >("nearest friendly players")->Get();
        for (auto& i : nearGuids)
        {
            Player* player = sObjectMgr.GetPlayer(i);

            if (!player)
                continue;

            if (player == bot)
                continue;

            if (!ai->IsSafe(player))
                continue;

            if (sServerFacade.GetDistance2d(bot, player) > 10.0f)
                continue;

            PlayerbotAI* botAi = player->GetPlayerbotAI();

            if (!botAi)
                continue;

            std::string valueName = qualifier.empty() ? "attackers" : ("attackers::" + qualifier);

            // Ignore bots without the value.
            if (!PHAS_AI_VALUE(valueName))
                continue;

            UntypedValue* pValue = botAi->GetAiObjectContext()->GetUntypedValue(valueName);

            AttackersValue* pAttackersValue = dynamic_cast<AttackersValue*>(pValue);

            if (!pAttackersValue)
                continue;

            // Ignore expired values.
            if (pAttackersValue->Expired())
                continue;

            if (pAttackersValue->calculatePos.sqDistance2d(bot) > 100.0f)
                continue;

            // Make the value expire at the same time as the copied value.
            lastCheckTime = pAttackersValue->lastCheckTime;

            calculatePos = pAttackersValue->calculatePos;

            result = PAI_VALUE(std::list<ObjectGuid>, valueName);

            std::vector<std::string> specificTargetNames = { "current target","old target","attack target","pull target" };
            Unit* target;

            //Remove bot specific targets of the other bot.
            for (auto& targetName : specificTargetNames)
            {
                target = (targetName == "attack target") ? ai->GetUnit(PAI_VALUE(ObjectGuid, targetName)) : PAI_VALUE(Unit*, targetName);
                if (target)
                    result.remove(target->GetObjectGuid());
            }

            //Add bot specific targets of this bot.
            for (auto& targetName : specificTargetNames)
            {
                target = (targetName == "attack target") ? ai->GetUnit(AI_VALUE(ObjectGuid, targetName)) : AI_VALUE(Unit*, targetName);
                if (target)
                    result.push_back(target->GetObjectGuid());
            }

            //Validate these targets.
            std::list<ObjectGuid> filter;

            for (auto& guid : result)
            {
                target = ai->GetUnit(guid);

                if (!IsValid(target, bot, bot))
                    filter.push_back(guid);
            }

            for(auto& guid : filter)
                result.remove(guid);

            return result;
        }
    }
    
    calculatePos = bot;

    std::set<Unit*> targets;

    // Check if we only need one attacker
    bool getOne = false;
    if (!qualifier.empty())
    {
        getOne = stoi(qualifier);
    }

    std::set<ObjectGuid> invalidTargets;

    // Add the targets of the bot
    AddTargetsOf(bot, targets, invalidTargets, getOne);

    // Don't check for group member targets if we only need one
    if (targets.empty() || !getOne)
    {
        // Add the targets of the members of the group
        Group* group = bot->GetGroup();

        if (group && (!bot->InBattleGround() || (bot->GetBattleGroundTypeId() == BATTLEGROUND_AV && !ai->IsAvQuester())))
        {
            AddTargetsOf(group, targets, invalidTargets, getOne);
        }

        Player* master = GetMaster();
        if (master && !bot->InBattleGround() && (!group || master->GetGroup() != group))
            AddTargetsOf(master, targets, invalidTargets, getOne);
    }

    // Convert the targets to guids
    for (Unit* target : targets)
    {
        result.push_back(target->GetObjectGuid());
    }

    return result;
}

void AttackersValue::AddTargetsOf(Group* group, std::set<Unit*>& targets, std::set<ObjectGuid>& invalidTargets, bool getOne)
{
    Group::MemberSlotList const& groupSlot = group->GetMemberSlots();
    for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); itr++)
    {
        // Only add group member targets that are alive and near the player
        Player* member = sObjectMgr.GetPlayer(itr->guid);
        if (member && (member != bot) &&
           sServerFacade.IsAlive(member) &&
           member->IsInWorld() &&
           !member->IsBeingTeleported() &&
           (member->GetMapId() == bot->GetMapId()) && 
           (sServerFacade.GetDistance2d(bot, member) <= GetRange()))
        {
            AddTargetsOf(member, targets, invalidTargets, getOne);

            // Finish early if we only need one target
            if (getOne && !targets.empty())
            {
                break;
            }
        }
    }
}

void AttackersValue::AddTargetsOf(Player* player, std::set<Unit*>& targets, std::set<ObjectGuid>& invalidTargets, bool getOne)
{
    if (!ai->IsSafe(player))
        return;

    std::vector<Unit*> units;
    if (PlayerbotAI* playerBot = player->GetPlayerbotAI())
    {
        // Validate the nearby candidates together with the player's explicit targets below.
        const std::string range = std::to_string((int32)GetRange());
        const std::vector<std::string> qualifiers = { range, "1" };
        const std::list<ObjectGuid> possibleTargets = PAI_VALUE2(std::list<ObjectGuid>, "possible targets", Qualified::MultiQualify(qualifiers, ":"));
        if (!possibleTargets.empty())
            units.reserve(possibleTargets.size() + player->GetAttackers().size() + 4);

        for (const ObjectGuid& guid : possibleTargets)
        {
            if (Unit* unit = ai->GetUnit(guid))
                units.push_back(unit);
        }

        if (Unit* currentTarget = PAI_VALUE(Unit*, "current target"))
            units.push_back(currentTarget);

        if (Unit* oldTarget = PAI_VALUE(Unit*, "old target"))
            units.push_back(oldTarget);

        if (bot == player)
        {
            if (Unit* attackTarget = playerBot->GetUnit(PAI_VALUE(ObjectGuid, "attack target")))
                units.push_back(attackTarget);

            if (Unit* pullTarget = PAI_VALUE(Unit*, "pull target"))
                units.push_back(pullTarget);
        }
    }

    for (Unit* attacker : player->GetAttackers())
    {
        if (attacker)
            units.push_back(attacker);
    }

    if (bot == player && bot->m_duel && bot->m_duel->opponent)
        units.push_back(bot->m_duel->opponent);

    Pet* pet = player->GetPet();
    if (pet && sServerFacade.GetDistance2d(bot, pet) <= GetRange())
    {
        for (Unit* attacker : pet->GetAttackers())
        {
            if (attacker)
                units.push_back(attacker);
        }
    }

    // Keep the same order as the target set when selecting a single attacker.
    std::sort(units.begin(), units.end(), std::less<Unit*>{});
    units.erase(std::unique(units.begin(), units.end()), units.end());

    for (Unit* unit : units)
    {
        if (!ai->IsSafe(unit) || targets.find(unit) != targets.end())
            continue;

        const ObjectGuid guid = unit->GetObjectGuid();
        if (invalidTargets.find(guid) != invalidTargets.end() && !InCombat(unit, player))
            continue;

        if (!IsValid(unit, player, bot))
        {
            invalidTargets.insert(guid);
            continue;
        }

        targets.insert(unit);
        unit->CallForAllControlledUnits(AddGuardiansHelper(targets), CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM | CONTROLLED_MINIPET | CONTROLLED_TOTEMS);

        if (getOne)
            break;
    }
}

bool AttackersValue::InCombat(Unit* target, Player* player, bool checkPullTargets)
{
    Unit* victim = target->GetVictim();
    if (victim == player || target->GetThreatManager().getThreat(player) > 0.0f)
        return true;

    if (Pet* pet = player->GetPet(); pet && (victim == pet || target->GetThreatManager().getThreat(pet) > 0.0f))
        return true;

    return checkPullTargets && player->GetPlayerbotAI() &&
           (PAI_VALUE(ObjectGuid, "attack target") == target->GetObjectGuid() || PAI_VALUE(Unit*, "pull target") == target);
}

bool AttackersValue::IsValid(Unit* target, Player* player, Player* owner, bool checkInCombat, bool validatePossibleTarget)
{
    if (!target || !player)
        return false;

    Player* playerToCheckAgainst = owner != nullptr ? owner : player;
    if (validatePossibleTarget && (!target->IsInWorld() || target->GetMapId() != playerToCheckAgainst->GetMapId()))
        return false;

    Player* enemyPlayer = target->ToPlayer();
    if (!enemyPlayer && checkInCombat && !InCombat(target, player, player == owner))
    {
        PlayerbotAI* playerAI = player->GetPlayerbotAI();
        if (!playerAI || playerAI->HasActivePlayerMaster() || !RtiTargetValue::IsTarget(player, target))
            return false;
    }

    if (validatePossibleTarget && !PossibleTargetsValue::IsValid(target, playerToCheckAgainst))
        return false;

    if (enemyPlayer)
    {
        PlayerbotAI* ownerAI = playerToCheckAgainst->GetPlayerbotAI();

        if (ownerAI && ownerAI->IsAvQuester())
        {
            // AV questers only fight enemy players that are actually attacking them.
            const auto& attackers = playerToCheckAgainst->GetAttackers();
            if (attackers.find(target) == attackers.end())
                return false;
        }
        else if (ownerAI && !ownerAI->HasStrategy("pvp", BotState::BOT_STATE_COMBAT))
        {
            return false;
        }

        if (!enemyPlayer->IsWithinDist(playerToCheckAgainst, EnemyPlayerValue::GetMaxAttackDistance(playerToCheckAgainst), false))
            return false;
    }

    if ((enemyPlayer || target->GetObjectGuid().IsPet()) &&
        sPlayerbotAIConfig.IsInPvpProhibitedZone(sServerFacade.GetAreaId(target)))
        return false;

    return !IgnoreTarget(target, playerToCheckAgainst);
}

bool AttackersValue::IgnoreTarget(Unit* target, Player* playerToCheckAgainst)
{
    if (!playerToCheckAgainst->GetPlayerbotAI())
        return false; 

    PlayerbotAI* ai = playerToCheckAgainst->GetPlayerbotAI();
    AiObjectContext* context = ai->GetAiObjectContext();

    //Ignore Hard hostiles while not already fighting.
    if (target->GetLevel() > (playerToCheckAgainst->GetLevel() + 5) && ai->GetState() == BotState::BOT_STATE_NON_COMBAT)
    {
        //When traveling a long distance.
        if (AI_VALUE(bool, "travel target traveling") && AI_VALUE2(float, "distance", "travel target") > sPlayerbotAIConfig.reactDistance)
            return true;

        //When moving to master far away.
        if ((ai->HasStrategy("follow", BotState::BOT_STATE_NON_COMBAT) ||
            ai->HasStrategy("wander", BotState::BOT_STATE_NON_COMBAT)) &&
            AI_VALUE2(bool, "trigger active", "out of react range"))
            return true;

        if (ai->GetMaster() && !ai->HasActivePlayerMaster())
        {
            Player* player = ai->GetMaster();

            //When master is traveling a long distance.
            if (PAI_VALUE(bool, "travel target traveling") && PAI_VALUE2(float, "distance", "travel target") > sPlayerbotAIConfig.reactDistance)
                return true;
        }
    }

    if (!target->IsPlayer())
    {
        bool isDummy = false;

        uint32 entry = target->GetEntry();

#define TRAINING_DUMMY_NPC_ENTRY1 190013
#define TRAINING_DUMMY_NPC_ENTRY2 190014
#define TRAINING_DUMMY_NPC_ENTRY3 190015
#define THERAMORE_COMBAT_DUMMY 4952
#define NAXXRAMAS_COMBAT_DUMMY 16211
#ifdef MANGOSBOT_TWO  
#define INITATES_TRAINING_DUMMY 32541
#endif

        switch (entry)
        {
        case TRAINING_DUMMY_NPC_ENTRY1:
        case TRAINING_DUMMY_NPC_ENTRY2:
        case TRAINING_DUMMY_NPC_ENTRY3:
        case THERAMORE_COMBAT_DUMMY:
        case NAXXRAMAS_COMBAT_DUMMY:
#ifdef MANGOSBOT_TWO     
        case INITATES_TRAINING_DUMMY:
#endif
            isDummy = true;
        }

        if (isDummy && ai->GetFixedBotNumber(BotTypeNumber::DUMMY_ATTACK_NUMBER, 10, 0.2f)) //90% of bots, cycle every 5 min.
            return true;
    }

    return false;
}

std::string AttackersValue::Format()
{
    std::ostringstream out;

    for (auto& target : value)
    {
        if(target != value.front())
        out << ",";

        WorldObject* wo = ai->GetWorldObject(target);

        if (wo)
            out << wo->GetName();
        else
            out << target;      
    }

    return out.str();
}

std::list<ObjectGuid> AttackersTargetingMeValue::Calculate()
{
    std::list<ObjectGuid> result;

    const std::list<ObjectGuid>& attackers = AI_VALUE(std::list<ObjectGuid>, "attackers");
    for (const ObjectGuid& attackerGuid : attackers)
    {
        Unit* attacker = ai->GetUnit(attackerGuid);
        if (attacker && (attacker->GetTargetGuid() == bot->GetObjectGuid() || attacker->GetVictim() == bot))
        {
            result.push_back(attackerGuid);
        }
    }

    return result;
}
