#include "playerbot/playerbot.h"
#include "AvTestCommands.h"
#include "AvTestSupport.h"
#include "TestRegistry.h"
#include "BattleGround.h"
#include "BattleGroundAV.h"
#include "BattleGroundMgr.h"
#include "playerbot/strategy/actions/BattleGroundJoinAction.h"

using namespace ai;

namespace
{
    BattleGroundAV* GetAvForBot(Player* bot)
    {
        BattleGround* bg = bot->GetBattleGround();
        if (!bg || bg->GetTypeID() != BATTLEGROUND_AV)
            return nullptr;

        return (BattleGroundAV*)bg;
    }

    class TestAvJoinAction : public BGJoinAction
    {
    public:
        TestAvJoinAction(PlayerbotAI* ai) : BGJoinAction(ai) {}
        bool Queue() { return JoinQueue(BATTLEGROUND_QUEUE_AV); }
    };

    bool ParseAvOwnerState(std::string const& ownerName, std::string const& stateName, uint8& owner, uint8& state)
    {
        owner = ownerName == "alliance" ? 0 : ownerName == "horde" ? 1 : ownerName == "neutral" ? 2 : 255;
        state = stateName == "assaulted" ? 0 : stateName == "controlled" ? 1 : 255;
        return owner != 255 && state != 255;
    }

    bool CollectAvSquad(Player* bot, TestContext& ctx, std::vector<Player*>& squad, std::string& message)
    {
        squad.push_back(bot);
        for (ObjectGuid const& guid : ctx.spawnedBots)
        {
            Player* member = sObjectMgr.GetPlayer(guid);
            if (!member || !member->IsInWorld())
            {
                message = "Waiting for spawned bots to enter world before joining Alterac Valley";
                return false;
            }
            squad.push_back(member);
        }
        return true;
    }
}

TestResult CommandAvStatus::Execute(const std::string& params, Player* bot,
                    PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    BattleGroundAV* av = GetAvForBot(bot);
    if (!av)
    {
        message = "av status: bot is not in an Alterac Valley";
        return TestResult::IMPOSSIBLE;
    }

    for (uint8 i = 0; i < AV_TEST_NODE_COUNT; ++i)
    {
        AvTestNode const& node = GetAvTestNodes()[i];
        std::ostringstream out;
        out << "av node " << node.name << ": " << FormatAvNodeState(av->GetAvNodeOwner(node.nodeId), av->GetAvNodeState(node.nodeId));
        ai->TellPlayerNoFacing(bot, out.str());
    }

    for (uint8 i = 0; i < AV_TEST_MINE_COUNT; ++i)
    {
        std::ostringstream out;
        out << "av mine " << uint32(i) << ": " << FormatAvNodeState(av->GetAvMineOwner(i), 1);
        ai->TellPlayerNoFacing(bot, out.str());
    }

    return TestResult::PASS;
}

TestResult RequireAvNode::Execute(const std::string& params, Player* bot,
                    PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    std::vector<std::string> args = Qualified::getMultiQualifiers(params, " ");
    if (args.size() < 2)
    {
        message = "usage: require av node <node name> <alliance|horde|neutral> [assaulted|controlled]";
        return TestResult::IMPOSSIBLE;
    }

    std::string nodeKey = args[0];
    std::string ownerName = args[1];
    std::string stateName = args.size() > 2 ? args[2] : "controlled";

    uint8 nodeId = 0;
    if (!ParseAvNodeName(nodeKey, nodeId))
    {
        message = "unknown av node: " + nodeKey;
        return TestResult::IMPOSSIBLE;
    }

    BattleGroundAV* av = GetAvForBot(bot);
    if (!av)
    {
        message = "require av node: bot is not in an Alterac Valley";
        return TestResult::IMPOSSIBLE;
    }

    uint8 expectedOwner = 0;
    uint8 expectedState = 0;
    if (!ParseAvOwnerState(ownerName, stateName, expectedOwner, expectedState))
    {
        message = "unknown av owner/state: " + ownerName + " " + stateName;
        return TestResult::IMPOSSIBLE;
    }

    uint8 owner = av->GetAvNodeOwner(nodeId);
    uint8 state = av->GetAvNodeState(nodeId);
    if (owner == expectedOwner && state == expectedState)
        return TestResult::PASS;

    message = "av node " + nodeKey + " is " + FormatAvNodeState(owner, state) + ", expected " + FormatAvNodeState(expectedOwner, expectedState);
    return TestResult::FAIL;
}

