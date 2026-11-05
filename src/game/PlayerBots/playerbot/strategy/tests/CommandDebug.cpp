#include "CommandDebug.h"
#include "playerbot/PlayerbotAI.h"

using namespace ai;

TestResult CommandDebug::Execute(const std::string& params, Player* bot, PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    ctx.debug = true;
    return TestResult::PASS;
}

TestResult CommandRecord::Execute(const std::string& params, Player* bot, PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    ai->RecordMessages(true);
    return TestResult::PASS;
}

TestResult CommandRead::Execute(const std::string& params, Player* bot, PlayerbotAI* ai, TestContext& ctx, std::string& message)
{
    for (auto& msg : ai->GetRecordedMessages())
    {
        ai->TellPlayer(bot, std::string("[Recorded Message] ") + msg);
        sLog.Out(LOG_BASIC, LOG_LVL_MINIMAL, "[TestAction] Bot %s outgoing %s", bot->GetName(), msg.c_str());
    }
    return TestResult::PASS;
}