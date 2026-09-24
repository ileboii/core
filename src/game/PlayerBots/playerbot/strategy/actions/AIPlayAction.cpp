#include "playerbot/playerbot.h"
#include "AIPlayAction.h"

#include "playerbot/PlayerbotAI.h"
#include "playerbot/PlayerbotAIConfig.h"
#include "playerbot/PlayerbotLLMInterface.h"
#include "playerbot/PlayerbotTextMgr.h"
#include "playerbot/ServerFacade.h"
#include "playerbot/TravelMgr.h"
#include "playerbot/WorldPosition.h"
#include "playerbot/strategy/actions/ChooseTravelTargetAction.h"
#include "playerbot/strategy/AiObjectContext.h"
#include "playerbot/strategy/NamedObjectContext.h"
#include "playerbot/strategy/actions/SayAction.h"
#include "playerbot/strategy/values/NearestGameObjects.h"
#include "World.h"
#include "ObjectAccessor.h"
#include "Group.h"
#include "Pet.h"
#include "Transport.h"
#include "QuestDef.h"
#include "Log.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <future>
#include <map>
#include <deque>
#include <thread>
#include <utility>
#include <vector>

using namespace ai;

namespace
{
    std::string BriefAIPlayText(std::string text, size_t limit = 48)
    {
        std::string plain;
        plain.reserve(text.size());
        for (size_t i = 0; i < text.size();)
        {
            if (text.compare(i, 2, "|H") == 0)
            {
                const size_t label = text.find("|h", i + 2);
                const size_t end = label == std::string::npos ? std::string::npos : text.find("|h", label + 2);
                if (end != std::string::npos)
                {
                    plain.append(text, label + 2, end - label - 2);
                    i = end + 2;
                    continue;
                }
            }
            if (text.compare(i, 2, "|c") == 0 && i + 10 <= text.size() &&
                std::all_of(text.begin() + i + 2, text.begin() + i + 10,
                    [](unsigned char c) { return std::isxdigit(c) != 0; }))
            {
                i += 10;
                continue;
            }
            if (text.compare(i, 2, "|r") == 0)
            {
                i += 2;
                continue;
            }
            if (text.compare(i, 2, "|T") == 0)
            {
                const size_t end = text.find("|t", i + 2);
                if (end != std::string::npos)
                {
                    i = end + 2;
                    continue;
                }
            }
            plain += text[i++];
        }

        std::string result;
        result.reserve(plain.size());
        for (unsigned char c : plain)
        {
            if (c == '|' || c == ';' || std::isspace(c) || c < 32)
            {
                if (!result.empty() && result.back() != ' ')
                    result += ' ';
            }
            else
                result += static_cast<char>(c);
        }
        if (!result.empty() && result.back() == ' ')
            result.pop_back();
        if (result.size() > limit)
        {
            size_t end = result.rfind(' ', limit);
            if (end == std::string::npos || end < limit / 2)
                end = limit;
            while (end && (static_cast<unsigned char>(result[end]) & 0xc0) == 0x80)
                --end;
            result.resize(end);
        }
        return result;
    }

    uint32 AIPlayPercent(uint32 current, uint32 maximum)
    {
        return maximum ? std::min<uint32>(100, uint32(uint64(current) * 100 / maximum)) : 0;
    }

    bool IsValidAIPlayAttackTarget(Player* bot, Unit* target)
    {
        return bot && target && target->IsInWorld() && sServerFacade.IsAlive(target) &&
            bot->IsValidAttackTarget(target) && bot->IsWithinLOSInMap(target);
    }

    Unit* GetAIPlayAttackTarget(PlayerbotAI* ai, Player* bot, ObjectGuid guid)
    {
        if (!ai || !bot || guid.IsEmpty())
            return nullptr;

        Unit* target = ai->GetUnit(guid);
        return IsValidAIPlayAttackTarget(bot, target) ? target : nullptr;
    }
}

void AIPlayAction::RememberEvent(PlayerbotAI* ai, std::string message)
{
    if (!ai || message.empty())
        return;
    auto& events = ai->aiPlayEvents;
    const time_t now = time(nullptr);
    message = BriefAIPlayText(message, 100);
    if (!events.empty() && events.back().second == message)
        return;
    events.emplace_back(now, std::move(message));
    while (events.size() > 12 || (!events.empty() && now - events.front().first > 300))
        events.pop_front();
}

void AIPlayAction::ObserveFact(PlayerbotAI* ai, const std::string& key, std::string value, bool reportChange)
{
    if (!ai)
        return;
    value = BriefAIPlayText(std::move(value));
    if (ai->aiPlayFacts.size() >= 128 && !ai->aiPlayFacts.count(key))
        ai->aiPlayFacts.erase(ai->aiPlayFacts.begin());
    auto result = ai->aiPlayFacts.emplace(key, value);
    if (!result.second && result.first->second != value)
    {
        if (reportChange)
            RememberEvent(ai, key + ": " + result.first->second + " -> " + value);
        result.first->second = std::move(value);
    }
}

bool AIPlayAttackAction::Execute(Event& event)
{
    (void)event;
    Player* bot = ai ? ai->GetBot() : nullptr;
    AiObjectContext* context = ai ? ai->GetAiObjectContext() : nullptr;
    if (!bot || !context)
        return false;

    // Prefer the master's selection, then the bot's selected/current target.
    Unit* target = nullptr;
    Player* master = ai->GetMaster();
    if (master)
        target = GetAIPlayAttackTarget(ai, bot, master->GetSelectionGuid());
    if (!target)
        target = GetAIPlayAttackTarget(ai, bot, bot->GetSelectionGuid());
    if (!target)
    {
        Value<Unit*>* currentTarget = context->GetValue<Unit*>("current target");
        if (currentTarget && IsValidAIPlayAttackTarget(bot, currentTarget->Get()))
            target = currentTarget->Get();
    }

    // With no selected target, attack the nearest valid hostile creature in sight.
    if (!target)
    {
        Value<std::list<ObjectGuid>>* possibleTargets = context->GetValue<std::list<ObjectGuid>>("possible targets");
        float closestDistance = 1000000000.0f;
        if (possibleTargets)
        {
            for (ObjectGuid guid : possibleTargets->Get())
            {
                Unit* candidate = GetAIPlayAttackTarget(ai, bot, guid);
                if (!candidate || !candidate->IsCreature() || !sServerFacade.IsHostileTo(candidate, bot))
                    continue;

                const float distance = sServerFacade.GetDistance2d(bot, candidate);
                if (distance < closestDistance)
                {
                    closestDistance = distance;
                    target = candidate;
                }
            }
        }
    }

    if (!target)
        return false;

    const ObjectGuid targetGuid = target->GetGUID();
    if (Value<GuidVector>* prioritizedTargets = context->GetValue<GuidVector>("prioritized targets"))
        prioritizedTargets->Set({ targetGuid });

    // It is already doing the requested thing.
    if (bot->GetVictim() == target)
        return true;

    const bool attacked = Attack(bot, target);
    if (attacked)
    {
        if (Value<ObjectGuid>* pullTarget = context->GetValue<ObjectGuid>("pull target"))
            pullTarget->Set(targetGuid);
    }
    return attacked;
}

