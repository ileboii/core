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
}
