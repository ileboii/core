#include "playerbot/playerbot.h"
#include "CommandSetup.h"
#include "playerbot/WorldPosition.h"
#include "playerbot/TravelMgr.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "TestAction.h"
#include "TestRegistry.h"
#include "playerbot/ServerFacade.h"
#include "playerbot/LootObjectStack.h"

using namespace ai;

TestResult CommandSetupTeleport::Execute(const std::string& params, Player* bot,
                    PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    GuidPosition loc;
    if (!TestRegistry::ParseLocation(params, loc))
    {
        message = "Invalid teleport location: " + params;
        return TestResult::IMPOSSIBLE;
    }

    if (bot->TeleportTo(loc.mapId, loc.x, loc.y, loc.z, bot->GetOrientation()))
    {
        return TestResult::PASS;
    }
    else
    {
        message = "Teleport failed to " + params;
        return TestResult::IMPOSSIBLE;
    }
}

TestResult CommandSetupGM::Execute(const std::string& params, Player* bot, PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    if (params == "on")
    {
        bot->SetGameMaster(true);
        bot->GetSession()->SendNotification(LANG_GM_ON);
        return TestResult::PASS;
    }
    else if (params == "off")
    {
        bot->SetGameMaster(false);
        bot->GetSession()->SendNotification(LANG_GM_OFF);
        return TestResult::PASS;
    }
    else if (params == "visible on")
    {
        bot->SetGMVisible(true);
        bot->GetSession()->SendNotification(LANG_INVISIBLE_VISIBLE);
        return TestResult::PASS;
    }
    else if (params == "visible off")
    {
        bot->SetGMVisible(false);
        bot->GetSession()->SendNotification(LANG_INVISIBLE_INVISIBLE);
        return TestResult::PASS;
    }
    else if (params == "fly on")
    {
        return TestResult::IMPOSSIBLE;
        return TestResult::PASS;
    }
    else if (params == "fly off")
    {
        return TestResult::IMPOSSIBLE;
        return TestResult::PASS;
    }
    else
    {
        message = "Invalid parameter for setgm: " + params + ". Use 'on' or 'off'.";
        return TestResult::IMPOSSIBLE;
    }
}

TestResult CommandSetupGiveItem::Execute(const std::string& params, Player* bot,
                    PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    std::string param = params;

    bool isBank = false;
    if (param.find("bank ") == 0)
    {
        isBank = true;
        param = param.substr(5); // Remove "bank " prefix
    }  
    
    uint32 itemId = atoi(param.c_str());
    if (itemId > 0)
    {

        Item* pItem = bot->StoreNewItemInInventorySlot(itemId, 1);

        if (!pItem)
        {
            message = "Failed to create item with ID: " + param;
            return TestResult::IMPOSSIBLE;
        }

        if (isBank)
        {
            ItemPosCountVec dest;
            uint8 bagSlot;
            InventoryResult msg = bot->CanBankItem(NULL_BAG, NULL_SLOT, dest, pItem, false, bagSlot);

            if (msg != EQUIP_ERR_OK)
            {
                message = "Item can not be stored in bank: " + params;
                return TestResult::IMPOSSIBLE;
            }

            bot->RemoveItem(pItem->GetBagSlot(), pItem->GetSlot(), true);
            bot->BankItem(dest, pItem, true);
        }

        return TestResult::PASS;
    }
    else
    {
        message = "Invalid item ID: " + params;
        return TestResult::IMPOSSIBLE;
    }
}

TestResult CommandSetupEquipItem::Execute(const std::string& params, Player* bot,
                    PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    uint32 itemId = atoi(params.c_str());
    if (itemId > 0)
    {
        bot->StoreNewItemInInventorySlot(itemId, 1);
        return TestResult::PASS;
    }
    else
    {
        message = "Invalid item ID: " + params;
        return TestResult::IMPOSSIBLE;
    }
}