bool AIPlayMoveToRequesterAction::Execute(Event& event)
{
    // Move once toward the player who asked; this does not install the persistent
    // follow movement generator or alter the bot's follow/stay strategies.
    Player* requester = event.getOwner() ? event.getOwner() : GetMaster();
    if (!requester || requester == bot || !requester->IsInWorld() ||
        requester->GetMapId() != bot->GetMapId() || requester->GetInstanceId() != bot->GetInstanceId())
    {
        return false;
    }

    const float followDistance = ai->GetRange("follow");
    if (sServerFacade.GetDistance2d(bot, requester) <= followDistance)
        return true;

    return MoveNear(requester, followDistance);
}

bool AIPlayMoveRandomAction::Execute(Event&)
{
    const uint32 randnum = urand(1, 2000);
    const float pi = 3.14159265358979323846f;
    const float angle = pi * (float)randnum / 1000.0f;
    const float distance = (float)urand(20, 200);

    return MoveTo(bot->GetMapId(), bot->GetPositionX() + std::cos(angle) * distance,
        bot->GetPositionY() + std::sin(angle) * distance, bot->GetPositionZ());
}

bool AIPlayStopAttackAction::Execute(Event&)
{
    if (!bot)
        return false;

    bot->AttackStop();
    return true;
}

namespace
{
    std::string LowerAIPlayText(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return text;
    }

    struct AIPlayCommand
    {
        const char* id;
        const char* action;
    };

    static const AIPlayCommand aiPlayCommands[] =
    {
        { "ATTACK", "attack my target" },
        { "COME", "ai play move to requester" },
        { "STOP", "ai play stop attack" },
        { "TRAVEL", "ai play travel" },
        { "EXPLORE", "ai play move random" },
        { "LOOT", "loot" },
        { "QUEST", "doquest" },
        { "INTERACT", "ai play interact" },
        { "GREET", "greet" },
        { "EMOTE", "emote" },
        { "EAT", "food" },
        { "DRINK", "drink" },
        { "HEAL", "ai play heal" },
        { "MOUNT", "mount" }
    };

    const AIPlayCommand* FindAIPlayCommand(const std::string& id)
    {
        const std::string loweredId = LowerAIPlayText(id);
        for (const AIPlayCommand& command : aiPlayCommands)
            if (loweredId == LowerAIPlayText(command.id))
                return &command;
        return nullptr;
    }

