#include "KiteStrategy.h"
#include "playerbot/playerbot.h"
#include "playerbot/strategy/actions/GenericSpellActions.h"
#include "playerbot/ServerFacade.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "playerbot/strategy/actions/MovementActions.h"
#include "playerbot/strategy/actions/ReachTargetActions.h"

using namespace ai;

namespace
{
    NextAction** GetKitePositionAction() { return NextAction::array(0, new NextAction("kite position", ACTION_EMERGENCY + 8.0f), NULL); }
    NextAction** GetKiteStackPositionAction() { return NextAction::array(0, new NextAction("kite stack position", ACTION_EMERGENCY + 8.0f), NULL); }
} // namespace

void KiteStrategy::InitCombatTriggers(std::list<TriggerNode*>& triggers) { triggers.push_back(new TriggerNode("kite position", GetKitePositionAction())); }

void KiteStrategy::InitReactionTriggers(std::list<TriggerNode*>& triggers) { triggers.push_back(new TriggerNode("kite position", GetKitePositionAction())); }

void KiteStackStrategy::InitCombatTriggers(std::list<TriggerNode*>& triggers) { triggers.push_back(new TriggerNode("kite stack position", GetKiteStackPositionAction())); }

void KiteStackStrategy::InitReactionTriggers(std::list<TriggerNode*>& triggers) { triggers.push_back(new TriggerNode("kite stack position", GetKiteStackPositionAction())); }

float KiteMeleeMultiplier::GetValue(Action* action)
{
    if (!action || !ai || !ai->GetBot())
        return 1.0f;

    const uint8 botClass = ai->GetBot()->GetClass();

    if (botClass != CLASS_WARRIOR && botClass != CLASS_ROGUE)
    {
        return 1.0f;
    }

    const std::string name = action->getName();

    if (name == "kite position" || name == "kite stack position")
        return 1.0f;

    if (dynamic_cast<CastShootAction*>(action))
        return 1.0f;

    if (ReachTargetAction* reach = dynamic_cast<ReachTargetAction*>(action))
    {
        const std::string spell = reach->GetSpellName();

        if (name == "reach spell" && (spell == "shoot" || spell == "shoot bow" || spell == "shoot gun" || spell == "shoot crossbow" || spell == "throw"))
        {
            return 1.0f;
        }
    }

    if (dynamic_cast<MovementAction*>(action))
        return 0.0f;

    if (dynamic_cast<CastSpellAction*>(action))
        return 0.0f;

    if (name == "melee" || name == "attack" || name == "switch to melee")
    {
        return 0.0f;
    }

    return 1.0f;
}

NextAction** KiteStrategy::GetDefaultCombatActions()
{
    if (ai->GetBot()->GetClass() == CLASS_WARRIOR || ai->GetBot()->GetClass() == CLASS_ROGUE)
    {
        return NextAction::array(0, new NextAction("shoot", ACTION_NORMAL), NULL);
    }

    return nullptr;
}

void KiteStrategy::InitCombatMultipliers(std::list<Multiplier*>& multipliers)
{
    if (ai->GetBot()->GetClass() == CLASS_WARRIOR || ai->GetBot()->GetClass() == CLASS_ROGUE)
    {
        multipliers.push_back(new KiteMeleeMultiplier(ai));
    }
}

std::list<Unit*> KiteStrategy::GetNearbyHostiles(PlayerbotAI* ai)
{
    std::list<Unit*> result;

    if (!ai || !ai->GetBot())
        return result;

    Player* bot = ai->GetBot();
    const float range = GetScanDistance();

    MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot, range);

    MaNGOS::UnitListSearcher<MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck> searcher(result, check);

    Cell::VisitAllObjects(bot, searcher, range);

    result.remove_if([bot](Unit* unit) { return !unit || !unit->IsInWorld() || !unit->IsAlive() || unit->GetMapId() != bot->GetMapId() || !sServerFacade.IsHostileTo(bot, unit); });

    return result;
}
