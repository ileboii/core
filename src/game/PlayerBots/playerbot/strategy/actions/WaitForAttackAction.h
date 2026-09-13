#pragma once
#include "MovementActions.h"
#include "GenericActions.h"

namespace ai
{
   class WaitForAttackKeepSafeDistanceAction : public MovementAction
   {
   public:
       WaitForAttackKeepSafeDistanceAction(PlayerbotAI* ai) : MovementAction(ai, "wait for attack keep safe distance") {}
       virtual bool Execute(Event& event) override;

   private:
       const WorldPosition GetBestPoint(Unit* target, float minDistance, float maxDistance) const;
       bool IsEnemyClose(const WorldPosition& point, const std::list<ObjectGuid>& enemies) const;
       virtual bool isUsefulWhenStunned() override { return true; }
   };

   class KitePositionAction : public MovementAction
   {
   public:
       KitePositionAction(PlayerbotAI* ai, const std::string& actionName = "kite position") : MovementAction(ai, actionName) {}

       bool Execute(Event& event) override;
       bool isUseful() override;

   protected:
       bool IsValidHostile(Unit* unit) const;

       Unit* GetClosestHostile(const std::list<Unit*>& hostiles) const;

       const WorldPosition GetBestPoint(Unit* anchor, const std::list<Unit*>& hostiles, bool outerOnly,
                                        const WorldPosition* referencePosition = nullptr, bool checkPath = true,
                                        Unit* rangeTarget = nullptr) const;

       bool IsSafePoint(const WorldPosition& point, const std::list<Unit*>& hostiles, bool keepCurrentTargetInRange,
                        bool requireCurrentTargetLos, Unit* rangeTarget = nullptr) const;

       bool IsSafePath(const WorldPosition& point, const std::list<Unit*>& hostiles) const;
   };

   class KiteStackPositionAction : public KitePositionAction
   {
   public:
       KiteStackPositionAction(PlayerbotAI* ai) : KitePositionAction(ai, "kite stack position") {}

       bool Execute(Event& event) override;
       bool isUseful() override;
   };
}