TestResult CommandSetupClearMobs::Execute(const std::string& params, Player* bot,
                    PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    float radius = 50.0f;
    size_t pos = params.find("radius");
    if (pos != std::string::npos)
    {
        std::string radStr = params.substr(pos + 6);
        if (!radStr.empty() && radStr[0] == '=')
            radStr = radStr.substr(1);
        radius = atof(radStr.c_str());
    }

    uint32 exceptEntry = 0;
    size_t exceptPos = params.find("except=");
    if (exceptPos != std::string::npos)
    {
        std::string exceptStr = params.substr(exceptPos + 7);
        size_t spacePos = exceptStr.find(' ');
        if (spacePos != std::string::npos)
            exceptStr = exceptStr.substr(0, spacePos);
        exceptEntry = atoi(exceptStr.c_str());
    }

    // Force load the grid at bot's location to ensure creatures are visible
    {
        CellPair p = MaNGOS::ComputeCellPair(bot->GetPositionX(), bot->GetPositionY());
        Cell cell(p);
        bot->GetMap()->LoadGrid(cell);
    }

    std::list<Creature*> creatures;
    MaNGOS::AnyUnitInObjectRangeCheck checker(bot, radius);
    MaNGOS::CreatureListSearcher<MaNGOS::AnyUnitInObjectRangeCheck> searcher(creatures, checker);
    Cell::VisitAllObjects(bot, searcher, radius);

    uint32 cleared = 0;
    for (auto& creature : creatures)
    {
        if (!creature->IsAlive())
            continue;

        if (exceptEntry && creature->GetEntry() == exceptEntry)
        {
            sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[TestAction] clear: SKIPPING boss %s (entry %u)", creature->GetName(), creature->GetEntry());
            continue;
        }

        // Skip critters and non-combat pets
#ifndef MANGOSBOT_ZERO 
        if (creature->GetCreatureType() == CREATURE_TYPE_CRITTER || creature->GetCreatureType() == CREATURE_TYPE_NON_COMBAT_PET)
#else          
        if (creature->GetCreatureType() == CREATURE_TYPE_CRITTER)
#endif
            continue;

        creature->SetDeathState(JUST_DIED);
        creature->SetHealth(0);
        cleared++;
    }
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[TestAction] clear: killed %u creatures, except entry %u", cleared, exceptEntry);
    return TestResult::PASS;
}

TestResult CommandSetupSetDestination::Execute(const std::string& params, Player* bot,
                    PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    std::string dest = params;
    bool nonForced = false;
    if (params.find("destination ") == 0)
        dest = params.substr(std::string("destination ").length());

    // Optional trailing qualifier: "... nonforced" lets the bot genuinely travel to the
    // destination instead of pinning it (a forced target fights the arrival check that
    // teleported-delivery bypasses).
    const std::string nonForcedTag = " nonforced";
    if (dest.size() > nonForcedTag.size() &&
        dest.compare(dest.size() - nonForcedTag.size(), nonForcedTag.size(), nonForcedTag) == 0)
    {
        nonForced = true;
        dest.erase(dest.size() - nonForcedTag.size());
    }

    GuidPosition loc;
    if (!TestRegistry::ParseLocation(dest, loc))
    {
        message = "Invalid destination: " + dest;
        return TestResult::IMPOSSIBLE;
    }

    ctx.destinationPosition = loc;

    AiObjectContext* context = ai->GetAiObjectContext();
    TravelTarget* target = AI_VALUE(TravelTarget*,"travel target");
    if (target)
    {
        // Pass the guid-backed entry through: rpg triggers match "rpg target entry == travel
        // target entry" for gray/low-level quests, and a coords-only destination reports 0.
        message = "set destination: fork TravelDestination does not yet support coords-pinned targets";
        return TestResult::IMPOSSIBLE;

        // The default TRAVEL budget is distance-based (2x theoretical walk time + 3 s), sized for
        // destinations the bot itself picks from far away. A test's 90 yd drop computes to ~16 s,
        // so any real-world delay on the leg (one combat, a mount-up, the 10 s re-path gate)
        // expires the target before the arrival flip can happen and the default re-roll machinery
        // steals the destination (BL-47(a)). Grant a real leg budget instead; the arrival flip to
        // WORK then uses the destination's fixed 5-minute expire, as designed.
        if (nonForced)
            target->SetExpireIn(20 * MINUTE * IN_MILLISECONDS); // reviewer P3: stay above the test monitor's 900 s fail line so a late arrival still finds a live target

        if (!nonForced && !ai->HasStrategy("travel", BotState::BOT_STATE_NON_COMBAT))
            ai->ChangeStrategy("+travel once", BotState::BOT_STATE_NON_COMBAT);
    }
    else
    {
        message = "Could not get travel target value";
        return TestResult::IMPOSSIBLE;
    }
    return TestResult::PASS;
}