    std::string ParseAIPlayActionLine(std::string line, bool allowBare = true)
    {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
            return "";
        line.erase(0, start);

        const std::string lower = LowerAIPlayText(line);
        size_t prefix = 0;
        if (lower.compare(0, 7, "action:") == 0)
            prefix = 7;
        else if (lower.compare(0, 8, "command:") == 0 ||
            lower.compare(0, 8, "ai_play=") == 0 || lower.compare(0, 8, "ai_play:") == 0)
            prefix = 8;
        else if (!allowBare)
            return "";

        line.erase(0, prefix);
        start = line.find_first_not_of(" \t");
        if (start == std::string::npos)
            return "";
        line.erase(0, start);
        size_t end = line.find_last_not_of(" \t\r\n");
        line.erase(end + 1);
        if (line.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_") != std::string::npos)
            return "";

        std::transform(line.begin(), line.end(), line.begin(), [](unsigned char c) { return (char)std::toupper(c); });
        if (line == "FOLLOW" || line == "FOLLOWING")
            return "COME";
        return (line == "NONE" || FindAIPlayCommand(line)) ? line : "";
    }

    std::string JoinStrings(const std::vector<std::string>& strings)
    {
        std::string result;
        for (const std::string& value : strings)
        {
            if (value.empty())
                continue;
            if (!result.empty())
                result += " ";
            result += value;
        }
        return result;
    }

    uint32 NextControlInterval()
    {
        uint32 minimum = sPlayerbotAIConfig.llmControlMinInterval;
        uint32 maximum = sPlayerbotAIConfig.llmControlMaxInterval;
        if (maximum < minimum)
            maximum = minimum;
        return urand(minimum, maximum);
    }

    void CapActionGenerationLength(std::string& json);
    void SetActionTemperature(std::string& json);

    bool BuildActionRequest(PlayerbotAI* ai, const std::string& latestText, std::string& json)
    {
        Player* bot = ai ? ai->GetBot() : nullptr;
        AiObjectContext* context = ai ? ai->GetAiObjectContext() : nullptr;
        if (!bot || !context)
            return false;

        std::map<std::string, std::string> placeholders;
        ChatReplyAction::GetAIChatPlaceholders(placeholders, bot, bot);
        ChatReplyAction::GetAIChatPlaceholders(placeholders, bot, "bot", bot);
        placeholders["<other name>"] = "the world around you";
        placeholders["<other gender>"] = "unknown";
        placeholders["<other level>"] = "unknown";
        placeholders["<other class>"] = "unknown";
        placeholders["<other race>"] = "unknown";
        placeholders["<other type>"] = "your surroundings";
        placeholders["<channel name>"] = "while adventuring";
        placeholders["<initial message>"] = latestText.empty() ? "Choose your next action." : latestText;

        std::string previousContext = ai->GetAIPlayContext();
        const size_t maxRecentContext = 512;
        if (previousContext.size() > maxRecentContext)
            previousContext.erase(0, previousContext.size() - maxRecentContext);

        std::map<std::string, std::string> jsonFill;
        jsonFill["<pre prompt>"] = "Select one action. Do not roleplay.";
        jsonFill["<context>"] = previousContext;
        jsonFill["<prompt>"] = latestText.empty() ? "Choose the next useful action from the current situation." : "Recent chat: " + latestText;
        const size_t stateLimit = sPlayerbotAIConfig.llmContextLength ?
            std::min<size_t>(420, std::max<size_t>(180, sPlayerbotAIConfig.llmContextLength / 3)) : 420;
        jsonFill["<prompt>"] += " World: " + AIPlayAction::DescribeWorld(ai, latestText, stateLimit);
        jsonFill["<post prompt>"] = "IDs: " + AIPlayAction::GetCompactActionMenu() +
            " Reply with one ID only. Use NONE when there is no clear action.";

        const uint32 fixedLength = jsonFill["<pre prompt>"].size() + jsonFill["<prompt>"].size() + jsonFill["<post prompt>"].size();
        PlayerbotLLMInterface::LimitContext(jsonFill["<context>"], fixedLength + jsonFill["<context>"].size());

        for (auto& field : jsonFill)
            field.second = PlayerbotLLMInterface::SanitizeForJson(field.second);
        for (auto& placeholder : placeholders)
            placeholder.second = PlayerbotLLMInterface::SanitizeForJson(placeholder.second);

        json = PlayerbotTextMgr::GetReplacePlaceholders(sPlayerbotAIConfig.llmApiJson, jsonFill);
        json = PlayerbotTextMgr::GetReplacePlaceholders(json, placeholders);
        CapActionGenerationLength(json);
        SetActionTemperature(json);
        return !json.empty();
    }

    void CapActionGenerationLength(std::string& json)
    {
        const std::string key = "\"max_length\"";
        const size_t keyPosition = json.find(key);
        if (keyPosition == std::string::npos)
            return;

        const size_t colon = json.find(':', keyPosition + key.size());
        if (colon == std::string::npos)
            return;

        const size_t valueStart = json.find_first_not_of(" \t\r\n", colon + 1);
        if (valueStart == std::string::npos || !std::isdigit(static_cast<unsigned char>(json[valueStart])))
            return;

        size_t valueEnd = valueStart;
        uint32 value = 0;
        while (valueEnd < json.size() && std::isdigit(static_cast<unsigned char>(json[valueEnd])))
        {
            if (value < 1000)
                value = value * 10 + (json[valueEnd] - '0');
            ++valueEnd;
        }

        const uint32 maxActionTokens = 16;
        if (value > maxActionTokens)
            json.replace(valueStart, valueEnd - valueStart, std::to_string(maxActionTokens));
    }

    void SetActionTemperature(std::string& json)
    {
        const size_t key = json.find("\"temperature\"");
        if (key == std::string::npos)
            return;
        const size_t colon = json.find_first_not_of(" \t\r\n", key + 13);
        if (colon == std::string::npos || json[colon] != ':')
            return;
        const size_t start = json.find_first_not_of(" \t\r\n", colon + 1);
        if (start == std::string::npos || !std::isdigit(static_cast<unsigned char>(json[start])))
            return;
        const size_t end = json.find_first_not_of("0123456789.eE+-", start);
        json.replace(start, end - start, "0.3");
    }

    bool ExecuteAIPlayTravel(PlayerbotAI* ai, bool autonomous)
    {
        Player* bot = ai ? ai->GetBot() : nullptr;
        if (!bot || bot->InBattleGround())
            return false;

        AiObjectContext* context = ai->GetAiObjectContext();
        if (!context)
            return false;

        TravelTarget* target = AI_VALUE(TravelTarget*, "travel target");
        if (!target)
            return false;

        if (autonomous && target->IsActive())
            return true;

        // Do not interrupt an in-flight destination query. Otherwise expire
        // the current goal so this explicit request can replace it.
        if (target->GetStatus() == TravelStatus::TRAVEL_STATUS_PREPARE)
            return true;

        const TravelStatus previousStatus = target->GetStatus();
        target->SetStatus(TravelStatus::TRAVEL_STATUS_EXPIRED);
        context->ClearValues("travel target active");
        context->ClearValues("no active travel destinations");

        const uint32 purpose = (uint32)TravelDestinationPurpose::Explore;
        WorldPosition center(bot);
        PlayerTravelInfo travelInfo(bot);
        FutureDestinations* destinations = AI_VALUE(FutureDestinations*, "future travel destinations");
        if (!destinations)
        {
            target->SetStatus(previousStatus);
            context->ClearValues("travel target active");
            return false;
        }

        try
        {
            *destinations = std::async(std::launch::async,
                [partitions = travelPartitions, travelInfo, center, purpose]()
                {
                    return sTravelMgr.GetPartitions(center, partitions, travelInfo, purpose);
                });
        }
        catch (const std::exception&)
        {
            target->SetStatus(previousStatus);
            context->ClearValues("travel target active");
            return false;
        }

        SET_AI_VALUE2(std::string, "manual string", "future travel purpose", std::to_string(purpose));
        SET_AI_VALUE2(std::string, "manual string", "future travel condition", std::string());
        SET_AI_VALUE2(int, "manual int", "future travel relevance", 629);
        target->SetStatus(TravelStatus::TRAVEL_STATUS_PREPARE);
        return true;
    }

    bool ExecuteAIPlayInteract(PlayerbotAI* ai, Player* requester)
    {
        Player* bot = ai ? ai->GetBot() : nullptr;
        AiObjectContext* context = ai ? ai->GetAiObjectContext() : nullptr;
        if (!bot || !context)
            return false;

        Value<std::list<ObjectGuid>>* nearbyNpcs = context->GetValue<std::list<ObjectGuid>>("nearest npcs");
        if (nearbyNpcs)
        {
            for (const ObjectGuid& guid : nearbyNpcs->Get())
            {
                Unit* unit = ai->GetUnit(guid);
                if (!unit || !unit->IsInWorld() || unit->GetMapId() != bot->GetMapId() ||
                    sServerFacade.GetDistance2d(bot, unit) > INTERACTION_DISTANCE)
                {
                    continue;
                }

                // GossipHelloAction receives its NPC target in the event packet;
                // it opens the gossip interaction without selecting an option.
                if (ai->DoSpecificAction("gossip hello", Event("ai play", guid, requester), true))
                    return true;
            }
        }

        Value<std::list<ObjectGuid>>* nearbyObjects = context->GetValue<std::list<ObjectGuid>>("nearest game objects no los");
        if (!nearbyObjects)
            return false;

        GameObject* nearestObject = nullptr;
        float closestDistance = 9999.0f;
        for (const ObjectGuid& guid : nearbyObjects->Get())
        {
            GameObject* gameObject = ai->GetGameObject(guid);
            if (!gameObject || !gameObject->IsInWorld() || gameObject->GetMapId() != bot->GetMapId())
                continue;

            const float distance = bot->GetDistance3dToCenter(gameObject);
            if (distance < closestDistance)
            {
                nearestObject = gameObject;
                closestDistance = distance;
            }
        }

        if (!nearestObject || bot->GetDistance(nearestObject) > INTERACTION_DISTANCE)
            return false;

        // "go" makes the native UseAction activate the nearest gameobject,
        // including its built-in chest, door, and quest-object handling.
        return ai->DoSpecificAction("use", Event("ai play", "go", requester), true);
    }

    bool ExecuteAIPlayHeal(PlayerbotAI* ai, Player* requester)
    {
        Player* bot = ai ? ai->GetBot() : nullptr;
        if (!bot)
            return false;

        std::vector<const char*> healingActions;
        switch (bot->GetClass())
        {
        case CLASS_PRIEST:
            healingActions = { "flash heal on party", "greater heal on party", "heal on party", "lesser heal on party",
                "flash heal", "greater heal", "heal", "lesser heal" };
            break;
        case CLASS_DRUID:
            healingActions = { "healing touch on party", "regrowth on party", "rejuvenation on party",
                "healing touch", "regrowth", "rejuvenation" };
            break;
        case CLASS_PALADIN:
            healingActions = { "holy light on party", "flash of light on party", "lay on hands on party",
                "holy light", "flash of light", "lay on hands" };
            break;
        case CLASS_SHAMAN:
            healingActions = { "healing wave on party", "lesser healing wave on party",
                "healing wave", "lesser healing wave" };
            break;
        default:
            break;
        }

        for (const char* action : healingActions)
        {
            if (ai->DoSpecificAction(action, Event("ai play", "", requester), true))
                return true;
        }

        // Classes without healing spells, or healers without a usable spell,
        // can still recover with the existing potion action.
        return ai->DoSpecificAction("healing potion", Event("ai play", "", requester), true);
    }

    bool ExecuteAIPlayCommand(PlayerbotAI* ai, const std::string& commandId, ObjectGuid ownerGuid,
        bool autonomous = false)
    {
        const AIPlayCommand* command = FindAIPlayCommand(commandId);
        if (!command || !ai || !ai->GetBot() || !ai->GetBot()->IsInWorld() ||
            !sServerFacade.IsAlive(ai->GetBot()) ||
            !ai->HasStrategy("ai play", BotState::BOT_STATE_NON_COMBAT) || !sPlayerbotAIConfig.llmEnabled)
        {
            return false;
        }

        if (sPlayerbotAIConfig.llmRequirePlayerPresence && !ai->HasRealPlayerNearbyOrInGroup())
            return false;

        AIPlayAction::ObserveFact(ai, "intent", commandId, true);

        Player* owner = ownerGuid.IsEmpty() ? nullptr : sObjectAccessor.FindPlayer(ownerGuid);
        if (!owner || !owner->IsInWorld() || !ai->IsRealPlayer(owner))
            owner = ai->GetMaster();

        if (command->id == std::string("TRAVEL") ||
            command->id == std::string("INTERACT") ||
            command->id == std::string("HEAL"))
        {
            bool started = command->id == std::string("TRAVEL") ? ExecuteAIPlayTravel(ai, autonomous) :
                command->id == std::string("INTERACT") ? ExecuteAIPlayInteract(ai, owner) :
                ExecuteAIPlayHeal(ai, owner);
            AIPlayAction::RememberEvent(ai, commandId + (started ? " accepted" : " could not start"));
            return started;
        }
        if (command->id == std::string("ATTACK"))
        {
            AIPlayAttackAction attackAction(ai);
            Event attackEvent("ai play", "", owner);
            if (!attackAction.Execute(attackEvent))
            {
                AIPlayAction::RememberEvent(ai, "ATTACK could not start");
                return false;
            }

            AIPlayAction::RememberEvent(ai, "ATTACK accepted");

            if (ai->HasStrategy("debug llm", BotState::BOT_STATE_NON_COMBAT))
                ai->TellPlayerNoFacing(ai->GetMaster(), "AI play selected action: attack my target");
            return true;
        }

        const std::string action = command->action;
        if (!ai->CanDoSpecificAction(action, true, true))
        {
            AIPlayAction::RememberEvent(ai, commandId + " unavailable");
            return false;
        }

        if (!ai->DoSpecificAction(action, Event("ai play", "", owner), true))
        {
            AIPlayAction::RememberEvent(ai, commandId + " could not start");
            return false;
        }

        AIPlayAction::RememberEvent(ai, commandId + " accepted");

        if (ai->HasStrategy("debug llm", BotState::BOT_STATE_NON_COMBAT))
            ai->TellPlayerNoFacing(ai->GetMaster(), "AI play selected action: " + action);

        return true;
    }

}

std::string AIPlayAction::GetCompactActionMenu()
{
    return "ATTACK, COME, STOP, TRAVEL, "
        "EXPLORE, LOOT, QUEST, "
        "INTERACT, GREET, EMOTE, EAT, DRINK, "
        "HEAL, MOUNT, NONE.";
}

void AIPlayAction::Observe(PlayerbotAI* ai)
{
    Player* bot = ai ? ai->GetBot() : nullptr;
    if (!bot || !bot->IsInWorld())
        return;

    const time_t now = time(nullptr);
    if (ai->aiPlayLastObservation && now - ai->aiPlayLastObservation < 2)
        return;
    ai->aiPlayLastObservation = now;
    while (!ai->aiPlayEvents.empty() && now - ai->aiPlayEvents.front().first > 300)
        ai->aiPlayEvents.pop_front();

    ObserveFact(ai, "life", sServerFacade.IsAlive(bot) ? "alive" : "dead", true);
    ObserveFact(ai, "combat", bot->IsInCombat() ? "fighting" : "safe", true);
    ObserveFact(ai, "health", std::to_string(AIPlayPercent(bot->GetHealth(), bot->GetMaxHealth()) / 20 * 20) + "%");
    if (bot->GetMaxPower(POWER_MANA))
        ObserveFact(ai, "mana", std::to_string(AIPlayPercent(bot->GetPower(POWER_MANA), bot->GetMaxPower(POWER_MANA)) / 20 * 20) + "%");
    ObserveFact(ai, "area", std::to_string(bot->GetMapId()) + "/" +
        std::to_string(sServerFacade.GetAreaId(bot)), true);
    ObserveFact(ai, "enemy", bot->GetVictim() ? bot->GetVictim()->GetName() : "none", true);
    ObserveFact(ai, "transport", bot->GetTransport() ? bot->GetTransport()->GetName() : "none", true);
    ObserveFact(ai, "mounted", bot->IsMounted() ? "yes" : "no");
    ObserveFact(ai, "level", std::to_string(bot->GetLevel()), true);
    ObserveFact(ai, "money", std::to_string(bot->GetMoney() / 10000) + "g");
    ObserveFact(ai, "group", bot->GetGroup() ? std::to_string(bot->GetGroup()->GetMembersCount()) : "solo", true);
    ObserveFact(ai, "companion", ai->GetMaster() ? ai->GetMaster()->GetName() : "none", true);
    std::string harmfulAuras;
    for (auto const& entry : bot->GetSpellAuraHolderMap())
    {
        SpellAuraHolder* holder = entry.second;
        if (!holder || holder->IsPositive() || !holder->GetSpellProto())
            continue;
        if (!harmfulAuras.empty())
            harmfulAuras += ", ";
        harmfulAuras += BriefAIPlayText(holder->GetSpellProto()->SpellName[0], 24);
        if (harmfulAuras.size() > 65)
            break;
    }
    ObserveFact(ai, "debuffs", harmfulAuras.empty() ? "none" : harmfulAuras, true);

    AiObjectContext* context = ai->GetAiObjectContext();
    if (context)
    {
        ObserveFact(ai, "movement", bot->IsStopped() ? "stopped" : "moving");
        auto sensed = [&](const char* name) -> bool
        {
            Value<bool>* found = context->GetValue<bool>(name);
            return found && found->Get();
        };
        ObserveFact(ai, "loot", sensed("has available loot") ? "nearby" : "none", true);
        ObserveFact(ai, "repair", sensed("should repair") ? "needed" : "okay");
        ObserveFact(ai, "selling", sensed("should sell") ? "needed" : "okay");
        if (auto* space = context->GetValue<uint8>("bag space"))
            ObserveFact(ai, "bags", std::to_string(space->Get() / 20 * 20) + "% free");
        if (auto* slots = context->GetValue<uint8>("free quest log slots"))
            ObserveFact(ai, "quest slots", std::to_string(slots->Get()));
        if (auto* attackers = context->GetValue<uint8>("attackers count"))
            ObserveFact(ai, "attackers", std::to_string(attackers->Get()), true);
        if (auto* visible = context->GetValue<std::list<ObjectGuid>>("possible targets"))
            ObserveFact(ai, "visible threats", visible->Get().empty() ? "none" : "present");
        if (auto* nearby = context->GetValue<std::list<ObjectGuid>>("nearest non bot players"))
            ObserveFact(ai, "nearby players", nearby->Get().empty() ? "none" : "present");
        if (auto* travel = context->GetValue<TravelTarget*>("travel target"))
        {
            TravelTarget* target = travel->Get();
            std::string goal = "none";
            if (target && target->GetDestination() &&
                (target->GetStatus() == TravelStatus::TRAVEL_STATUS_TRAVEL ||
                 target->GetStatus() == TravelStatus::TRAVEL_STATUS_WORK))
                goal = target->GetDestination()->GetTitle();
            ObserveFact(ai, "travel goal", goal);
        }
    }

    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->getSource();
            if (!member || member == bot)
                continue;
            const uint32 health = AIPlayPercent(member->GetHealth(), member->GetMaxHealth());
            if (health <= 40 || !sServerFacade.IsAlive(member))
                ObserveFact(ai, std::string("ally ") + member->GetName(),
                    sServerFacade.IsAlive(member) ? std::to_string(health / 20 * 20) + "% hp" : "dead", true);
            else
                ObserveFact(ai, std::string("ally ") + member->GetName(), "okay");
        }
    }
}

