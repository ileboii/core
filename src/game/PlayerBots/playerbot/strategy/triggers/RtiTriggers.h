#pragma once
#include "playerbot/strategy/Trigger.h"
#include "playerbot/PlayerbotAIConfig.h"
#include "playerbot/ServerFacade.h"
#include "playerbot/strategy/values/RtiTargetValue.h"

namespace ai
{
    class NoRtiTrigger : public Trigger
    {
    public:
        NoRtiTrigger(PlayerbotAI* ai) : Trigger(ai, "no rti target") {}

        virtual bool IsActive() override
		{
            return !RtiTargetValue::GetRtiIndices(AI_VALUE(std::string, "rti")).empty() &&
                !AI_VALUE(Unit*, "rti target");
        }
    };
}
