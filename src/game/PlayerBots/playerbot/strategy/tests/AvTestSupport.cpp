#include "playerbot/playerbot.h"
#include "AvTestSupport.h"
#include "BattleGround.h"
#include "BattleGroundAV.h"

using namespace ai;

namespace
{
    AvTestNode const s_avTestNodes[AV_TEST_NODE_COUNT] = {
        {"av_firstaid_station",      0, false,  638.606f,  -32.151f, 46.099f},
        {"av_stormpike_grave",       1, false,  669.317f, -294.055f, 30.290f},
        {"av_stoneheart_grave",      2, false,   77.798f, -404.696f, 46.913f},
        {"av_snowfall_grave",        3, false, -202.611f, -112.778f, 78.487f},
        {"av_iceblood_grave",        4, false, -612.672f, -396.693f, 60.858f},
        {"av_frostwolf_grave",       5, false,-1082.529f, -346.567f, 54.977f},
        {"av_frostwolf_hut",         6, false,-1402.192f, -307.467f, 89.442f},
        {"av_dunbaldar_south",       7, true,   553.779f,  -78.657f, 51.938f},
        {"av_dunbaldar_north",       8, true,   674.001f, -143.125f, 63.661f},
        {"av_icewing_bunker",        9, true,   203.238f, -360.264f, 56.386f},
        {"av_stoneheart_bunker",    10, true,  -152.434f, -441.615f, 40.397f},
        {"av_iceblood_tower",      11, true,  -571.880f, -262.777f, 75.009f},
        {"av_tower_point",         12, true,  -768.907f, -363.710f, 90.895f},
        {"av_frostwolf_east_tower",13, true, -1302.870f, -316.968f, 113.867f},
        {"av_frostwolf_west_tower",14, true, -1297.720f, -266.741f, 114.151f},
    };
}

namespace ai
{

AvTestNode const* GetAvTestNodes()
{
    return s_avTestNodes;
}

bool ParseAvNodeName(std::string const& name, uint8& outNodeId)
{
    for (uint8 i = 0; i < AV_TEST_NODE_COUNT; ++i)
    {
        if (name == s_avTestNodes[i].name)
        {
            outNodeId = i;
            return true;
        }
    }
    return false;
}

std::string FormatAvNodeState(uint8 owner, uint8 state)
{
    std::string ownerName = owner == 0 ? "alliance" : owner == 1 ? "horde" : "neutral";
    std::string stateName = state == 0 ? "assaulted" : "controlled";
    return ownerName + " " + stateName;
}

std::string FormatAvSquadScore(Player* bot, TestContext const& ctx)
{
    BattleGround* bg = bot->GetBattleGround();
    if (!bg || bg->GetTypeID() != BATTLEGROUND_AV)
        return "no av score (not in Alterac Valley)";

    std::vector<ObjectGuid> squad;
    squad.push_back(bot->GetObjectGuid());
    squad.insert(squad.end(), ctx.spawnedBots.begin(), ctx.spawnedBots.end());

    std::string out = "av score (kills/deaths/towers assaulted/towers defended):";
    for (ObjectGuid const& guid : squad)
    {
        Player* member = sObjectMgr.GetPlayer(guid);
        std::string name = member ? member->GetName() : guid.GetString();
        std::string team = member ? (member->GetTeam() == ALLIANCE ? "A" : "H") : "?";
        out += " " + name + "(" + team + (member && member->IsInCombat() ? ",combat" : "") + (member && !member->IsAlive() ? ",dead" : "") + (member && member != bot && member->GetMapId() == bot->GetMapId() ? "," + std::to_string(uint32(member->GetDistance(bot))) + "m" : "") + ")";

        bool found = false;
        for (auto itr = bg->GetPlayerScoresBegin(); itr != bg->GetPlayerScoresEnd(); ++itr)
        {
            if (itr->first != guid)
                continue;

            BattleGroundAVScore const* score = static_cast<BattleGroundAVScore const*>(itr->second);
            out += "=" + std::to_string(score->killingBlows) + "/" + std::to_string(score->deaths) + "/" +
                std::to_string(score->towersAssaulted) + "/" + std::to_string(score->towersDefended);
            found = true;
            break;
        }

        if (!found)
            out += "=left";
    }
    return out;
}

}