std::string AIPlayAction::DescribeWorld(PlayerbotAI* ai, const std::string& topic, size_t maxLength)
{
    Player* bot = ai ? ai->GetBot() : nullptr;
    AiObjectContext* context = ai ? ai->GetAiObjectContext() : nullptr;
    if (!bot || !context || !maxLength)
        return "";
    Observe(ai);

    struct Fact { int priority; std::string category; std::string text; };
    std::vector<Fact> facts;
    const std::string lowerTopic = LowerAIPlayText(topic);
    auto mentions = [&](const char* words) -> bool
    {
        std::string list(words);
        size_t start = 0;
        while (start < list.size())
        {
            size_t end = list.find(' ', start);
            if (end == std::string::npos)
                end = list.size();
            if (lowerTopic.find(list.substr(start, end - start)) != std::string::npos)
                return true;
            start = end + 1;
        }
        return false;
    };
    auto add = [&](int priority, const char* category, std::string value)
    {
        if (value.empty())
            return;
        const std::string categoryName(category);
        const bool related = lowerTopic.find(categoryName) != std::string::npos ||
            (categoryName == "group" && mentions("heal help party friend resurrect")) ||
            (categoryName == "combat" && mentions("attack kill fight enemy target")) ||
            (categoryName == "travel" && mentions("go come follow move explore destination")) ||
            (categoryName == "items" && mentions("loot bag sell item")) ||
            (categoryName == "nearby" && mentions("see look who around interact"));
        if (related)
            priority += 35;
        else if (!lowerTopic.empty() &&
            (categoryName == "travel" || categoryName == "loot" || categoryName == "quest" || categoryName == "items"))
            priority -= 40;
        facts.push_back({ priority, categoryName, BriefAIPlayText(std::move(value), 110) });
    };
    auto value = [&](const char* name) -> std::string
    {
        auto it = ai->aiPlayFacts.find(name);
        return it == ai->aiPlayFacts.end() ? "" : it->second;
    };
    auto flag = [&](const char* name) -> bool
    {
        Value<bool>* observed = context->GetValue<bool>(name);
        return observed && observed->Get();
    };

    std::map<std::string, std::string> placeholders;
    ChatReplyAction::GetAIChatPlaceholders(placeholders, bot, bot);
    std::string place = placeholders["<bot subzone>"];
    std::string zone = placeholders["<bot zone>"];
    if (LowerAIPlayText(place) == "unknown")
        place.clear();
    if (LowerAIPlayText(zone) == "unknown" || zone == place)
        zone.clear();
    if (!place.empty() || !zone.empty())
        add(100, "where", place + (place.empty() || zone.empty() ? "" : ", ") + zone);
    add(98, "self", "hp " + std::to_string(AIPlayPercent(bot->GetHealth(), bot->GetMaxHealth())) + "%" +
        (bot->GetMaxPower(POWER_MANA) ? ", mana " +
            std::to_string(AIPlayPercent(bot->GetPower(POWER_MANA), bot->GetMaxPower(POWER_MANA))) + "%" : "") +
        (bot->IsInCombat() ? ", in combat" : ""));
    if (!sServerFacade.IsAlive(bot))
        add(120, "self", "dead");
    if (bot->IsMounted())
        add(35, "movement", "mounted");
    if (bot->GetTransport())
        add(80, "movement", "aboard " + std::string(bot->GetTransport()->GetName()));
    if (!value("intent").empty())
        add(58, "intent", "last choice " + value("intent"));
    if (ai->HasStrategy("stay", BotState::BOT_STATE_NON_COMBAT))
        add(60, "movement", "staying");
    else if (ai->HasStrategy("follow", BotState::BOT_STATE_NON_COMBAT))
        add(60, "movement", "following");

    if (Player* master = ai->GetMaster())
    {
        std::string companion = master->GetName();
        if (master->IsInWorld() && master->GetMapId() == bot->GetMapId())
            companion += " " + std::to_string(uint32(sServerFacade.GetDistance2d(bot, master))) +
                "y, hp " + std::to_string(AIPlayPercent(master->GetHealth(), master->GetMaxHealth())) + "%";
        else
            companion += " elsewhere";
        add(94, "group", "with " + companion);
    }
    if (Group* group = bot->GetGroup())
    {
        add(48, "group", "group size " + std::to_string(group->GetMembersCount()));
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->getSource();
            if (member && member != bot &&
                (!sServerFacade.IsAlive(member) || AIPlayPercent(member->GetHealth(), member->GetMaxHealth()) <= 60))
                add(90, "group", std::string(member->GetName()) + " hp " +
                    std::to_string(AIPlayPercent(member->GetHealth(), member->GetMaxHealth())) + "%");
        }
    }
    if (Unit* enemy = bot->GetVictim())
        add(105, "combat", std::string("fighting ") + enemy->GetName() + " hp " +
            std::to_string(AIPlayPercent(enemy->GetHealth(), enemy->GetMaxHealth())) + "%");
    if (auto* selected = context->GetValue<Unit*>("current target"))
        if (Unit* unit = selected->Get())
            add(65, "target", std::string("target ") + unit->GetName());
    if (auto* attackers = context->GetValue<uint8>("attackers count"))
        if (attackers->Get())
            add(100, "combat", std::to_string(attackers->Get()) + " attackers");
    if (auto* heal = context->GetValue<Unit*>("party member to heal"))
        if (Unit* member = heal->Get())
            add(95, "group", std::string(member->GetName()) + " needs healing");
    if (auto* resurrect = context->GetValue<Unit*>("party member to resurrect"))
        if (Unit* member = resurrect->Get())
            add(98, "group", std::string(member->GetName()) + " needs resurrection");

    if (auto* travel = context->GetValue<TravelTarget*>("travel target"))
    {
        TravelTarget* target = travel->Get();
        if (target && target->GetDestination() && target->IsActive())
            add(92, "travel", "going to " + target->GetDestination()->GetTitle());
    }
    if (flag("has available loot"))
        add(70, "loot", "loot available");
    if (flag("should repair"))
        add(42, "gear", "needs repair");
    if (flag("should sell"))
        add(42, "items", "should sell items");
    if (auto* space = context->GetValue<uint8>("bag space"))
        if (space->Get() < 20)
            add(68, "items", "bag space " + std::to_string(space->Get()) + "%");
    if (auto* durability = context->GetValue<uint8>("durability"))
        if (durability->Get() < 50)
            add(68, "gear", "durability " + std::to_string(durability->Get()) + "%");
    if (auto* slots = context->GetValue<uint8>("free quest log slots"))
        add(24, "quest", std::to_string(slots->Get()) + " free quest slots");
    if (Pet* pet = bot->GetPet())
        add(52, "pet", std::string(pet->GetName()) + " hp " +
            std::to_string(AIPlayPercent(pet->GetHealth(), pet->GetMaxHealth())) + "%");
    if (Item* weapon = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND))
        if (weapon->GetProto() && weapon->GetProto()->Name1)
            add(26, "gear", std::string("weapon ") + weapon->GetProto()->Name1);

    size_t debuffCount = 0;
    size_t buffCount = 0;
    for (auto const& entry : bot->GetSpellAuraHolderMap())
    {
        SpellAuraHolder* holder = entry.second;
        if (!holder || !holder->GetSpellProto() || holder->GetSpellProto()->SpellName[0].empty())
            continue;
        if (!holder->IsPositive() && debuffCount++ < 3)
            add(88, "self", "debuff " + holder->GetSpellProto()->SpellName[0]);
        else if (holder->IsPositive() && lowerTopic.find("buff") != std::string::npos && buffCount++ < 3)
            add(34, "self", "buff " + holder->GetSpellProto()->SpellName[0]);
        if (debuffCount >= 3 && (buffCount >= 3 || lowerTopic.find("buff") == std::string::npos))
            break;
    }

    size_t questCount = 0;
    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        const uint32 id = bot->GetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_ID_OFFSET);
        if (!id)
            continue;
        ++questCount;
        const Quest* quest = sObjectMgr.GetQuestTemplate(id);
        if (quest && (questCount <= 3 || bot->GetQuestStatus(id) == QUEST_STATUS_COMPLETE))
            add(bot->GetQuestStatus(id) == QUEST_STATUS_COMPLETE ? 75 : 45, "quest",
                std::string(bot->GetQuestStatus(id) == QUEST_STATUS_COMPLETE ? "ready: " : "quest: ") +
                quest->GetTitle());
    }
    if (questCount)
        add(27, "quest", std::to_string(questCount) + " quests in log");

    struct Signal { const char* value; const char* category; const char* label; int priority; };
    static const Signal signals[] = {
        { "should eat", "self", "needs food", 84 },
        { "should drink", "self", "needs water", 84 },
        { "has nearby quest taker", "quest", "quest giver nearby", 65 },
        { "should get money", "money", "needs money", 50 },
        { "can repair", "gear", "can repair here", 60 },
        { "can sell", "items", "can sell here", 56 },
        { "can buy", "items", "can buy here", 38 },
        { "should get mail", "mail", "mail to collect", 60 },
        { "has focus travel target", "travel", "following a focus destination", 68 },
        { "travel target working", "travel", "working at destination", 62 },
        { "travel target traveling", "travel", "traveling to destination", 62 },
        { "near leader", "group", "near group leader", 34 },
        { "following party", "group", "following the party", 40 },
        { "can fight elite", "combat", "can fight elite enemies", 31 },
        { "can fight boss", "combat", "can fight bosses", 31 },
        { "can fish", "world", "can fish here", 28 },
        { "should home bind", "travel", "should set hearthstone", 35 }
    };
    for (const Signal& signal : signals)
        if (flag(signal.value))
            add(signal.priority, signal.category, signal.label);
    if (!sServerFacade.IsAlive(bot) && flag("should spirit healer"))
        add(110, "death", "spirit healer needed");

    if (auto* nearby = context->GetValue<std::list<ObjectGuid>>("nearest friendly players"))
    {
        std::vector<std::pair<float, Player*>> nearbyBots;
        for (ObjectGuid guid : nearby->Get())
        {
            Unit* unit = ai->GetUnit(guid);
            if (!unit || !unit->IsPlayer())
                continue;
            Player* other = static_cast<Player*>(unit);
            if (other != bot && other != ai->GetMaster() && other->IsInWorld() && other->GetPlayerbotAI())
                nearbyBots.emplace_back(sServerFacade.GetDistance2d(bot, other), other);
        }
        std::sort(nearbyBots.begin(), nearbyBots.end(),
            [](const std::pair<float, Player*>& left, const std::pair<float, Player*>& right)
            {
                return left.first < right.first;
            });
        for (size_t i = 0; i < nearbyBots.size() && i < 2; ++i)
        {
            Player* other = nearbyBots[i].second;
            add(84, "nearby", std::string("player ") + other->GetName() + " " +
                std::to_string(uint32(nearbyBots[i].first)) + "y");
        }
    }

    auto sight = [&](const char* name, bool objects, int priority)
    {
        Value<std::list<ObjectGuid>>* nearby = context->GetValue<std::list<ObjectGuid>>(name);
        if (!nearby)
            return;
        size_t count = 0;
        for (ObjectGuid guid : nearby->Get())
        {
            if (count >= 3)
                break;
            if (objects)
            {
                if (GameObject* object = ai->GetGameObject(guid))
                {
                    add(priority, "nearby", std::string("object ") + object->GetName());
                    ++count;
                }
            }
            else if (Unit* unit = ai->GetUnit(guid))
            {
                add(priority + (sServerFacade.IsHostileTo(unit, bot) ? 12 : 0), "nearby",
                    std::string(sServerFacade.IsHostileTo(unit, bot) ? "hostile " : "") +
                    std::string(unit->IsPlayer() ? "player " : "creature ") + unit->GetName());
                ++count;
            }
        }
    };
    sight("possible targets", false, 57);
    sight("nearest npcs", false, 38);
    sight("nearest non bot players", false, 72);
    sight("nearest game objects", true, 35);

    if (!ai->aiPlayEvents.empty())
    {
        size_t count = 0;
        for (auto it = ai->aiPlayEvents.rbegin(); it != ai->aiPlayEvents.rend() && count < 2; ++it, ++count)
            add(68 - int(count * 8), "recent", it->second);
    }

    std::stable_sort(facts.begin(), facts.end(),
        [](const Fact& a, const Fact& b) { return a.priority > b.priority; });
    std::string result;
    for (const Fact& fact : facts)
    {
        const std::string part = (result.empty() ? "" : "; ") + fact.text;
        if (result.size() + part.size() > maxLength)
            continue;
        result += part;
    }
    return result;
}

