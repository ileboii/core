#pragma once

#include "playerbot/strategy/Action.h"
#include "MovementActions.h"
#include "playerbot/strategy/values/LastMovementValue.h"

namespace ai
{
    class TravelTarget;

    class MoveToTravelTargetAction : public MovementAction {
    public:
        MoveToTravelTargetAction(PlayerbotAI* ai) : MovementAction(ai, "move to travel target") {}

        virtual bool Execute(Event& event) override;
        virtual bool isUseful() override;
        bool MoveMinimally(TravelTarget& target);

    private:
        bool CanTravel(bool minimalMove);
        void UpdateMoveRetries(TravelTarget& target, bool moved);
    };

}
