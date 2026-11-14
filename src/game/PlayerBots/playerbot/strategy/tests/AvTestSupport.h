#pragma once

#include "TestComponent.h"

namespace ai
{
    // =====================================================
    // AV test support - Alterac Valley state access for the
    // test framework. Node names mirror the battleground_events
    // descriptions of the world db.
    // =====================================================
    struct AvTestNode
    {
        const char* name;
        uint8 nodeId;
        bool tower;
        float x;
        float y;
        float z;
    };

    uint8 const AV_TEST_MAP_ID = 30;
    uint8 const AV_TEST_NODE_COUNT = 15;
    uint8 const AV_TEST_MINE_COUNT = 2;

    AvTestNode const* GetAvTestNodes();
    bool ParseAvNodeName(std::string const& name, uint8& outNodeId);
    std::string FormatAvNodeState(uint8 owner, uint8 state);
    std::string FormatAvSquadScore(Player* bot, TestContext const& ctx);
}