TestResult CommandAvJoin::Execute(const std::string& params, Player* bot,
                    PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    if (!sBattleGroundMgr.isTesting())
    {
        message = "av join: battleground testing mode is off - enable it with .debug bg";
        return TestResult::IMPOSSIBLE;
    }

    uint32 timeoutSec = 300;
    std::string timeoutArg = params;
    timeoutArg.erase(0, timeoutArg.find_first_not_of(' '));
    if (!timeoutArg.empty())
    {
        if (timeoutArg.size() > 4 || timeoutArg.find_first_not_of("0123456789") != std::string::npos)
        {
            message = "usage: av join [timeout seconds 1-3600]";
            return TestResult::IMPOSSIBLE;
        }
        timeoutSec = std::min<uint32>(std::max<uint32>(std::stoul(timeoutArg), 1), 3600);
    }

    if (!ctx.avJoinStart)
        ctx.avJoinStart = WorldTimer::getMSTime();

    bool timedOut = WorldTimer::getMSTimeDiff(ctx.avJoinStart, WorldTimer::getMSTime()) > timeoutSec * IN_MILLISECONDS;

    std::vector<Player*> squad;
    if (!CollectAvSquad(bot, ctx, squad, message))
    {
        if (!timedOut)
            return TestResult::PENDING;

        ctx.avJoinStart = 0;
        message = "av join: spawned bots did not enter the world within " + std::to_string(timeoutSec) + "s";
        return TestResult::FAIL;
    }

    uint32 inside = 0;
    for (Player* member : squad)
    {
        if (BattleGroundAV* av = GetAvForBot(member))
        {
            if (member->GetMapId() == av->GetMapId() && member->IsInWorld() && !member->IsBeingTeleported())
                ++inside;
            continue;
        }

        if (member->InBattleGround())
        {
            message = "av join: " + std::string(member->GetName()) + " is in another battleground";
            return TestResult::IMPOSSIBLE;
        }

        if (member->InBattleGroundQueueForBattleGroundQueueType(BATTLEGROUND_QUEUE_AV))
            continue;

        PlayerbotAI* memberAi = member->GetPlayerbotAI();
        if (!memberAi)
        {
            message = "av join: " + std::string(member->GetName()) + " is not a bot";
            return TestResult::IMPOSSIBLE;
        }

        TestAvJoinAction join(memberAi);
        if (!join.Queue())
        {
            message = "av join: " + std::string(member->GetName()) + " could not queue for Alterac Valley (level " + std::to_string(member->GetLevel()) + ")";
            return TestResult::IMPOSSIBLE;
        }
    }

    if (inside == squad.size())
    {
        ctx.avJoinStart = 0;
        return TestResult::PASS;
    }

    if (timedOut)
    {
        std::string stuck;
        for (Player* member : squad)
        {
            if (BattleGroundAV* av = GetAvForBot(member))
            {
                if (member->GetMapId() != av->GetMapId() || !member->IsInWorld() || member->IsBeingTeleported())
                    stuck += " " + std::string(member->GetName()) + "=porting";
                continue;
            }

            GroupQueueInfo ginfo;
            std::string state = "not queued";
            if (member->InBattleGroundQueueForBattleGroundQueueType(BATTLEGROUND_QUEUE_AV) && sBattleGroundMgr.m_battleGroundQueues[BATTLEGROUND_QUEUE_AV].GetPlayerGroupInfoData(member->GetObjectGuid(), &ginfo))
                state = ginfo.isInvitedToBgInstanceGuid ? "invited" : "queued";
            stuck += " " + std::string(member->GetName()) + "=" + state;
        }
        if (stuck.find("=queued") != std::string::npos && sWorld.getConfig(CONFIG_UINT32_BATTLEGROUND_INVITATION_TYPE) != 0)
            stuck += " (Battleground.InvitationType balances team sizes; one-sided squads need 0)";
        message = "av join: " + std::to_string(inside) + "/" + std::to_string(squad.size()) + " bots entered Alterac Valley within " + std::to_string(timeoutSec) + "s;" + stuck;
        ctx.avJoinStart = 0;
        return TestResult::FAIL;
    }

    message = "Waiting for " + std::to_string(squad.size() - inside) + " bots to enter Alterac Valley";
    return TestResult::PENDING;
}

