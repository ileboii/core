
#include "playerbot/playerbot.h"
#include "DeadValues.h"
#include "playerbot/TravelMgr.h"

using namespace ai;

GuidPosition GraveyardValue::Calculate()
{
    WorldPosition refPosition = bot, botPos(bot);

    if (getQualifier() == "master")
    {
        if (ai->GetGroupMaster() && ai->IsSafe(ai->GetGroupMaster()) && ai->GetGroupMaster()->GetMapId() == bot->GetMapId())
        {
            refPosition = ai->GetGroupMaster();
        }
    }
    else if (getQualifier() == "travel")
    {
        auto travelTarget = AI_VALUE(TravelTarget*, "travel target");

        if (travelTarget && travelTarget->GetPosition() && travelTarget->GetPosition()->getMapId() == bot->GetMapId())
        {
            refPosition = *travelTarget->GetPosition();
        }
    }
    else if (getQualifier() == "another closest appropriate")
    {
        //just get ANOTHER nearest appropriate for level (neutral or same team zone)
        if (auto anotherAppropriate = GetAnotherAppropriateClosestGraveyard())
        {
            return GuidPosition(0, anotherAppropriate);
        }
    }

    WorldSafeLocsEntry const* ClosestGrave = sObjectMgr.GetClosestGraveYard(
        refPosition.getX(),
        refPosition.getY(),
        refPosition.getZ(),
        refPosition.getMapId(),
        bot->GetTeam()
    );

    if (!ClosestGrave)
    {
        sLog.Out(LOG_BASIC, LOG_LVL_DETAIL, 
            "ERROR: Unable to find closest graveyard in GraveyardValue, will return GuidPosition() which is 0,0,0 - bot #%d %s:%d <%s>",
            bot->GetGUIDLow(),
            bot->GetTeam() == ALLIANCE ? "A" : "H",
            bot->GetLevel(),
            bot->GetName()
        );
        return GuidPosition();
    }

    return GuidPosition(0, ClosestGrave);
}

WorldSafeLocsEntry const* GraveyardValue::GetAnotherAppropriateClosestGraveyard() const
{
    Corpse* corpse = bot->GetCorpse();
    if (!corpse)
    {
        sLog.Out(LOG_BASIC, LOG_LVL_DETAIL,
            "ERROR: No corpse in GetAnotherAppropriateClosestGraveyard - bot #%d %s:%d <%s>",
            bot->GetGUIDLow(),
            bot->GetTeam() == ALLIANCE ? "A" : "H",
            bot->GetLevel(),
            bot->GetName()
        );
        return nullptr;
    }

    /* GraveyardMap not accessible in vmangos - return nullptr */
    return nullptr;
}

GuidPosition BestGraveyardValue::Calculate()
{
    Corpse* corpse = bot->GetCorpse();
    if (!corpse)
    {
        sLog.Out(LOG_BASIC, LOG_LVL_DETAIL, 
            "ERROR: Unable to find closest graveyard in BestGraveyardValue, will return GuidPosition() which is 0,0,0 - bot #%d %s:%d <%s>",
            bot->GetGUIDLow(),
            bot->GetTeam() == ALLIANCE ? "A" : "H",
            bot->GetLevel(),
            bot->GetName()
        );
        return GuidPosition();
    }

    uint32 deathCount = AI_VALUE(uint32, "death count");

    //attempt to revive at other same map graveyards which are not enemy territory
    if (!ai->HasActivePlayerMaster() && deathCount >= DEATH_COUNT_BEFORE_TRYING_ANOTHER_GRAVEYARD)
    {
        GuidPosition anotherGraveyard = AI_VALUE2(GuidPosition, "graveyard", "another closest appropriate");
        if (anotherGraveyard)
        {
            return anotherGraveyard;
        }
        sLog.Out(LOG_BASIC, LOG_LVL_DETAIL, 
            "ERROR: Unable to find another closest appropriate graveyard in BestGraveyardValue, resorting to self graveyard - bot #%d %s:%d <%s>",
            bot->GetGUIDLow(),
            bot->GetTeam() == ALLIANCE ? "A" : "H",
            bot->GetLevel(),
            bot->GetName()
        );
    }

    //Revive near master.
    if ((ai->HasStrategy("follow", BotState::BOT_STATE_NON_COMBAT) ||
        ai->HasStrategy("wander", BotState::BOT_STATE_NON_COMBAT)) &&
        ai->GetGroupMaster() && ai->GetGroupMaster() != bot)
    {
        GuidPosition masterGraveyard = AI_VALUE2(GuidPosition, "graveyard", "master");
        if (masterGraveyard)
        {
            return masterGraveyard;
        }
        sLog.Out(LOG_BASIC, LOG_LVL_DETAIL, 
            "ERROR: Unable to find master graveyard in BestGraveyardValue, resorting to self graveyard - bot #%d %s:%d <%s>",
            bot->GetGUIDLow(),
            bot->GetTeam() == ALLIANCE ? "A" : "H",
            bot->GetLevel(),
            bot->GetName()
        );
    }

    //Revive near travel target if it's far away from last death.
    if (AI_VALUE2(GuidPosition, "graveyard", "travel") && AI_VALUE2(GuidPosition, "graveyard", "travel").fDist(corpse) > sPlayerbotAIConfig.reactDistance)
    {
        GuidPosition travelGraveyard = AI_VALUE2(GuidPosition, "graveyard", "travel");
        if (travelGraveyard)
        {
            return travelGraveyard;
        }
        sLog.Out(LOG_BASIC, LOG_LVL_DETAIL, 
            "ERROR: Unable to find travel graveyard in BestGraveyardValue, resorting to self graveyard - bot #%d %s:%d <%s>",
            bot->GetGUIDLow(),
            bot->GetTeam() == ALLIANCE ? "A" : "H",
            bot->GetLevel(),
            bot->GetName()
        );
    }

    return AI_VALUE2(GuidPosition, "graveyard", "self");
}

bool ShouldSpiritHealerValue::Calculate()
{
    uint32 deathCount = AI_VALUE(uint32, "death count");
    uint8 durability = AI_VALUE(uint8, "durability");

    if (ai->HasActivePlayerMaster())
        return false;

    Corpse* corpse = bot->GetCorpse();
    if (!corpse)
    {
        return true;
    }

    if (ai->HasAura(SPELL_ID_PASSIVE_RESURRECTION_SICKNESS, bot) || durability < 10)
        return true;

    if (deathCount > DEATH_COUNT_BEFORE_REVIVING_AT_SPIRIT_HEALER)
        return true;

    uint32 deadTime = time(nullptr) - corpse->GetGhostTime();

    if (deadTime > 30 * MINUTE)
        return true;

    return false;
}