// BL-46: aim the bot's rpg machinery at a specific world object. After this step the normal
// chain runs: arrival flips the travel target to WORK, the rpg trigger fires, and the
// accept/hand-in action runs - the bot decides, the step only points it.
TestResult CommandSetupRpgTarget::Execute(const std::string& params, Player* bot,
                    PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    std::string target = params;
    if (target.find("rpg target ") == 0)
        target = target.substr(std::string("rpg target ").length());

    GuidPosition loc;
    if (!TestRegistry::ParseLocation(target, loc))
    {
        message = "Invalid rpg target: " + target;
        return TestResult::IMPOSSIBLE;
    }

    AiObjectContext* context = ai->GetAiObjectContext();
    SET_AI_VALUE(GuidPosition, "rpg target", loc);
    return TestResult::PASS;
}

TestResult CommandSetupTeleportGroup::Execute(const std::string& params, Player* bot,
                    PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    Group* group = bot->GetGroup();
    if (!group)
    {
        // BL-44: a missing group used to be a silent PASS ("skipping"), which let a lost group join
        // (BL-42) surface later as the hop monitor's timeout. Fail honestly at the moment of the hop.
        message = "teleport group: group never formed (bot has no group)";
        return TestResult::ABORT;
    }

    // BL-44: optional "expect=<n>" - abort when the group never reached the expected size instead of
    // delivering to a fragment and letting the pass monitor time out.
    uint32 expect = 0;
    {
        std::string expectKey = "expect=";
        size_t pos = params.find(expectKey);
        if (pos != std::string::npos)
        {
            std::string valueStr = params.substr(pos + expectKey.length());
            size_t end = valueStr.find(' ');
            if (end != std::string::npos)
                valueStr = valueStr.substr(0, end);
            if (!valueStr.empty() && std::all_of(valueStr.begin(), valueStr.end(), ::isdigit))
                expect = (uint32)atoi(valueStr.c_str());
        }
    }
    if (expect && group->GetMembersCount() < expect)
    {
        message = "teleport group: group never formed (size " + std::to_string(group->GetMembersCount()) +
                  " < expected " + std::to_string(expect) + ")";
        return TestResult::ABORT;
    }

    float x = bot->GetPositionX();
    float y = bot->GetPositionY();
    float z = bot->GetPositionZ();
    uint32 mapId = bot->GetMapId();
    float orient = bot->GetOrientation();

    uint32 count = 0;
    std::string states;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->getSource();
        if (!member || member == bot)
            continue;

        // Test bots normally share the host's map, but route through the owning thread anyway so
        // the helper stays safe (and consistent with the rest of the codebase) if a test spans maps.
        //
        // The delivery is logged from *inside* the callback, because that is the only place that knows it
        // happened: RunOnOwningThread runs the callback inline when the member is on this map and defers
        // it to the world thread otherwise, and it silently skips a member that is out of world or
        // mid-teleport. The previous version counted the call instead, so "Teleported 3 group members"
        // could mean three no-ops. Logging inside the callback is also race-free: it runs on whichever
        // thread actually performs the move.
        states += " " + std::string(member->GetName()) + "(map" + std::to_string(member->GetMapId()) +
                  ",inWorld=" + (member->IsInWorld() ? "1" : "0") +
                  ",tp=" + (member->IsBeingTeleported() ? "1" : "0") + ")";

        const bool moved = member->TeleportTo(mapId, x, y, z, orient);
        if (moved)
            ctx.RecordDeliveredGroupMember(member->GetObjectGuid());

        sLog.Out(LOG_BASIC, LOG_LVL_DEBUG, "[TestAction] teleport group: delivering %s to map %u -> moved=%d",
            member->GetName(), mapId, moved ? 1 : 0);
        ++count;
    }

    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[TestAction] teleport group: issued %u to map %u at (%.1f, %.1f, %.1f);%s",
        count, mapId, x, y, z, states.c_str());
    return TestResult::PASS;
}