std::string AIPlayAction::ExtractActionIntent(std::string& text)
{
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    const size_t end = text.find_first_of("\r\n", start);
    const std::string id = ParseAIPlayActionLine(text.substr(start, end - start));
    text.clear();
    return id == "NONE" ? "" : id;
}

std::string AIPlayAction::ExtractCombinedActionIntent(std::string& text, const std::string& responseSpeakerName)
{
    (void)responseSpeakerName;
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";

    size_t firstEnd = text.find_first_of("\r\n", first);
    if (firstEnd == std::string::npos)
        firstEnd = text.size();
    const bool hasReply = text.find_first_not_of(" \t\r\n", firstEnd) != std::string::npos;
    const std::string firstLine = text.substr(first, firstEnd - first);
    std::string id = ParseAIPlayActionLine(firstLine, hasReply);
    if (!hasReply && id.empty() && !ParseAIPlayActionLine(firstLine).empty())
    {
        text.clear();
        return "";
    }
    if (!id.empty())
    {
        size_t eraseEnd = firstEnd;
        while (eraseEnd < text.size() && (text[eraseEnd] == '\r' || text[eraseEnd] == '\n'))
            ++eraseEnd;
        text.erase(0, eraseEnd);
        return id;
    }

    const size_t last = text.find_last_not_of(" \t\r\n");
    const size_t lineBreak = text.find_last_of("\r\n", last);
    if (lineBreak != std::string::npos)
    {
        const size_t lastStart = text.find_first_not_of(" \t", lineBreak + 1);
        if (lastStart != std::string::npos && lastStart <= last)
        {
            id = ParseAIPlayActionLine(text.substr(lastStart, last - lastStart + 1), false);
            if (!id.empty())
            {
                text.erase(lineBreak);
                const size_t end = text.find_last_not_of(" \t\r\n");
                if (end != std::string::npos)
                    text.erase(end + 1);
                return id;
            }
        }
    }

    const size_t tag = LowerAIPlayText(text).rfind("action:");
    if (tag == std::string::npos || !tag || !std::isspace(static_cast<unsigned char>(text[tag - 1])))
        return "";
    id = ParseAIPlayActionLine(text.substr(tag), false);
    if (!id.empty())
    {
        text.erase(tag);
        const size_t end = text.find_last_not_of(" \t\r\n");
        if (end != std::string::npos)
            text.erase(end + 1);
    }
    return id;
}

