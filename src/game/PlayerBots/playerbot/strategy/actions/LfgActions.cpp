#include "playerbot/playerbot.h"
#include "LfgActions.h"
#include "playerbot/strategy/ItemVisitors.h"
#include "playerbot/TravelMgr.h"

using namespace ai;

bool LfgJoinAction::Execute(Event& event)
{
    return false; /* LFG not fully supported in vmangos vanilla */
}

bool LfgJoinAction::isUseful()
{
    return false;
}

bool LfgJoinAction::JoinLFG()
{
    return false;
}

bool LfgJoinAction::SetRoles()
{
    return false;
}

bool LfgAcceptAction::Execute(Event& event)
{
    return false;
}

bool LfgRoleCheckAction::Execute(Event& event)
{
    return false;
}

bool LfgLeaveAction::Execute(Event& event)
{
    return false;
}

bool LfgLeaveAction::isUseful()
{
    return false;
}

bool LfgTeleportAction::Execute(Event& event)
{
    return false;
}