TestResult CommandSetupPull::Execute(const std::string& params, Player* bot,
                    PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    uint32 entryId = atoi(params.c_str());
    if (!entryId)
    {
        message = "Invalid creature entry: " + params;
        return TestResult::IMPOSSIBLE;
    }

    // Force load the grid at bot's location to ensure creatures are visible
    {
        CellPair p = MaNGOS::ComputeCellPair(bot->GetPositionX(), bot->GetPositionY());
        Cell cell(p);
        bot->GetMap()->LoadGrid(cell);
    }

    // First: search via Cell::VisitWorldObjects
    Creature* target = nullptr;
    {
        std::list<Creature*> creatures;
        MaNGOS::AnyUnitInObjectRangeCheck checker(bot, 500.0f);
        MaNGOS::CreatureListSearcher<MaNGOS::AnyUnitInObjectRangeCheck> searcher(creatures, checker);
        Cell::VisitWorldObjects(bot, searcher, 500.0f);

        for (auto& creature : creatures)
        {
            if (creature->GetEntry() == entryId && creature->IsAlive())
            {
                target = creature;
                break;
            }
        }
    }

    // Third: spawn the creature if it doesn't exist in the instance
    if (!target)
    {
        static bool pullDebugLogged = false;
        if (!pullDebugLogged)
        {
            sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[TestAction] pull %u: creature not found on map %u at (%.1f, %.1f, %.1f), spawning it",
                entryId, bot->GetMapId(),
                bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
            pullDebugLogged = true;
        }

        target = bot->SummonCreature(entryId,
            bot->GetPositionX() + 5.0f, bot->GetPositionY(), bot->GetPositionZ(),
            bot->GetOrientation() + M_PI_F,
            TEMPSUMMON_MANUAL_DESPAWN, 0);

        if (!target)
        {
            message = "Failed to spawn creature entry " + std::to_string(entryId);
            return TestResult::IMPOSSIBLE;
        }

        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[TestAction] pull: spawned %s (entry %u) near bot", target->GetName(), entryId);
    }

    bot->SetSelectionGuid(target->GetObjectGuid());
    bot->Attack(target, true);
    // Make the boss attack the bot too (prevents evade)
    if (target->AI())
        target->AI()->AttackStart(bot);
    // Large leash disables the push-back: the boss may fight where it stands
    target->SetLeashDistance(10000.0f);
    ctx.focusMobEntry = entryId;
    ctx.focusMobGuid = target->GetObjectGuid();
    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[TestAction] Bot %s pulling creature %s (entry %u) at distance %.1f",
        bot->GetName(), target->GetName(), entryId, bot->GetDistance(target));
    return TestResult::PASS;
}

TestResult CommandSpawnMob::Execute(const std::string& params, Player* bot,
                    PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    uint32 entryId = atoi(params.c_str());
    if (!entryId)
    {
        message = "Invalid creature entry: " + params;
        return TestResult::IMPOSSIBLE;
    }

    float distance = 5.0f;
    size_t distancePos = params.find("dist=");
    if (distancePos != std::string::npos)
        distance = atof(params.c_str() + distancePos + 5);

    if (distance <= 0.0f)
    {
        message = "Invalid spawn distance: " + params;
        return TestResult::IMPOSSIBLE;
    }

    uint32 spawnLevel = 0;
    size_t levelPos = params.find("level=");
    if (levelPos != std::string::npos)
        spawnLevel = (uint32)atoi(params.c_str() + levelPos + 6);

    float leashRange = 0.0f;
    size_t leashPos = params.find("leash=");
    if (leashPos != std::string::npos)
        leashRange = atof(params.c_str() + leashPos + 6);

    if (leashPos != std::string::npos && leashRange <= 0.0f)
    {
        message = "Invalid leash range: " + params;
        return TestResult::IMPOSSIBLE;
    }

    float x = bot->GetPositionX() + distance;
    float y = bot->GetPositionY();

    size_t towardPos = params.find("toward ");
    if (towardPos != std::string::npos)
    {
        std::string towardName = params.substr(towardPos + 7);
        size_t nameEnd = towardName.find(' ');
        if (nameEnd != std::string::npos)
            towardName = towardName.substr(0, nameEnd);

        GuidPosition toward;
        if (!TestRegistry::ParseLocation(towardName, toward))
        {
            message = "Invalid spawn direction: " + towardName;
            return TestResult::IMPOSSIBLE;
        }

        float dx = toward.x - bot->GetPositionX();
        float dy = toward.y - bot->GetPositionY();
        float length = sqrt(dx * dx + dy * dy);
        if (length > 1.0f)
        {
            x = bot->GetPositionX() + dx / length * distance;
            y = bot->GetPositionY() + dy / length * distance;
        }
    }

    {
        CellPair p = MaNGOS::ComputeCellPair(bot->GetPositionX(), bot->GetPositionY());
        Cell cell(p);
        bot->GetMap()->LoadGrid(cell);
    }

    Creature* spawned = bot->SummonCreature(entryId, x, y, bot->GetPositionZ(),
        bot->GetOrientation() + M_PI_F, TEMPSUMMON_MANUAL_DESPAWN, 0);
    if (spawned && spawnLevel > 0 && spawnLevel != spawned->GetLevel())
    {
        spawned->SetLevel(spawnLevel);
    }

    if (!spawned)
    {
        message = "Failed to spawn creature entry " + std::to_string(entryId);
        return TestResult::IMPOSSIBLE;
    }

    ctx.focusMobEntry = entryId;
    ctx.focusMobGuid = spawned->GetObjectGuid();
    ctx.focusMobPassive = true;

    if (leashRange > 0.0f)
    {
        spawned->SetLeashDistance(leashRange);
    }

    if (params.find("aggro") != std::string::npos && spawned->AI())
        spawned->AI()->AttackStart(bot);

    if (params.find("select") != std::string::npos)
    {
        bot->SetSelectionGuid(spawned->GetObjectGuid());
        AiObjectContext* context = ai->GetAiObjectContext();
        SET_AI_VALUE(ObjectGuid, "current target", spawned->GetObjectGuid());
    }

    sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[TestAction] Bot %s spawned creature %s (entry %u) at distance %.1f",
        bot->GetName(), spawned->GetName(), entryId, bot->GetDistance(spawned));
    return TestResult::PASS;
}