void AIPlayAction::StartActionSelection(PlayerbotAI* ai, const std::string& latestText, ObjectGuid ownerGuid)
{
    if (!ai || ai->aiPlayGenerationPending || !ai->GetBot() || !ai->GetBot()->IsInWorld() ||
        !ai->HasStrategy("ai play", BotState::BOT_STATE_NON_COMBAT) || !sPlayerbotAIConfig.llmEnabled)
    {
        return;
    }

    if (sPlayerbotAIConfig.llmRequirePlayerPresence && !ai->HasRealPlayerNearbyOrInGroup())
        return;

    if (!PlayerbotLLMInterface::TryReserveAIPlayGeneration())
        return;

    try
    {
        std::string json;
        if (!BuildActionRequest(ai, latestText, json))
        {
            PlayerbotLLMInterface::FinishAIPlayGeneration();
            return;
        }

        if (ai->GetMaster() && ai->HasStrategy("debug llm", BotState::BOT_STATE_NON_COMBAT))
            ai->TellPlayerNoFacing(ai->GetMaster(), "AI play world: " + DescribeWorld(ai, latestText, 480));

        ai->aiPlayGenerationPending = true;
        ObjectGuid botGuid = ai->GetBot()->GetObjectGuid();
        std::thread([botGuid, ownerGuid, autonomous = latestText.empty(), json]()
        {
            struct ReleaseGeneration
            {
                ~ReleaseGeneration() { PlayerbotLLMInterface::FinishAIPlayGeneration(); }
            } releaseGeneration;

            std::string selected;
            try
            {
                std::vector<std::string> debugLines;
                std::string response = PlayerbotLLMInterface::Generate(json,
                    sPlayerbotAIConfig.llmGenerationTimeout, sPlayerbotAIConfig.llmMaxSimultaniousGenerations, debugLines);
                selected = AIPlayAction::ExtractActionIntent(response);
            }
            catch (const std::exception& e)
            {
                sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "AI play action selection error: %s", e.what());
            }

            sWorld.GetMessager().AddMessage([botGuid, ownerGuid, autonomous, selected](World*)
            {
                Player* bot = sObjectAccessor.FindPlayer(botGuid);
                if (!bot || !bot->GetPlayerbotAI())
                    return;

                PlayerbotAI* botAI = bot->GetPlayerbotAI();
                botAI->aiPlayGenerationPending = false;
                if (!bot->IsInWorld())
                    return;

                ExecuteAIPlayCommand(botAI, selected, ownerGuid, autonomous);
            });
        }).detach();
    }
    catch (const std::exception& e)
    {
        ai->aiPlayGenerationPending = false;
        PlayerbotLLMInterface::FinishAIPlayGeneration();
        sLog.Out(LOG_BASIC, LOG_LVL_ERROR, "Unable to start AI play action selection: %s", e.what());
    }
}

