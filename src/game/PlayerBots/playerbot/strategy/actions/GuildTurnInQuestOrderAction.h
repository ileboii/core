
#include "playerbot/strategy/Action.h"
#include "playerbot/strategy/values/GuildValues.h"

namespace ai
{
    class GuildTurnInQuestOrderAction : public Action
    {
    public:
        GuildTurnInQuestOrderAction(PlayerbotAI* ai) : Action(ai, "guild turn in quest order") {}

        bool Execute(Event& event) override;
        bool isUseful() override;
    };
}