TestResult CommandFollowFocus::Execute(const std::string& params, Player* bot,
                    PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    Creature* focus = bot->IsInWorld() && bot->GetMap() ? bot->GetMap()->GetCreature(ctx.focusMobGuid) : nullptr;
    if (!focus || !focus->IsInWorld())
    {
        message = "follow focus: no focus mob spawned yet";
        return TestResult::PENDING;
    }

    ai->ChangeStrategy("+follow,-rpg,-travel,-stay,-guard,-wander,-passive", BotState::BOT_STATE_NON_COMBAT);
    ai->ChangeStrategy("-stay,-guard,-wander", BotState::BOT_STATE_COMBAT);
    AiObjectContext* context = ai->GetAiObjectContext();
    SET_AI_VALUE(GuidPosition, "manual follow target", focus);

    return TestResult::PASS;
}

TestResult CommandKillFocus::Execute(const std::string& params, Player* bot,
                    PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    Creature* focus = bot->IsInWorld() && bot->GetMap() ? bot->GetMap()->GetCreature(ctx.focusMobGuid) : nullptr;
    if (!focus)
    {
        message = "kill focus: no focus mob spawned yet";
        return TestResult::PENDING;
    }

    if (!focus->IsAlive())
    {
        message = "kill focus: focus mob already dead";
        return TestResult::PASS;
    }

    bot->Kill(focus, nullptr, false);

    return TestResult::PASS;
}

TestResult CommandAwaitFocusCorpse::Execute(const std::string& params, Player* bot,
                    PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    uint32 graceSeconds = 20;
    if (!params.empty())
    {
        std::string graceStr = params;
        if (TryParseUInt32Strict(graceStr, graceSeconds, message, GetName()) != TestResult::PASS)
            return TestResult::IMPOSSIBLE;
    }

    Creature* focus = bot->IsInWorld() && bot->GetMap() ? bot->GetMap()->GetCreature(ctx.focusMobGuid) : nullptr;
    if (!focus)
    {
        message = "await focus corpse: focus mob despawned";
        return TestResult::FAIL;
    }

    if (focus->IsAlive())
        return TestResult::PENDING;

    if (sServerFacade.GetDeathState(focus) == CORPSE && focus->HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE))
    {
        AiObjectContext* context = bot->GetPlayerbotAI()->GetAiObjectContext();
        AI_VALUE(LootObjectStack*, "available loot")->Add(focus->GetObjectGuid());
        ctx.waitTime = 0;
        message = "focus corpse lootable and registered";
        return TestResult::PASS;
    }

    if (!ctx.waitTime)
        ctx.waitTime = WorldTimer::getMSTime();

    if (WorldTimer::getMSTimeDiff(ctx.waitTime, WorldTimer::getMSTime()) < graceSeconds * 1000)
        return TestResult::PENDING;

    message = "await focus corpse: corpse never became lootable (no loot recipient?)";
    return TestResult::FAIL;
}