bool AIPlayAction::isUseful()
{
    if (!sPlayerbotAIConfig.llmEnabled || !ai->HasStrategy("ai play", BotState::BOT_STATE_NON_COMBAT))
        return false;

    if (sPlayerbotAIConfig.llmRequirePlayerPresence && !ai->HasRealPlayerNearbyOrInGroup())
        return false;

    Observe(ai);
    return !ai->aiPlayGenerationPending && (!ai->nextAIPlayGenerationTime || time(nullptr) >= ai->nextAIPlayGenerationTime);
}

bool AIPlayAction::Execute(Event& event)
{
    (void)event;
    TryStartAutonomous(ai);
    return true;
}

void AIPlayAction::TryStartAutonomous(PlayerbotAI* ai)
{
    if (!ai)
        return;

    AIPlayAction* action = dynamic_cast<AIPlayAction*>(ai->GetAiObjectContext()->GetAction("ai play"));
    if (!action || !action->isUseful())
        return;

    const time_t now = time(nullptr);
    if (!ai->nextAIPlayGenerationTime)
    {
        ai->nextAIPlayGenerationTime = now + NextControlInterval();
        return;
    }

    ai->nextAIPlayGenerationTime = now + NextControlInterval();
    StartActionSelection(ai, "", ai->GetMaster() ? ai->GetMaster()->GetObjectGuid() : ObjectGuid());
}

