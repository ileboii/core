#pragma once
#pragma once

// Stub for vmangos PlayerBotAI - replaced by Playerbots system
// This header provides minimal compatibility for core files that reference the old bot AI.

#include "Common.h"

enum PlayerBotAutoEquip
{
    PLAYER_BOT_AUTO_EQUIP_STARTING_GEAR = 0,
    PLAYER_BOT_AUTO_EQUIP_RANDOM_GEAR   = 1,
    PLAYER_BOT_AUTO_EQUIP_PREMADE_GEAR  = 2,
};

enum PlayerBotState
{
    PB_STATE_OFFLINE = 0,
    PB_STATE_LOADING = 1,
    PB_STATE_ONLINE  = 2,
};

class Player;
class Unit;
class WorldPacket;

class PlayerAI
{
public:
    virtual ~PlayerAI() {}
    virtual void UpdateAI(uint32 /*diff*/) {}
    virtual void Remove() {}
    virtual void OnPacketReceived(WorldPacket const* /*packet*/) {}
    virtual void BeforeAddToMap(Player* /*player*/) {}
};

class PlayerControlledAI : public PlayerAI
{
public:
    PlayerControlledAI(Player* /*player*/, Unit* /*controller*/) {}
};

struct PlayerBotEntry
{
    std::shared_ptr<PlayerAI> ai;
    PlayerBotState state = PB_STATE_OFFLINE;
    bool requestRemoval = false;
};
