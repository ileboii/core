#include "playerbot/playerbot.h"
#include "AvTestCommands.h"
#include "AvTestSupport.h"
#include "TestRegistry.h"
#include "BattleGround.h"
#include "BattleGroundAV.h"

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

    uint8 expectedOwner = ownerName == "alliance" ? 0 : ownerName == "horde" ? 1 : ownerName == "neutral" ? 2 : 255;
    uint8 expectedState = stateName == "assaulted" ? 0 : stateName == "controlled" ? 1 : 255;
    if (expectedOwner == 255 || expectedState == 255)
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