bool AIPlayAction::ProcessPlayerMessage(PlayerbotAI* ai, uint32 type, ObjectGuid sender, ObjectGuid receiver, const std::string& text)
{
    (void)receiver;
    if (!ai || !ai->HasStrategy("ai play", BotState::BOT_STATE_NON_COMBAT) || !sPlayerbotAIConfig.llmEnabled)
        return false;

    if (sPlayerbotAIConfig.llmRequirePlayerPresence && !ai->HasRealPlayerNearbyOrInGroup())
        return false;

    Player* player = sObjectAccessor.FindPlayer(sender);
    if (!player || !player->IsInWorld() || !ai->IsRealPlayer(player))
        return false;

    Player* bot = ai->GetBot();
    const bool addressed = type == CHAT_MSG_WHISPER;
    Group* group = bot->GetGroup();
    const bool grouped = group && player->GetGroup() == group;
    const std::string normalizedText = LowerAIPlayText(text);
    const std::string loweredBotName = LowerAIPlayText(bot->GetName());
    const bool mentioned = !loweredBotName.empty() && normalizedText.find(loweredBotName) != std::string::npos;
    if (!addressed && !grouped && !mentioned)
        return false;

    if (!ai->aiPlayContext.empty())
        ai->aiPlayContext += "\n";
    ai->aiPlayContext += std::string(player->GetName()) + ": " + text;
    if (ai->aiPlayContext.size() > 32768)
        ai->aiPlayContext.erase(0, ai->aiPlayContext.size() - 32768);

    // The following generated bot reply will be classified together with this
    // player message, so a turn produces at most one selected action.
    return false;
}

bool AIPlayAction::ProcessGeneratedText(PlayerbotAI* ai, const std::string& text, bool appendContext, Player* owner)
{
    if (!ai || !ai->HasStrategy("ai play", BotState::BOT_STATE_NON_COMBAT))
        return false;

    if (appendContext && ai->GetBot() && !text.empty())
    {
        if (!ai->aiPlayContext.empty())
            ai->aiPlayContext += "\n";
        ai->aiPlayContext += std::string(ai->GetBot()->GetName()) + ": " + text;
        if (ai->aiPlayContext.size() > 32768)
            ai->aiPlayContext.erase(0, ai->aiPlayContext.size() - 32768);
    }

    StartActionSelection(ai, text, owner ? owner->GetObjectGuid() : ObjectGuid());
    return ai->aiPlayGenerationPending;
}

void AIPlayAction::QueueGeneratedResponse(ObjectGuid botGuid, ObjectGuid ownerGuid, const std::string& text)
{
    sWorld.GetMessager().AddMessage([botGuid, ownerGuid, text](World*)
    {
        Player* bot = sObjectAccessor.FindPlayer(botGuid);
        if (!bot || !bot->IsInWorld() || !bot->GetPlayerbotAI())
            return;

        Player* owner = ownerGuid.IsEmpty() ? nullptr : sObjectAccessor.FindPlayer(ownerGuid);
        AIPlayAction::ProcessGeneratedText(bot->GetPlayerbotAI(), text, true, owner);
    });
}

void AIPlayAction::QueueCombinedResponse(ObjectGuid botGuid, ObjectGuid ownerGuid,
    const std::string& replyText, const std::string& commandId)
{
    if (botGuid.IsEmpty() || (replyText.empty() && commandId.empty()))
        return;

    sWorld.GetMessager().AddMessage([botGuid, ownerGuid, replyText, commandId](World*)
    {
        Player* bot = sObjectAccessor.FindPlayer(botGuid);
        if (!bot || !bot->IsInWorld() || !bot->GetPlayerbotAI())
            return;

        PlayerbotAI* ai = bot->GetPlayerbotAI();
        if (!ai->HasStrategy("ai play", BotState::BOT_STATE_NON_COMBAT))
            return;

        if (!replyText.empty())
        {
            if (!ai->aiPlayContext.empty())
                ai->aiPlayContext += "\n";
            ai->aiPlayContext += std::string(bot->GetName()) + ": " + replyText;
            if (ai->aiPlayContext.size() > 32768)
                ai->aiPlayContext.erase(0, ai->aiPlayContext.size() - 32768);
        }

        if (!commandId.empty() && LowerAIPlayText(commandId) != "none")
            ExecuteAIPlayCommand(ai, commandId, ownerGuid);
    });
}
