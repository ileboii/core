#include "playerbot/playerbot.h"
#include "TestRegistry.h"
#include "AvTestSupport.h"

using namespace ai;

namespace
{
    void RegisterAvNamedLocation(std::string const& name, float x, float y, float z)
    {
        TestRegistry::RegisterNamedLocation(name, GuidPosition(ObjectGuid(), WorldPosition(AV_TEST_MAP_ID, x, y, z, 0.0f)));
    }
}

void TestRegistry::RegisterAvTests()
{
    AvTestNode const* nodes = GetAvTestNodes();
    for (uint8 i = 0; i < AV_TEST_NODE_COUNT; ++i)
        RegisterAvNamedLocation(nodes[i].name, nodes[i].x, nodes[i].y, nodes[i].z);

    RegisterAvNamedLocation("av_captain_galvangar", -545.230f, -165.350f, 57.789f);
    RegisterAvNamedLocation("av_captain_balinda",   -57.789f, -286.597f, 15.647f);
    RegisterAvNamedLocation("av_boss_drekthar",   -1370.260f, -219.702f, 98.510f);
    RegisterAvNamedLocation("av_boss_vanndar",      722.430f,  -10.998f, 50.705f);
    RegisterAvNamedLocation("av_stoneheart_outpost_waiting_horde", 28.100f, -302.600f, 15.100f);

    RegisterTest("av_status_smoke_default", {
        "require bot is level=60",
        "wait 2",
        "av status",
        "observe"
    });

    RegisterTest("av_join_smoke_default", {
        "require bot is level=60",
        "debug",
        "av join 300",
        "require av node av_dunbaldar_north alliance",
        "require av node av_frostwolf_east_tower horde",
        "require av node av_snowfall_grave neutral",
        "av status",
        "av leave",
        "cleanup av leave"
    });

    RegisterTest("av_join_squad_default", {
        "require bot is level=60",
        "debug",
        "spawn level=60 class=warrior race=human",
        "spawn level=60 class=mage race=troll",
        "av join 300",
        "require av node av_dunbaldar_south alliance",
        "av leave",
        "wait 10",
        "despawn",
        "cleanup av leave"
    });

    RegisterTest("av_capture_stoneheart_bunker_horde", {
        "require bot is level=60 race=orc class=warrior",
        "debug",
        "spawn level=60 class=warrior race=tauren",
        "spawn level=60 class=warrior race=troll",
        "form",
        "av join 300",
        "av wait start",
        "monitor av node av_stoneheart_bunker horde assaulted => pass \"Horde squad assaulted Stonehearth Bunker after <time elapsed>\"",
        "monitor time > 900 => fail \"Squad parked at the Stonehearth Bunker banner did not capture it - <current position>\"",
        "teleport av_stoneheart_bunker",
        "teleport group",
        "observe",
        "cleanup av leave"
    });

    RegisterTest("av_captain_balinda_small_squad_horde", {
        "require bot is level=60 race=orc class=warrior",
        "debug",
        "spawn level=60 class=warrior race=tauren",
        "spawn level=60 class=warrior race=troll",
        "form",
        "av join 300",
        "av wait start",
        "monitor distance to av_captain_balinda < 25 => pass \"Small squad left the Stonehearth waiting spot and reached Captain Balinda after <time elapsed>\"",
        "monitor time > 900 => fail \"Small squad stayed parked: the captain assault waits for 5 friendlies at the waiting spot and has no timeout - <current position>\"",
        "teleport av_stoneheart_outpost_waiting_horde",
        "teleport group",
        "observe",
        "cleanup av leave"
    });
}
