#include "playerbot/playerbot.h"
#include "TestStrategy.h"
#include "TestAction.h"
#include "TestTriggers.h"

using namespace ai;

void TestStrategy::InitNonCombatTriggers(std::list<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode(
        "test ready",
        NextAction::array(0, new NextAction("test", 100.0f), NULL)));
}

void TestStrategy::OnStrategyRemoved(BotState state)
{
    AiObjectContext* context = ai->GetAiObjectContext();
    Action* action = context->GetAction("test");

    if (action)
    {
        action->Reset();
    };
}
