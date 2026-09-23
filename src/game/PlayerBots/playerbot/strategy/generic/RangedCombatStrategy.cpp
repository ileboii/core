
#include "playerbot/playerbot.h"
#include "RangedCombatStrategy.h"

using namespace ai;

void RangedCombatStrategy::InitCombatTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
        "enemy too close for spell",
        NextAction::array(0, new NextAction("flee", static_cast<float>(ACTION_MOVE) + 1.0f), NULL)));
}