TestResult CommandAvLeave::Execute(const std::string& params, Player* bot,
                    PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    ctx.avJoinStart = 0;

    if (bot->InBattleGround())
        bot->LeaveBattleground();
    else if (bot->InBattleGroundQueueForBattleGroundQueueType(BATTLEGROUND_QUEUE_AV))
    {
        bot->RemoveBattleGroundQueueId(BATTLEGROUND_QUEUE_AV);
        sBattleGroundMgr.m_battleGroundQueues[BATTLEGROUND_QUEUE_AV].RemovePlayer(bot->GetObjectGuid(), true);
        sBattleGroundMgr.ScheduleQueueUpdate(BATTLEGROUND_QUEUE_AV, BATTLEGROUND_AV, bot->GetBattleGroundBracketIdFromLevel(BATTLEGROUND_AV));
    }

    return TestResult::PASS;
}

TestResult CommandAvWaitStart::Execute(const std::string& params, Player* bot,
                    PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    BattleGroundAV* av = GetAvForBot(bot);
    if (!av)
    {
        message = "av wait start: bot is not in an Alterac Valley";
        return TestResult::IMPOSSIBLE;
    }

    if (av->GetStatus() == STATUS_IN_PROGRESS)
    {
        ctx.avJoinStart = 0;
        return TestResult::PASS;
    }

    if (av->GetStatus() == STATUS_WAIT_LEAVE)
    {
        message = "av wait start: the match already ended";
        return TestResult::IMPOSSIBLE;
    }

    if (!ctx.avJoinStart)
        ctx.avJoinStart = WorldTimer::getMSTime();

    if (WorldTimer::getMSTimeDiff(ctx.avJoinStart, WorldTimer::getMSTime()) > 300 * IN_MILLISECONDS)
    {
        ctx.avJoinStart = 0;
        message = "av wait start: Alterac Valley did not start within 300s";
        return TestResult::FAIL;
    }

    message = "Waiting for Alterac Valley gates to open";
    return TestResult::PENDING;
}

bool MonitorAvNode::IsConditionMet(const std::string& monitorStr, Player* bot, TestContext& ctx) const
{
    BattleGroundAV* av = GetAvForBot(bot);
    if (!av)
        return false;

    size_t arrowPos = monitorStr.find("=>");
    std::string condition = monitorStr.substr(0, arrowPos);
    std::vector<std::string> args = Qualified::getMultiQualifiers(condition, " ");
    args.erase(std::remove(args.begin(), args.end(), std::string()), args.end());
    if (args.size() < 2)
        return false;

    uint8 nodeId = 0;
    uint8 owner = 0;
    uint8 state = 0;
    if (!ParseAvNodeName(args[0], nodeId) || !ParseAvOwnerState(args[1], args.size() > 2 ? args[2] : "controlled", owner, state))
        return false;

    return av->GetAvNodeOwner(nodeId) == owner && av->GetAvNodeState(nodeId) == state;
}
