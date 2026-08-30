#include "playerbot/playerbot.h"
#include "BattleGround.h"
#include "BattleGroundAV.h"
#include "BattleGroundTactics.h"
#include "BattleGroundMgr.h"
#include "AcceptQuestAction.h"
#include "TalkToQuestGiverAction.h"

static constexpr uint32 AV_ARMOR_SCRAPS_ITEM = 17422;
static constexpr uint32 AV_ARMOR_SCRAPS_REQUIRED = 20;

static constexpr uint32 AV_IRONDEEP_SUPPLIES_ITEM = 17522;
static constexpr uint32 AV_COLDTOOTH_SUPPLIES_ITEM = 17542;
static constexpr uint32 AV_MINE_SUPPLIES_REQUIRED = 10;

static constexpr uint32 AV_ARMORER_ALLIANCE = 13257;
static constexpr uint32 AV_ARMORER_HORDE = 13176;

static constexpr uint32 AV_STORMPIKE_QUARTERMASTER = 12096;
static constexpr uint32 AV_FROSTWOLF_QUARTERMASTER = 12097;

static Position const AV_ARMORER_POS_ALLIANCE = {647.61f, -61.1548f, 41.7405f, 4.24115f};

static Position const AV_ARMORER_POS_HORDE = {-1251.5f, -316.327f, 62.6565f, 5.02655f};

static std::tuple<uint32, uint32, std::string> AV_HordeAttackObjectives[] =
{
    // Attack
#ifndef MANGOSBOT_TWO  
    { BG_AV_NODES_STONEHEART_BUNKER, BG_AV_NODE_STATUS_ALLY_OCCUPIED, "AV_STONEHEART_BUNKER"},
    { BG_AV_NODES_STONEHEART_GRAVE, BG_AV_NODE_STATUS_ALLY_CONTESTED, "AV_STONEHEART_GRAVEYARD" },
    { BG_AV_NODES_STONEHEART_GRAVE, BG_AV_NODE_STATUS_ALLY_OCCUPIED, "AV_STONEHEART_GRAVEYARD" },
    { BG_AV_NODES_ICEWING_BUNKER, BG_AV_NODE_STATUS_ALLY_OCCUPIED, "AV_ICEWING_BUNKER" },
    { BG_AV_NODES_STORMPIKE_GRAVE, BG_AV_NODE_STATUS_ALLY_CONTESTED, "AV_STORMPIKE_GRAVEYARD" },
    { BG_AV_NODES_STORMPIKE_GRAVE, BG_AV_NODE_STATUS_ALLY_OCCUPIED, "AV_STORMPIKE_GRAVEYARD" },
    { BG_AV_NODES_DUNBALDAR_SOUTH, BG_AV_NODE_STATUS_ALLY_OCCUPIED, "AV_DUNBALDAR_SOUTH" },
    { BG_AV_NODES_DUNBALDAR_NORTH, BG_AV_NODE_STATUS_ALLY_OCCUPIED, "AV_DUNBALDAR_NORTH" },
    { BG_AV_NODES_FIRSTAID_STATION, BG_AV_NODE_STATUS_ALLY_CONTESTED, "AV_STORMPIKE_AID_STATION" },
    { BG_AV_NODES_FIRSTAID_STATION, BG_AV_NODE_STATUS_ALLY_OCCUPIED, "AV_STORMPIKE_AID_STATION" },
#else
    { BG_AV_NODE_STONEHEART_BUNKER, BG_AV_NODE_STATUS_ALLY_OCCUPIED, "AV_STONEHEART_BUNKER" },
    { BG_AV_NODE_GY_STONEHEARTH, BG_AV_NODE_STATUS_ALLY_CONTESTED, "AV_STONEHEART_GRAVEYARD" },
    { BG_AV_NODE_GY_STONEHEARTH, BG_AV_NODE_STATUS_ALLY_OCCUPIED, "AV_STONEHEART_GRAVEYARD" },
    { BG_AV_NODE_ICEWING_BUNKER, BG_AV_NODE_STATUS_ALLY_OCCUPIED, "AV_ICEWING_BUNKER" },
    { BG_AV_NODE_GY_STORMPIKE, BG_AV_NODE_STATUS_ALLY_CONTESTED, "AV_STORMPIKE_GRAVEYARD" },
    { BG_AV_NODE_GY_STORMPIKE, BG_AV_NODE_STATUS_ALLY_OCCUPIED, "AV_STORMPIKE_GRAVEYARD" },
    { BG_AV_NODE_DUNBALDAR_SOUTH, BG_AV_NODE_STATUS_ALLY_OCCUPIED, "AV_DUNBALDAR_SOUTH" },
    { BG_AV_NODE_DUNBALDAR_NORTH, BG_AV_NODE_STATUS_ALLY_OCCUPIED, "AV_DUNBALDAR_NORTH" },
    { BG_AV_NODE_GY_DUN_BALDAR, BG_AV_NODE_STATUS_ALLY_CONTESTED, "AV_STORMPIKE_AID_STATION" },
    { BG_AV_NODE_GY_DUN_BALDAR, BG_AV_NODE_STATUS_ALLY_OCCUPIED, "AV_STORMPIKE_AID_STATION" },
#endif
};

static std::tuple<uint32, uint32, std::string> AV_HordeDefendObjectives[] =
{
    // Defend
#ifndef MANGOSBOT_TWO
    { BG_AV_NODES_FROSTWOLF_GRAVE, BG_AV_NODE_STATUS_ALLY_CONTESTED, "AV_FROSTWOLF_GRAVEYARD" },
    { BG_AV_NODES_FROSTWOLF_GRAVE, BG_AV_NODE_STATUS_ALLY_OCCUPIED, "AV_FROSTWOLF_GRAVEYARD" },
    { BG_AV_NODES_FROSTWOLF_ETOWER, BG_AV_NODE_STATUS_ALLY_CONTESTED, "AV_EAST_FROSTWOLF_TOWER" },
    { BG_AV_NODES_FROSTWOLF_WTOWER, BG_AV_NODE_STATUS_ALLY_CONTESTED, "AV_WEST_FROSTWOLF_TOWER" },
    { BG_AV_NODES_TOWER_POINT, BG_AV_NODE_STATUS_ALLY_CONTESTED, "AV_TOWERPOINT" },
    { BG_AV_NODES_ICEBLOOD_TOWER, BG_AV_NODE_STATUS_ALLY_CONTESTED, "AV_ICEBLOOD_TOWER" },
#else
    { BG_AV_NODE_GY_FROSTWOLF, BG_AV_NODE_STATUS_ALLY_CONTESTED, "AV_FROSTWOLF_GRAVEYARD" },
    { BG_AV_NODE_GY_FROSTWOLF, BG_AV_NODE_STATUS_ALLY_OCCUPIED, "AV_FROSTWOLF_GRAVEYARD" },
    { BG_AV_NODE_FROSTWOLF_EAST, BG_AV_NODE_STATUS_ALLY_CONTESTED, "AV_EAST_FROSTWOLF_TOWER" },
    { BG_AV_NODE_FROSTWOLF_WEST, BG_AV_NODE_STATUS_ALLY_CONTESTED, "AV_WEST_FROSTWOLF_TOWER" },
    { BG_AV_NODE_TOWER_POINT, BG_AV_NODE_STATUS_ALLY_CONTESTED, "AV_TOWERPOINT" },
    { BG_AV_NODE_ICEBLOOD_TOWER, BG_AV_NODE_STATUS_ALLY_CONTESTED, "AV_ICEBLOOD_TOWER" },
#endif
};

