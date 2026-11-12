#pragma once

#include "playerbot/strategy/Action.h"
#include "MovementActions.h"

namespace ai
{
	class FollowAction : public MovementAction {
	public:
		FollowAction(PlayerbotAI* ai, std::string name = "follow") : MovementAction(ai, name) {}
		virtual bool Execute(Event& event) override;
        virtual bool isUseful() override;
        virtual bool CanDeadFollow(Unit* target);

        static bool NeedsLowerFloorPath(PlayerbotAI* ai, Unit* target);
        static bool IsFollowingLowerFloor(PlayerbotAI* ai);

    private:
        bool IsCurrentLowerFloorTarget(Unit* target) const;
        ObjectGuid lowerFloorTarget;
        G3D::Vector3 lowerFloorDestination;
        time_t retryLowerFloorPath = 0;
	};

	class StopFollowAction : public MovementAction {
	public:
		StopFollowAction(PlayerbotAI* ai, std::string name = "stop follow") : MovementAction(ai, name) {}
        virtual bool Execute(Event& event) override;
	};

    class FleeToMasterAction : public FollowAction {
    public:
        FleeToMasterAction(PlayerbotAI* ai) : FollowAction(ai, "flee to master") {}

        virtual bool Execute(Event& event) override;
        virtual bool isUseful() override;
    };

}
