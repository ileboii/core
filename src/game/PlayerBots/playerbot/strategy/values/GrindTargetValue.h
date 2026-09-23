#pragma once
#include "playerbot/strategy/Value.h"
#include "TargetValue.h"

namespace ai
{
   
    class GrindTargetValue : public TargetValue
	{
	public:
        GrindTargetValue(PlayerbotAI* ai, std::string name = "grind target") : TargetValue(ai, name, 6) {}

    public:
        Unit* Calculate() override;
        Unit* Get() override;
        Unit* LazyGet() override;
        void Set(Unit* unit) override;

    private:
        ObjectGuid targetGuid;
        int GetTargetingPlayerCount(Unit* unit);
        Unit* FindTargetForGrinding(int assistCount);
    };
}