static std::tuple<uint32, uint32, std::string> AV_AllianceAttackObjectives[] =
{
    // Attack
#ifndef MANGOSBOT_TWO  
    { BG_AV_NODES_ICEBLOOD_TOWER, BG_AV_NODE_STATUS_HORDE_OCCUPIED, "AV_ICEBLOOD_TOWER" },
    { BG_AV_NODES_ICEBLOOD_GRAVE, BG_AV_NODE_STATUS_HORDE_CONTESTED, "AV_ICEBLOOD_GRAVEYARD" },
    { BG_AV_NODES_ICEBLOOD_GRAVE, BG_AV_NODE_STATUS_HORDE_OCCUPIED, "AV_ICEBLOOD_GRAVEYARD" },
    { BG_AV_NODES_TOWER_POINT, BG_AV_NODE_STATUS_HORDE_OCCUPIED, "AV_TOWERPOINT" },
    { BG_AV_NODES_FROSTWOLF_GRAVE, BG_AV_NODE_STATUS_HORDE_CONTESTED, "AV_FROSTWOLF_GRAVEYARD" },
    { BG_AV_NODES_FROSTWOLF_GRAVE, BG_AV_NODE_STATUS_HORDE_OCCUPIED, "AV_FROSTWOLF_GRAVEYARD" },
    { BG_AV_NODES_FROSTWOLF_ETOWER, BG_AV_NODE_STATUS_HORDE_OCCUPIED, "AV_EAST_FROSTWOLF_TOWER" },
    { BG_AV_NODES_FROSTWOLF_WTOWER, BG_AV_NODE_STATUS_HORDE_OCCUPIED, "AV_WEST_FROSTWOLF_TOWER" },
    { BG_AV_NODES_FROSTWOLF_HUT, BG_AV_NODE_STATUS_HORDE_CONTESTED, "AV_FROSTWOLF_RELIEF_HUT" },
    { BG_AV_NODES_FROSTWOLF_HUT, BG_AV_NODE_STATUS_HORDE_OCCUPIED, "AV_FROSTWOLF_RELIEF_HUT" },
#else
    { BG_AV_NODE_ICEBLOOD_TOWER, BG_AV_NODE_STATUS_HORDE_OCCUPIED, "AV_ICEBLOOD_TOWER" },
    { BG_AV_NODE_GY_ICEBLOOD, BG_AV_NODE_STATUS_HORDE_CONTESTED, "AV_ICEBLOOD_GRAVEYARD" },
    { BG_AV_NODE_GY_ICEBLOOD, BG_AV_NODE_STATUS_HORDE_OCCUPIED, "AV_ICEBLOOD_GRAVEYARD" },
    { BG_AV_NODE_TOWER_POINT, BG_AV_NODE_STATUS_HORDE_OCCUPIED, "AV_TOWERPOINT" },
    { BG_AV_NODE_GY_FROSTWOLF, BG_AV_NODE_STATUS_HORDE_CONTESTED, "AV_FROSTWOLF_GRAVEYARD" },
    { BG_AV_NODE_GY_FROSTWOLF, BG_AV_NODE_STATUS_HORDE_OCCUPIED, "AV_FROSTWOLF_GRAVEYARD" },
    { BG_AV_NODE_FROSTWOLF_EAST, BG_AV_NODE_STATUS_HORDE_OCCUPIED, "AV_EAST_FROSTWOLF_TOWER" },
    { BG_AV_NODE_FROSTWOLF_WEST, BG_AV_NODE_STATUS_HORDE_OCCUPIED, "AV_WEST_FROSTWOLF_TOWER" },
    { BG_AV_NODE_GY_FROSTWOLF_KEEP, BG_AV_NODE_STATUS_HORDE_CONTESTED, "AV_FROSTWOLF_RELIEF_HUT" },
    { BG_AV_NODE_GY_FROSTWOLF_KEEP, BG_AV_NODE_STATUS_HORDE_OCCUPIED, "AV_FROSTWOLF_RELIEF_HUT" },
#endif
};

static std::tuple<uint32, uint32, std::string> AV_AllianceDefendObjectives[] =
{
    // Defend
#ifndef MANGOSBOT_TWO
    { BG_AV_NODES_STORMPIKE_GRAVE, BG_AV_NODE_STATUS_HORDE_CONTESTED, "AV_STORMPIKE_GRAVEYARD" },
    { BG_AV_NODES_STORMPIKE_GRAVE, BG_AV_NODE_STATUS_HORDE_OCCUPIED, "AV_STORMPIKE_GRAVEYARD" },
    { BG_AV_NODES_DUNBALDAR_SOUTH, BG_AV_NODE_STATUS_HORDE_CONTESTED, "AV_DUNBALDAR_SOUTH" },
    { BG_AV_NODES_DUNBALDAR_NORTH, BG_AV_NODE_STATUS_HORDE_CONTESTED, "AV_DUNBALDAR_NORTH" },
    { BG_AV_NODES_ICEWING_BUNKER, BG_AV_NODE_STATUS_HORDE_CONTESTED, "AV_ICEWING_BUNKER" },
    { BG_AV_NODES_STONEHEART_BUNKER, BG_AV_NODE_STATUS_HORDE_CONTESTED, "AV_STONEHEART_BUNKER" },
#else
    { BG_AV_NODE_GY_STORMPIKE, BG_AV_NODE_STATUS_HORDE_CONTESTED, "AV_STORMPIKE_GRAVEYARD" },
    { BG_AV_NODE_GY_STORMPIKE, BG_AV_NODE_STATUS_HORDE_OCCUPIED, "AV_STORMPIKE_GRAVEYARD" },
    { BG_AV_NODE_DUNBALDAR_SOUTH, BG_AV_NODE_STATUS_HORDE_CONTESTED, "AV_DUNBALDAR_SOUTH" },
    { BG_AV_NODE_DUNBALDAR_NORTH, BG_AV_NODE_STATUS_HORDE_CONTESTED, "AV_DUNBALDAR_NORTH" },
    { BG_AV_NODE_ICEWING_BUNKER, BG_AV_NODE_STATUS_HORDE_CONTESTED, "AV_ICEWING_BUNKER" },
    { BG_AV_NODE_STONEHEART_BUNKER, BG_AV_NODE_STATUS_HORDE_CONTESTED, "AV_STONEHEART_BUNKER" },
#endif
};

