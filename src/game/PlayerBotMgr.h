#pragma once

// Stub for vmangos PlayerBotMgr - replaced by Playerbots system
// This header provides minimal compatibility for core files that reference the old bot system.

class WorldSession;
class Player;

class PlayerBotMgr
{
public:
    static PlayerBotMgr& Instance()
    {
        static PlayerBotMgr instance;
        return instance;
    }

    void DeleteAll() {}
    void LoadConfig() {}
    void Load() {}
    void Update(uint32 /*diff*/) {}
    bool IsSavingAllowed() const { return true; }
    bool IsChatBot(uint32 /*guid*/) const { return false; }
    bool ForceAccountConnection(WorldSession* /*session*/) const { return false; }
    void OnPlayerInWorld(Player* /*player*/) {}
};

#define sPlayerBotMgr PlayerBotMgr::Instance()