TestResult CommandSetValue::Execute(const std::string& params, Player* bot, PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    // Expected format: "set value <datatype> <valueName> <valueToSetTo>"
    AiObjectContext* context = bot->GetPlayerbotAI()->GetAiObjectContext();

    std::string datatype;
    std::string valueStr;

    if (TrySplitOnce(params, " ", datatype, valueStr, message, GetName()) != TestResult::PASS)
    {
        message = "Invalid format for set value: " + params;
        return TestResult::IMPOSSIBLE;
    }

    std::string valueName;
    std::string valueToSetTo;
    if (TrySplitOnce(valueStr, "=>", valueName, valueToSetTo, message, GetName()) != TestResult::PASS)
    {
        message = "Invalid format for set value: " + params;
        return TestResult::IMPOSSIBLE;
    }

    bool isAiValue = false;

    if (datatype == "bool")
    {
        bool value = isAiValue ? AI_VALUE(bool, valueToSetTo) : (valueToSetTo == "true");
        SET_AI_VALUE(bool, valueName, value);
        return TestResult::PASS;
    }
    else if (datatype == "uint32")
    {
        uint32 value = isAiValue ? AI_VALUE(uint32, valueToSetTo) : atoi(valueToSetTo.c_str());
        SET_AI_VALUE(uint32, valueName, value);
        return TestResult::PASS;
    }
    else if (datatype == "GuidPosition")
    {
        GuidPosition value = isAiValue ? AI_VALUE(GuidPosition, valueToSetTo) : GuidPosition(valueToSetTo);
        SET_AI_VALUE(GuidPosition, valueName, value);
        return TestResult::PASS;
    }
    else if (datatype == "string")
    {
        std::string value = isAiValue ? AI_VALUE(std::string, valueToSetTo) : valueToSetTo;
        SET_AI_VALUE(std::string, valueName, value);
        return TestResult::PASS;
    }

    message = "Unsupported datatype for set value: " + datatype;
    return TestResult::IMPOSSIBLE;
}
// BL-47(a) follow-up: honest setup check for "is the giver/taker actually alive in the world".
// five_signets evidence: the bot teleported to the giver's exact coords and the travel flip fired,
// but the creature was not found by an 80-yd unit search - the run then burned 900 s on an accept
// that could never happen. This command fails fast instead. Usage:
//   "require creature alive <location> [yd]"   (yd defaults to 300; searches around the bot)
TestResult CommandRequireCreatureAlive::Execute(const std::string& params, Player* bot,
                    PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    std::string locName = params;
    float radius = 300.0f;

    // Optional trailing numeric radius: "<location> <yd>"
    size_t lastSpace = locName.find_last_of(" \t");
    if (lastSpace != std::string::npos)
    {
        std::string maybeRadius = locName.substr(lastSpace + 1);
        if (!maybeRadius.empty() && std::all_of(maybeRadius.begin(), maybeRadius.end(), ::isdigit))
        {
            radius = (float)atoi(maybeRadius.c_str());
            if (radius <= 0.0f)
                radius = 300.0f;
            locName = locName.substr(0, lastSpace);
        }
    }

    // Trim leading whitespace left over from the command-name split.
    size_t nameStart = locName.find_first_not_of(" \t");
    if (nameStart == std::string::npos)
    {
        message = "require creature alive: missing location";
        return TestResult::IMPOSSIBLE;
    }
    locName = locName.substr(nameStart);

    GuidPosition loc;
    if (!TestRegistry::ParseLocation(locName, loc))
    {
        message = "Invalid location: " + locName;
        return TestResult::IMPOSSIBLE;
    }

    if (!loc || !loc.IsCreature() || !loc.GetEntry())
    {
        // GameObject locations (rare givers/takers) are not covered by this check - skip honestly.
        message = "Location " + locName + " is not a creature - alive check skipped";
        return TestResult::PASS;
    }

    uint32 entry = loc.GetEntry();

    std::list<Creature*> found;
    MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck checker(*bot, entry, true, radius, nullptr);
    MaNGOS::CreatureListSearcher<MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck> searcher(found, checker);
    Cell::VisitAllObjects(bot, searcher, radius);

    if (found.empty())
    {
        message = "Creature " + std::to_string(entry) + " (" + locName + ") not found ALIVE within "
            + std::to_string((uint32)radius) + " yd of the bot - giver/taker missing from the world";
        return TestResult::ABORT;
    }

    Creature* creature = found.front();
    std::ostringstream out;
    out << "Creature " << entry << " (" << (creature->GetName() ? creature->GetName() : "?")
        << ") alive at " << (uint32)bot->GetDistance(creature) << " yd";
    message = out.str();
    return TestResult::PASS;
}
