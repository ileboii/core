#include "playerbot/playerbot.h"
#include "playerbot/strategy/Trigger.h"

namespace ai
{
    class GuildMeetingTrigger : public Trigger
    {
    public:
        GuildMeetingTrigger(PlayerbotAI* ai) : Trigger(ai, "guild meeting", 60) {}

        bool IsActive() override;
    };
}