bool BGTactics::SelectAvObjectiveAlliance(WorldLocation& objectiveLocation)
{
    if (IsAvQuester())
        return SelectAvQuesterObjective(objectiveLocation);

    BattleGround* bg = bot->GetBattleGround();
    if (!bg)
    {
        return false;
    }

    if (ai->IsAvQuester())
        return SelectAvQuesterObjective(objectiveLocation);

    // End boss
#ifndef MANGOSBOT_TWO  
    if (!bg->IsActiveEvent(BG_AV_NODES_ICEBLOOD_TOWER, BG_AV_NODE_STATUS_HORDE_OCCUPIED) &&
        !bg->IsActiveEvent(BG_AV_NODES_TOWER_POINT, BG_AV_NODE_STATUS_HORDE_OCCUPIED) &&
        !bg->IsActiveEvent(BG_AV_NODES_FROSTWOLF_ETOWER, BG_AV_NODE_STATUS_HORDE_OCCUPIED) &&
        !bg->IsActiveEvent(BG_AV_NODES_FROSTWOLF_WTOWER, BG_AV_NODE_STATUS_HORDE_OCCUPIED) &&
        !bg->IsActiveEvent(BG_AV_NODES_FROSTWOLF_HUT, BG_AV_NODE_STATUS_HORDE_OCCUPIED))
#else
    if (!bg->IsActiveEvent(BG_AV_NODE_ICEBLOOD_TOWER, BG_AV_NODE_STATUS_HORDE_OCCUPIED) &&
        !bg->IsActiveEvent(BG_AV_NODE_TOWER_POINT, BG_AV_NODE_STATUS_HORDE_OCCUPIED) &&
        !bg->IsActiveEvent(BG_AV_NODE_FROSTWOLF_EAST, BG_AV_NODE_STATUS_HORDE_OCCUPIED) &&
        !bg->IsActiveEvent(BG_AV_NODE_FROSTWOLF_WEST, BG_AV_NODE_STATUS_HORDE_OCCUPIED) &&
        !bg->IsActiveEvent(BG_AV_NODE_GY_FROSTWOLF_KEEP, BG_AV_NODE_STATUS_HORDE_OCCUPIED))
#endif
    {
        if (Creature* pDrek = bot->GetMap()->GetCreature(bg->GetSingleCreatureGuid(BG_AV_BOSS_H, 0)))
        {
            objectiveLocation = WorldLocation(pDrek->GetMapId(), pDrek->GetPositionX(), pDrek->GetPositionY(), pDrek->GetPositionZ(), pDrek->GetOrientation());
            return true;
        }
    }

    bool strifeTime = bg->GetStartTime() < (uint32)(10 * MINUTE * IN_MILLISECONDS);
    uint32 role = context->GetValue<uint32>("bg role")->Get();
    bool supporter = role < (uint32)(strifeTime ? 4 : 2);

    // Only go to Snowfall Graveyard if already close to it.
#ifndef MANGOSBOT_TWO  
    if (supporter && (bg->IsActiveEvent(BG_AV_NODES_SNOWFALL_GRAVE, BG_AV_NODE_STATUS_HORDE_CONTESTED) || bg->IsActiveEvent(BG_AV_NODES_SNOWFALL_GRAVE, BG_AV_NODE_STATUS_HORDE_OCCUPIED) || bg->IsActiveEvent(BG_AV_NODES_SNOWFALL_GRAVE, NEUTRAL_CONTROLLED)))
    {
#else
    if (supporter && (bg->IsActiveEvent(BG_AV_NODE_GY_SNOWFALL, BG_AV_NODE_STATUS_HORDE_CONTESTED) || bg->IsActiveEvent(BG_AV_NODE_GY_SNOWFALL, BG_AV_NODE_STATUS_HORDE_OCCUPIED) || bg->IsActiveEvent(BG_AV_NODE_GY_SNOWFALL, NEUTRAL_CONTROLLED)))
    {
#endif
        if (WorldLocation snowfallGraveyard; sRandomPlayerbotMgr.GetNamedLocation("AV_SNOWFALL_GRAVEYARD", snowfallGraveyard))
        {
            if (WorldPosition(bot).IsWithinDist(WorldPosition(snowfallGraveyard), VISIBILITY_DISTANCE_LARGE))
            {
                objectiveLocation = snowfallGraveyard;
                return true;
            }
        }
    }

    // Galv
    if (!supporter && !bg->IsActiveEvent(BG_AV_NodeEventCaptainDead_A, 0))
    {
        if (Creature* pGalvangar = bot->GetMap()->GetCreature(bg->GetSingleCreatureGuid(BG_AV_CAPTAIN_H, 0)))
        {
            if (pGalvangar->GetHealth() > 0)
            {
                if (WorldLocation icebloodGarrison; sRandomPlayerbotMgr.GetNamedLocation("AV_ICEBLOOD_GARRISON_WAITING_ALLIANCE", icebloodGarrison))
                {
                    uint32 attackCount = getDefendersCount(Position(icebloodGarrison.x, icebloodGarrison.y, icebloodGarrison.z, icebloodGarrison.o), 10.0f, false);

                    // Prepare to attack Captain
                    if (attackCount < 5 && !sServerFacade.IsInCombat(pGalvangar))
                    {
                        objectiveLocation = icebloodGarrison;
                    }
                    else
                    {
                        objectiveLocation = WorldLocation(pGalvangar->GetMapId(), pGalvangar->GetPositionX(), pGalvangar->GetPositionY(), pGalvangar->GetPositionZ(), pGalvangar->GetOrientation());
                    }

                    return true;
                }
            }
        }
    }

    // Chance to defend
    if (supporter)
    {
        std::vector<WorldLocation> objectiveLocations;

        for (auto const& [nodeId, nodeStatus, locationName] : AV_AllianceDefendObjectives)
        {
            if (!bg->IsActiveEvent(nodeId, nodeStatus))
            {
                continue;
            }

            if (WorldLocation location; sRandomPlayerbotMgr.GetNamedLocation(locationName, location))
            {
                objectiveLocations.push_back(location);
            }
        }

        if (!objectiveLocations.empty())
        {
            objectiveLocation = objectiveLocations[urand(0, objectiveLocations.size() - 1)];
            return true;
        }
    }

    // Mine capture
    if (!supporter && (bg->IsActiveEvent(BG_AV_MINE_BOSSES_SOUTH, 1) || bg->IsActiveEvent(BG_AV_MINE_BOSSES_SOUTH, 2)))
    {
        if (Creature* neutralMineBoss = bot->GetMap()->GetCreature(bg->GetSingleCreatureGuid(BG_AV_MINE_BOSSES_SOUTH, 2)))
        {
            if (bot->IsWithinDist(neutralMineBoss, VISIBILITY_DISTANCE_LARGE) && neutralMineBoss->GetDeathState() != DEAD && bg->IsActiveEvent(BG_AV_MINE_BOSSES_SOUTH, 2))
            {
                objectiveLocation = WorldLocation(neutralMineBoss->GetMapId(), neutralMineBoss->GetPositionX(), neutralMineBoss->GetPositionY(), neutralMineBoss->GetPositionZ(), neutralMineBoss->GetOrientation());
                return true;
            }
        }

        if (Creature* hordeMineBoss = bot->GetMap()->GetCreature(bg->GetSingleCreatureGuid(BG_AV_MINE_BOSSES_SOUTH, 1)))
        {
            if (bot->IsWithinDist(hordeMineBoss, VISIBILITY_DISTANCE_LARGE) && hordeMineBoss->GetDeathState() != DEAD && bg->IsActiveEvent(BG_AV_MINE_BOSSES_SOUTH, 1))
            {
                objectiveLocation = WorldLocation(hordeMineBoss->GetMapId(), hordeMineBoss->GetPositionX(), hordeMineBoss->GetPositionY(), hordeMineBoss->GetPositionZ(), hordeMineBoss->GetOrientation());
                return true;
            }
        }
    }

    // Block without condition
    {
        std::vector<WorldLocation> objectiveLocations;

        for (auto const& [nodeId, nodeStatus, locationName] : AV_AllianceAttackObjectives)
        {
            if (!bg->IsActiveEvent(nodeId, nodeStatus))
            {
                continue;
            }

            // Split team to capture 2 towers at same time
#ifndef MANGOSBOT_TWO  
            if (urand(0, 1) && nodeId == BG_AV_NODES_FROSTWOLF_ETOWER && bg->IsActiveEvent(BG_AV_NODES_FROSTWOLF_WTOWER, BG_AV_NODE_STATUS_HORDE_OCCUPIED))
#else
            if (urand(0, 1) && nodeId == BG_AV_NODE_FROSTWOLF_EAST && bg->IsActiveEvent(BG_AV_NODE_FROSTWOLF_WEST, BG_AV_NODE_STATUS_HORDE_OCCUPIED))
#endif
                continue;

            if (WorldLocation location; sRandomPlayerbotMgr.GetNamedLocation(locationName, location))
            {
                objectiveLocations.push_back(location);
            }
        }

        if (!objectiveLocations.empty())
        {
            objectiveLocation = objectiveLocations[urand(0, objectiveLocations.size() - 1)];
            return true;
        }
    }

    return false;
}

bool BGTactics::SelectAvObjectiveHorde(WorldLocation& objectiveLocation)
{
    if (IsAvQuester())
        return SelectAvQuesterObjective(objectiveLocation);

    BattleGround* bg = bot->GetBattleGround();
    if (!bg)
    {
        return false;
    }

    if (ai->IsAvQuester())
        return SelectAvQuesterObjective(objectiveLocation);

    // End Boss
#ifndef MANGOSBOT_TWO  
    if (!bg->IsActiveEvent(BG_AV_NODES_DUNBALDAR_SOUTH, BG_AV_NODE_STATUS_ALLY_OCCUPIED) &&
        !bg->IsActiveEvent(BG_AV_NODES_DUNBALDAR_NORTH, BG_AV_NODE_STATUS_ALLY_OCCUPIED) &&
        !bg->IsActiveEvent(BG_AV_NODES_ICEWING_BUNKER, BG_AV_NODE_STATUS_ALLY_OCCUPIED) &&
        !bg->IsActiveEvent(BG_AV_NODES_STONEHEART_BUNKER, BG_AV_NODE_STATUS_ALLY_OCCUPIED) &&
        !bg->IsActiveEvent(BG_AV_NODES_FIRSTAID_STATION, BG_AV_NODE_STATUS_ALLY_OCCUPIED))
#else
    if (!bg->IsActiveEvent(BG_AV_NODE_DUNBALDAR_SOUTH, BG_AV_NODE_STATUS_ALLY_OCCUPIED) &&
        !bg->IsActiveEvent(BG_AV_NODE_DUNBALDAR_NORTH, BG_AV_NODE_STATUS_ALLY_OCCUPIED) &&
        !bg->IsActiveEvent(BG_AV_NODE_ICEWING_BUNKER, BG_AV_NODE_STATUS_ALLY_OCCUPIED) &&
        !bg->IsActiveEvent(BG_AV_NODE_STONEHEART_BUNKER, BG_AV_NODE_STATUS_ALLY_OCCUPIED) &&
        !bg->IsActiveEvent(BG_AV_NODE_GY_DUN_BALDAR, BG_AV_NODE_STATUS_ALLY_OCCUPIED))
#endif
    {
        if (Creature* pVanndar = bot->GetMap()->GetCreature(bg->GetSingleCreatureGuid(BG_AV_BOSS_A, 0)))
        {
            objectiveLocation = WorldLocation(pVanndar->GetMapId(), pVanndar->GetPositionX(), pVanndar->GetPositionY(), pVanndar->GetPositionZ(), pVanndar->GetOrientation());
            return true;
        }
    }

    bool strifeTime = bg->GetStartTime() < (uint32)(10 * MINUTE * IN_MILLISECONDS);
    uint32 role = context->GetValue<uint32>("bg role")->Get();
    bool supporter = role < (uint32)(strifeTime ? 4 : 2); // first bunker strike team

    // Only go to Snowfall Graveyard if already close to it.
#ifndef MANGOSBOT_TWO  
    if (supporter && (bg->IsActiveEvent(BG_AV_NODES_SNOWFALL_GRAVE, BG_AV_NODE_STATUS_ALLY_CONTESTED) || bg->IsActiveEvent(BG_AV_NODES_SNOWFALL_GRAVE, BG_AV_NODE_STATUS_ALLY_OCCUPIED) || bg->IsActiveEvent(BG_AV_NODES_SNOWFALL_GRAVE, NEUTRAL_CONTROLLED)))
    {
#else
    if (supporter && (bg->IsActiveEvent(BG_AV_NODE_GY_SNOWFALL, BG_AV_NODE_STATUS_ALLY_CONTESTED) || bg->IsActiveEvent(BG_AV_NODE_GY_SNOWFALL, BG_AV_NODE_STATUS_ALLY_OCCUPIED) || bg->IsActiveEvent(BG_AV_NODE_GY_SNOWFALL, NEUTRAL_CONTROLLED)))
    {
#endif
        if (WorldLocation snowfallGraveyard; sRandomPlayerbotMgr.GetNamedLocation("AV_SNOWFALL_GRAVEYARD", snowfallGraveyard))
        {
            if (WorldPosition(bot).IsWithinDist(WorldPosition(snowfallGraveyard), VISIBILITY_DISTANCE_LARGE))
            {
                objectiveLocation = snowfallGraveyard;
                return true;
            }
        }
    }

    // Balinda
    if (!supporter && !bg->IsActiveEvent(BG_AV_NodeEventCaptainDead_H, 0))
    {
        if (Creature* pBalinda = bot->GetMap()->GetCreature(bg->GetSingleCreatureGuid(BG_AV_CAPTAIN_A, 0)))
        {
            if (pBalinda->GetHealth() > 0)
            {
                if (WorldLocation stoneheartOutpost; sRandomPlayerbotMgr.GetNamedLocation("AV_STONEHEART_OUTPOST_WAITING_HORDE", stoneheartOutpost))
                {
                    uint32 attackCount = getDefendersCount(Position(stoneheartOutpost.x, stoneheartOutpost.y, stoneheartOutpost.z, stoneheartOutpost.o), 10.0f, false);

                    // Prepare to attack Captain
                    if (attackCount < 5 && !sServerFacade.IsInCombat(pBalinda))
                    {
                        objectiveLocation = stoneheartOutpost;
                    }
                    else
                    {
                        objectiveLocation = WorldLocation(pBalinda->GetMapId(), pBalinda->GetPositionX(), pBalinda->GetPositionY(), pBalinda->GetPositionZ(), pBalinda->GetOrientation());
                    }

                    return true;
                }
            }
        }
    }

    // Chance to defend
    if (supporter)
    {
        std::vector<WorldLocation> objectiveLocations;

        for (auto const& [nodeId, nodeStatus, locationName] : AV_HordeDefendObjectives)
        {
            if (!bg->IsActiveEvent(nodeId, nodeStatus))
            {
                continue;
            }

            if (WorldLocation location; sRandomPlayerbotMgr.GetNamedLocation(locationName, location))
            {
                objectiveLocations.push_back(location);
            }
        }

        if (!objectiveLocations.empty())
        {
            objectiveLocation = objectiveLocations[urand(0, objectiveLocations.size() - 1)];
            return true;
        }
    }

    // Mine capture (need paths & script fix)
#ifndef MANGOSBOT_TWO  
    if (!supporter && (bg->IsActiveEvent(BG_AV_MINE_BOSSES_NORTH, 0) || bg->IsActiveEvent(BG_AV_MINE_BOSSES_NORTH, 2)) &&
        !bg->IsActiveEvent(BG_AV_NODES_STORMPIKE_GRAVE, BG_AV_NODE_STATUS_ALLY_OCCUPIED))
#else
    if (!supporter && (bg->IsActiveEvent(BG_AV_MINE_BOSSES_NORTH, 0) || bg->IsActiveEvent(BG_AV_MINE_BOSSES_NORTH, 2)) &&
        !bg->IsActiveEvent(BG_AV_NODE_GY_STORMPIKE, BG_AV_NODE_STATUS_ALLY_OCCUPIED))
#endif
    {
        if (Creature* neutralMineBoss = bot->GetMap()->GetCreature(bg->GetSingleCreatureGuid(BG_AV_MINE_BOSSES_NORTH, 2)))
        {
            if (bot->IsWithinDist(neutralMineBoss, VISIBILITY_DISTANCE_GIGANTIC) && neutralMineBoss->GetDeathState() != DEAD && bg->IsActiveEvent(BG_AV_MINE_BOSSES_NORTH, 2))
            {
                objectiveLocation = WorldLocation(neutralMineBoss->GetMapId(), neutralMineBoss->GetPositionX(), neutralMineBoss->GetPositionY(), neutralMineBoss->GetPositionZ(), neutralMineBoss->GetOrientation());
                return true;
            }
        }

        if (Creature* allianceMineBoss = bot->GetMap()->GetCreature(bg->GetSingleCreatureGuid(BG_AV_MINE_BOSSES_NORTH, 0)))
        {
            if (bot->IsWithinDist(allianceMineBoss, VISIBILITY_DISTANCE_GIGANTIC) && allianceMineBoss->GetDeathState() != DEAD && bg->IsActiveEvent(BG_AV_MINE_BOSSES_NORTH, 0))
            {
                objectiveLocation = WorldLocation(allianceMineBoss->GetMapId(), allianceMineBoss->GetPositionX(), allianceMineBoss->GetPositionY(), allianceMineBoss->GetPositionZ(), allianceMineBoss->GetOrientation());
                return true;
            }
        }
    }

    // Block without condition
    {
        std::vector<WorldLocation> objectiveLocations;

        for (auto const& [nodeId, nodeStatus, locationName] : AV_HordeAttackObjectives)
        {
            if (!bg->IsActiveEvent(nodeId, nodeStatus))
            {
                continue;
            }

            // Split team to capture 2 towers at same time
#ifndef MANGOSBOT_TWO  
            if (urand(0, 1) && nodeId == BG_AV_NODES_DUNBALDAR_SOUTH && bg->IsActiveEvent(BG_AV_NODES_DUNBALDAR_NORTH, BG_AV_NODE_STATUS_ALLY_OCCUPIED))
#else
            if (urand(0, 1) && nodeId == BG_AV_NODE_DUNBALDAR_SOUTH && bg->IsActiveEvent(BG_AV_NODE_DUNBALDAR_NORTH, BG_AV_NODE_STATUS_ALLY_OCCUPIED))
#endif
                continue;

            if (WorldLocation location; sRandomPlayerbotMgr.GetNamedLocation(locationName, location))
            {
                objectiveLocations.push_back(location);
            }
        }

        if (!objectiveLocations.empty())
        {
            objectiveLocation = objectiveLocations[urand(0, objectiveLocations.size() - 1)];
            return true;
        }
    }

    return false;
}

std::vector<uint32> const FlagEntries =
{
    178925,
    178940,
    178943,
    178932,
    178365,
    179286,
    179310,
    179308,
    180418,
};

bool BGTactics::CheckFlagAv()
{
    BattleGround* bg = bot->GetBattleGround();
    if (!bg)
    {
        return false;
    }

    if (IsAvQuester())
    {
        if (TurnInAvMineSupplies())
            return true;

        if (LootAvMineSupplies())
            return true;

        return false;
    }

    BattleGroundTypeId bgType = bg->GetTypeID();
#ifdef MANGOSBOT_TWO
    if (bgType == BATTLEGROUND_RB)
        bgType = bg->GetTypeId(true);
#endif

    if (bgType != BATTLEGROUND_AV)
    {
        return false;
    }

for (auto closeGameObjectGuid : (*context->GetValue<std::list<ObjectGuid>>("closest game objects static los")).Get())
    {
        GameObject* go = ai->GetGameObject(closeGameObjectGuid);
        if (!go)
            continue;

        std::vector<uint32>::const_iterator f = std::find(FlagEntries.begin(), FlagEntries.end(), go->GetEntry());
        if (f == FlagEntries.end())
            continue;

        auto eventIndex = sBattleGroundMgr.GetGameObjectEventIndex(go->GetGUIDLow());

        if (eventIndex.event1 >= BG_AV_NODES_MAX)
            continue;

        BattleGroundAVTeamIndex flagTeam = BattleGroundAVTeamIndex(eventIndex.event2 / BG_AV_MAX_STATES);

        BattleGroundAVTeamIndex botTeam = BattleGroundAV::GetAVTeamIndexByTeamId(bot->GetTeam());

        // Do not try to capture our own banner.
        if (flagTeam == botTeam)
            continue;

        if (!sServerFacade.isSpawned(go) || go->GetGoState() != GO_STATE_READY)
            continue;

        if (!bot->IsWithinDistInMap(go, INTERACTION_DISTANCE))
            continue;

        /*
         * Only one nearby playerbot should capture this banner.
         *
         * Every eligible bot sees the same nearby bots and the lowest GUID
         * wins. Everyone else keeps fighting.
         */
        uint32 capturerGuid = bot->GetGUIDLow();

        for (auto closePlayerGuid : (*context->GetValue<std::list<ObjectGuid>>("closest friendly players")).Get())
        {
            Unit* friendly = ai->GetUnit(closePlayerGuid);
            if (!friendly || !friendly->IsPlayer())
                continue;

            Player* friendlyPlayer = (Player*)friendly;

            if (!friendlyPlayer->IsAlive())
                continue;

            // Only playerbots participate in the election.
            PlayerbotAI* friendlyAI = friendlyPlayer->GetPlayerbotAI();
            if (!friendlyAI || friendlyAI->IsRealPlayer())
                continue;

            // Only bots close enough to actually capture THIS banner count.
            if (!friendlyPlayer->IsWithinDistInMap(go, INTERACTION_DISTANCE))
                continue;

            if (friendlyPlayer->GetGUIDLow() < capturerGuid)
                capturerGuid = friendlyPlayer->GetGUIDLow();
        }

        // Another nearby bot was elected to capture it.
        // Keep fighting instead.
        if (capturerGuid != bot->GetGUIDLow())
            continue;

        if (bot->IsMounted())
            bot->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

        if (bot->IsInDisallowedMountForm())
            bot->RemoveSpellsCausingAura(SPELL_AURA_MOD_SHAPESHIFT);

        // Capture banner.
        ai->StopMoving();

        SpellEntry const* spellInfo = sServerFacade.LookupSpellInfo(SPELL_CAPTURE_BANNER);
        if (!spellInfo)
            return false;

        Spell* spell = new Spell(bot, spellInfo, false);
        spell->m_targets.setGOTarget(go);
        spell->prepare(spell->m_targets);
        ai->WaitForSpellCast(spell);

        resetObjective();

        return true;
    }

    return false;
}

bool BGTactics::IsAvQuester()
{
    BattleGround* bg = bot->GetBattleGround();
    if (!bg || bg->GetTypeID() != BATTLEGROUND_AV)
        return false;

    if (bot->GetLevel() < 51 || bot->GetLevel() > 59)
        return false;

    return (bot->GetGUIDLow() % 5) == 0;
}

bool BGTactics::SelectAvQuesterObjective(WorldLocation& objectiveLocation)
{
    BattleGround* bg = bot->GetBattleGround();
    if (!bg || !ai->IsAvQuester())
        return false;

    if (SelectAvMineSupplyTurnInObjective(objectiveLocation))
        return true;

    if (SelectAvMineSupplyObjective(objectiveLocation))
        return true;

    if (AvQuesterNeedsArmorer())
    {
        Position const& armorerPos = bot->GetTeam() == ALLIANCE ? AV_ARMORER_POS_ALLIANCE : AV_ARMORER_POS_HORDE;

        objectiveLocation = WorldLocation(bot->GetMapId(), armorerPos.x, armorerPos.y, armorerPos.z, armorerPos.o);

        return true;
    }

    std::vector<WorldLocation> farmLocations;

    auto addLocation = [&](char const* name)
    {
        WorldLocation location;

        if (sRandomPlayerbotMgr.GetNamedLocation(name, location))
            farmLocations.push_back(location);
    };

    if (bot->GetTeam() == ALLIANCE)
    {
#ifndef MANGOSBOT_TWO
        if (bg->IsActiveEvent(BG_AV_NODES_ICEBLOOD_GRAVE, BG_AV_NODE_STATUS_HORDE_OCCUPIED))
        {
            addLocation("AV_ICEBLOOD_GRAVEYARD");
        }

        if (bg->IsActiveEvent(BG_AV_NODES_FROSTWOLF_GRAVE, BG_AV_NODE_STATUS_HORDE_OCCUPIED))
        {
            addLocation("AV_FROSTWOLF_GRAVEYARD");
        }

        if (bg->IsActiveEvent(BG_AV_NODES_FROSTWOLF_HUT, BG_AV_NODE_STATUS_HORDE_OCCUPIED))
        {
            addLocation("AV_FROSTWOLF_RELIEF_HUT");
        }
#else
        if (bg->IsActiveEvent(BG_AV_NODE_GY_ICEBLOOD, BG_AV_NODE_STATUS_HORDE_OCCUPIED))
        {
            addLocation("AV_ICEBLOOD_GRAVEYARD");
        }

        if (bg->IsActiveEvent(BG_AV_NODE_GY_FROSTWOLF, BG_AV_NODE_STATUS_HORDE_OCCUPIED))
        {
            addLocation("AV_FROSTWOLF_GRAVEYARD");
        }

        if (bg->IsActiveEvent(BG_AV_NODE_GY_FROSTWOLF_KEEP, BG_AV_NODE_STATUS_HORDE_OCCUPIED))
        {
            addLocation("AV_FROSTWOLF_RELIEF_HUT");
        }
#endif

        // Fallback if all three happen to be contested/captured.
        if (farmLocations.empty())
            addLocation("AV_ICEBLOOD_GRAVEYARD");
    }
    else
    {
#ifndef MANGOSBOT_TWO
        if (bg->IsActiveEvent(BG_AV_NODES_STONEHEART_GRAVE, BG_AV_NODE_STATUS_ALLY_OCCUPIED))
        {
            addLocation("AV_STONEHEART_GRAVEYARD");
        }

        if (bg->IsActiveEvent(BG_AV_NODES_STORMPIKE_GRAVE, BG_AV_NODE_STATUS_ALLY_OCCUPIED))
        {
            addLocation("AV_STORMPIKE_GRAVEYARD");
        }

        if (bg->IsActiveEvent(BG_AV_NODES_FIRSTAID_STATION, BG_AV_NODE_STATUS_ALLY_OCCUPIED))
        {
            addLocation("AV_STORMPIKE_AID_STATION");
        }
#else
        if (bg->IsActiveEvent(BG_AV_NODE_GY_STONEHEARTH, BG_AV_NODE_STATUS_ALLY_OCCUPIED))
        {
            addLocation("AV_STONEHEART_GRAVEYARD");
        }

        if (bg->IsActiveEvent(BG_AV_NODE_GY_STORMPIKE, BG_AV_NODE_STATUS_ALLY_OCCUPIED))
        {
            addLocation("AV_STORMPIKE_GRAVEYARD");
        }

        if (bg->IsActiveEvent(BG_AV_NODE_GY_DUN_BALDAR, BG_AV_NODE_STATUS_ALLY_OCCUPIED))
        {
            addLocation("AV_STORMPIKE_AID_STATION");
        }
#endif

        if (farmLocations.empty())
            addLocation("AV_STONEHEART_GRAVEYARD");
    }

    if (farmLocations.empty())
        return false;

    objectiveLocation = farmLocations[bot->GetGUIDLow() % farmLocations.size()];

    return true;
}

bool BGTactics::AvQuesterNeedsArmorer()
{
    if (!ai->IsAvQuester())
        return false;

    uint32 firstQuest = bot->GetTeam() == ALLIANCE ? BG_AV_QUEST_A_SCRAPS1 : BG_AV_QUEST_H_SCRAPS1;

    uint32 repeatQuest = bot->GetTeam() == ALLIANCE ? BG_AV_QUEST_A_SCRAPS2 : BG_AV_QUEST_H_SCRAPS2;

    QuestStatus firstStatus = bot->GetQuestStatus(firstQuest);
    QuestStatus repeatStatus = bot->GetQuestStatus(repeatQuest);

    if (firstStatus == QUEST_STATUS_COMPLETE && !bot->GetQuestRewardStatus(firstQuest))
    {
        return true;
    }

    if (repeatStatus == QUEST_STATUS_COMPLETE)
        return true;

    bool firstActive = firstStatus == QUEST_STATUS_INCOMPLETE;
    bool repeatActive = repeatStatus == QUEST_STATUS_INCOMPLETE;

    if ((firstActive || repeatActive) && bot->GetItemCount(AV_ARMOR_SCRAPS_ITEM) >= AV_ARMOR_SCRAPS_REQUIRED)
    {
        return true;
    }

    if (!firstActive && !repeatActive)
        return true;

    return false;
}

bool BGTactics::IsAvQuesterArmorerObjective(ai::PositionEntry const& pos)
{
    if (!ai->IsAvQuester() || !pos.isSet())
        return false;

    Position const& armorerPos = bot->GetTeam() == ALLIANCE ? AV_ARMORER_POS_ALLIANCE : AV_ARMORER_POS_HORDE;

    float dx = pos.x - armorerPos.x;
    float dy = pos.y - armorerPos.y;
    float dz = pos.z - armorerPos.z;

    return (dx * dx + dy * dy + dz * dz) < 100.0f;
}

bool BGTactics::HandleAvQuesterArmorer()
{
    if (!ai->IsAvQuester())
        return false;

    uint32 armorerEntry = bot->GetTeam() == ALLIANCE ? AV_ARMORER_ALLIANCE : AV_ARMORER_HORDE;

    Creature* armorer = bot->FindNearestCreature(armorerEntry, 60.0f, true);

    if (!armorer)
        return false;

    if (!bot->IsWithinDistInMap(armorer, INTERACTION_DISTANCE))
    {
        return MoveNear(bot->GetMapId(), armorer->GetPositionX(), armorer->GetPositionY(), armorer->GetPositionZ(), 2.0f);
    }

    if (bot->IsMounted())
        bot->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

    ai->StopMoving();

    uint32 firstQuest = bot->GetTeam() == ALLIANCE ? BG_AV_QUEST_A_SCRAPS1 : BG_AV_QUEST_H_SCRAPS1;

    uint32 repeatQuest = bot->GetTeam() == ALLIANCE ? BG_AV_QUEST_A_SCRAPS2 : BG_AV_QUEST_H_SCRAPS2;

    if (bot->GetQuestStatus(firstQuest) == QUEST_STATUS_INCOMPLETE && bot->CanCompleteQuest(firstQuest))
    {
        bot->CompleteQuest(firstQuest);
    }

    if (bot->GetQuestStatus(repeatQuest) == QUEST_STATUS_INCOMPLETE && bot->CanCompleteQuest(repeatQuest))
    {
        bot->CompleteQuest(repeatQuest);
    }

    Event talkEvent("av armor scraps turnin", armorer->GetObjectGuid());

    TalkToQuestGiverAction talk(ai);
    talk.Execute(talkEvent);

    Event acceptEvent("av armor scraps accept", armorer->GetObjectGuid());

    AcceptAllQuestsAction accept(ai);
    accept.Execute(acceptEvent);

    return true;
}

bool BGTactics::LootAvMineSupplies()
{
    BattleGround* bg = bot->GetBattleGround();
    if (!bg || bg->GetTypeID() != BATTLEGROUND_AV)
        return false;

    BattleGroundAV* av = static_cast<BattleGroundAV*>(bg);

    for (auto closeGameObjectGuid : (*context->GetValue<std::list<ObjectGuid>>("closest game objects static los")).Get())
    {
        GameObject* go = ai->GetGameObject(closeGameObjectGuid);
        if (!go)
            continue;

        uint32 itemId = 0;
        uint32 questId = 0;

        switch (go->GetEntry())
        {
        case BG_AV_OBJECTID_MINE_N:
            itemId = AV_IRONDEEP_SUPPLIES_ITEM;

            questId = bot->GetTeam() == ALLIANCE ? BG_AV_QUEST_A_NEAR_MINE : BG_AV_QUEST_H_OTHER_MINE;
            break;

        case BG_AV_OBJECTID_MINE_S:
            itemId = AV_COLDTOOTH_SUPPLIES_ITEM;

            questId = bot->GetTeam() == ALLIANCE ? BG_AV_QUEST_A_OTHER_MINE : BG_AV_QUEST_H_NEAR_MINE;
            break;

        default:
            continue;
        }

        if (bot->GetQuestStatus(questId) != QUEST_STATUS_INCOMPLETE)
            continue;

        if (bot->HasItemCount(itemId, AV_MINE_SUPPLIES_REQUIRED))
            continue;

        if (!av->PlayerCanDoMineQuest(go->GetEntry(), bot->GetTeam()))
            continue;

        if (!sServerFacade.isSpawned(go) || go->GetGoState() != GO_STATE_READY)
            continue;

        if (!bot->IsWithinDistInMap(go, INTERACTION_DISTANCE))
            continue;

        if (bot->IsMounted())
            bot->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

        if (bot->IsInDisallowedMountForm())
            bot->RemoveSpellsCausingAura(SPELL_AURA_MOD_SHAPESHIFT);

        ai->StopMoving();

        SET_AI_VALUE(LootObject, "loot target", LootObject(bot, go->GetObjectGuid()));

        Event event("av mine supplies", go->GetObjectGuid(), bot);

        return ai->DoSpecificAction("open loot", event, true);
    }

    return false;
}

bool BGTactics::SelectAvMineSupplyObjective(WorldLocation& objectiveLocation)
{
    BattleGround* bg = bot->GetBattleGround();
    if (!bg || bg->GetTypeID() != BATTLEGROUND_AV)
        return false;

    BattleGroundAV* av = static_cast<BattleGroundAV*>(bg);

    uint32 northQuestId = bot->GetTeam() == ALLIANCE ? BG_AV_QUEST_A_NEAR_MINE : BG_AV_QUEST_H_OTHER_MINE;

    uint32 southQuestId = bot->GetTeam() == ALLIANCE ? BG_AV_QUEST_A_OTHER_MINE : BG_AV_QUEST_H_NEAR_MINE;

    bool needIrondeep = bot->GetQuestStatus(northQuestId) == QUEST_STATUS_INCOMPLETE && !bot->HasItemCount(AV_IRONDEEP_SUPPLIES_ITEM, AV_MINE_SUPPLIES_REQUIRED) && av->PlayerCanDoMineQuest(BG_AV_OBJECTID_MINE_N, bot->GetTeam());

    bool needColdtooth = bot->GetQuestStatus(southQuestId) == QUEST_STATUS_INCOMPLETE && !bot->HasItemCount(AV_COLDTOOTH_SUPPLIES_ITEM, AV_MINE_SUPPLIES_REQUIRED) && av->PlayerCanDoMineQuest(BG_AV_OBJECTID_MINE_S, bot->GetTeam());

    if (!needIrondeep && !needColdtooth)
        return false;

    WorldLocation irondeep(bot->GetMapId(), 881.273f, -442.002f, 54.664f, 0.0f);

    WorldLocation coldtooth(bot->GetMapId(), -853.671f, -91.427f, 68.569f, 0.0f);

    if (needIrondeep && needColdtooth)
    {
        float irondeepDist = bot->GetDistance(irondeep.x, irondeep.y, irondeep.z);

        float coldtoothDist = bot->GetDistance(coldtooth.x, coldtooth.y, coldtooth.z);

        objectiveLocation = irondeepDist <= coldtoothDist ? irondeep : coldtooth;

        return true;
    }

    objectiveLocation = needIrondeep ? irondeep : coldtooth;
    return true;
}

bool BGTactics::HasAvMineSuppliesToTurnIn()
{
    uint32 irondeepQuestId = bot->GetTeam() == ALLIANCE ? BG_AV_QUEST_A_NEAR_MINE : BG_AV_QUEST_H_OTHER_MINE;

    uint32 coldtoothQuestId = bot->GetTeam() == ALLIANCE ? BG_AV_QUEST_A_OTHER_MINE : BG_AV_QUEST_H_NEAR_MINE;

    bool irondeepReady = bot->GetQuestStatus(irondeepQuestId) == QUEST_STATUS_COMPLETE || (bot->GetQuestStatus(irondeepQuestId) == QUEST_STATUS_INCOMPLETE && bot->HasItemCount(AV_IRONDEEP_SUPPLIES_ITEM, AV_MINE_SUPPLIES_REQUIRED));

    bool coldtoothReady = bot->GetQuestStatus(coldtoothQuestId) == QUEST_STATUS_COMPLETE || (bot->GetQuestStatus(coldtoothQuestId) == QUEST_STATUS_INCOMPLETE && bot->HasItemCount(AV_COLDTOOTH_SUPPLIES_ITEM, AV_MINE_SUPPLIES_REQUIRED));

    return irondeepReady || coldtoothReady;
}

bool BGTactics::SelectAvMineSupplyTurnInObjective(WorldLocation& objectiveLocation)
{
    if (!HasAvMineSuppliesToTurnIn())
        return false;

    uint32 quartermasterEntry = bot->GetTeam() == ALLIANCE ? AV_STORMPIKE_QUARTERMASTER : AV_FROSTWOLF_QUARTERMASTER;

    for (ObjectGuid guid : AI_VALUE(std::list<ObjectGuid>, "nearest npcs"))
    {
        Creature* quartermaster = ai->GetCreature(guid);
        if (!quartermaster)
            continue;

        if (quartermaster->GetEntry() != quartermasterEntry)
            continue;

        objectiveLocation = WorldLocation(quartermaster->GetMapId(), quartermaster->GetPositionX(), quartermaster->GetPositionY(), quartermaster->GetPositionZ(), quartermaster->GetOrientation());

        return true;
    }

    char const* baseLocation = bot->GetTeam() == ALLIANCE ? "AV_STORMPIKE_AID_STATION" : "AV_FROSTWOLF_RELIEF_HUT";

    return sRandomPlayerbotMgr.GetNamedLocation(baseLocation, objectiveLocation);
}

bool BGTactics::TurnInAvMineSupplies()
{
    if (!HasAvMineSuppliesToTurnIn())
        return false;

    uint32 quartermasterEntry = bot->GetTeam() == ALLIANCE ? AV_STORMPIKE_QUARTERMASTER : AV_FROSTWOLF_QUARTERMASTER;

    for (ObjectGuid guid : AI_VALUE(std::list<ObjectGuid>, "nearest npcs"))
    {
        Creature* quartermaster = ai->GetCreature(guid);
        if (!quartermaster)
            continue;

        if (quartermaster->GetEntry() != quartermasterEntry)
            continue;

        if (!bot->IsWithinDistInMap(quartermaster, INTERACTION_DISTANCE))
        {
            continue;
        }

        if (bot->IsMounted())
            bot->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

        if (bot->IsInDisallowedMountForm())
            bot->RemoveSpellsCausingAura(SPELL_AURA_MOD_SHAPESHIFT);

        ai->StopMoving();

        Event turnInEvent("av mine supplies turn in", quartermaster->GetObjectGuid(), bot);

        bool turnedIn = ai->DoSpecificAction("talk to quest giver", turnInEvent, true);

        Event acceptEvent("av mine supplies reaccept", quartermaster->GetObjectGuid(), bot);

        bool accepted = ai->DoSpecificAction("accept all quests", acceptEvent, true);

        if (turnedIn || accepted)
        {
            ai::PositionMap& posMap = context->GetValue<ai::PositionMap&>("position")->Get();

            ai::PositionEntry pos = posMap["bg objective"];

            pos.Reset();
            posMap["bg objective"] = pos;

            return true;
        }
    }

    return false;
}
