#include "playerbot/playerbot.h"
#include "GuildMeetingTrigger.h"
#include "playerbot/strategy/values/GuildValues.h"

using namespace ai;

bool GuildMeetingTrigger::IsActive()
{
    param = GuildScheduleValue::GetMeetingLocation(ai);
    return !param.empty();
}