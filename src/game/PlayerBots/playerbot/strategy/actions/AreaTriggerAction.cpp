
#include "playerbot/playerbot.h"
#include "AreaTriggerAction.h"
#include "playerbot/PlayerbotAIConfig.h"

using namespace ai;

bool ReachAreaTriggerAction::Execute(Event& event)
{
    Player* requester = event.getOwner() ? event.getOwner() : GetMaster();
    uint32 triggerId;

    if (ai->IsRealPlayer()) //Do not trigger own area trigger.
        return false;

    WorldPacket p(event.getPacket());
    
    p >> triggerId;

    AreaTriggerEntry const* atEntry = sObjectMgr.GetAreaTrigger(triggerId);
    if(!atEntry)
        return false;

    AreaTriggerEntry const* at = sObjectMgr.GetAreaTrigger(triggerId);
    if (!at)
    {
        WorldPackets::Misc::AreaTrigger atPacket1;
        atPacket1.triggerId = triggerId;
        
        bot->GetSession()->HandleAreaTriggerOpcode(atPacket1);

        return true;
    }

    if (bot->GetMapId() != atEntry->map_id || bot->GetDistance(atEntry->x, atEntry->y, atEntry->z) > sPlayerbotAIConfig.sightDistance)
    {
        ai->TellError(requester, "I won't follow: too far away");
        return true;
    }

    MotionMaster &mm = *bot->GetMotionMaster();
	mm.MovePoint(atEntry->map_id, atEntry->x, atEntry->y, atEntry->z, 0 /* MOVE_RUN_MODE */);
    const float distance = sqrt(bot->GetDistance(atEntry->x, atEntry->y, atEntry->z));
    const float duration = 1000.0f * distance / bot->GetSpeed(MOVE_RUN) + sPlayerbotAIConfig.reactDelay;
    ai->TellError(requester, "Wait for me");
    SetDuration(duration);
    context->GetValue<LastMovement&>("last area trigger")->Get().lastAreaTrigger = triggerId;

    return true;
}



bool AreaTriggerAction::Execute(Event& event)
{
    LastMovement& movement = context->GetValue<LastMovement&>("last area trigger")->Get();

    uint32 triggerId = movement.lastAreaTrigger;
    movement.lastAreaTrigger = 0;

    AreaTriggerEntry const* atEntry = sObjectMgr.GetAreaTrigger(triggerId);
    if(!atEntry)
        return false;

    AreaTriggerEntry const* at = sObjectMgr.GetAreaTrigger(triggerId);
    if (!at)
        return true;

    WorldPackets::Misc::AreaTrigger atPacket;
    atPacket.triggerId = triggerId;
    
    bot->GetSession()->HandleAreaTriggerOpcode(atPacket);
    return true;
}
