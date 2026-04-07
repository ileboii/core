#ifndef _ServerFacade_H
#define _ServerFacade_H

#include "VmangosCompat.h"
#include "Common.h"
#include "Unit.h"
#include "Player.h"
#ifdef CMANGOS
#include "GameObject.h"
#endif
#ifdef MANGOS
#include "GameObject.h"
#endif
#include "BattleGroundMgr.h"
#include "PlayerbotAIBase.h"
#include "playerbot/PlayerbotAIConfig.h"
#include "playerbot/WorldPosition.h"

class ServerFacade
{
    public:
        ServerFacade();
        virtual ~ServerFacade();
        static ServerFacade& instance()
        {
            static ServerFacade instance;
            return instance;
        }

	public:
        bool UnitIsDead(Unit *unit)
        {
            return unit->IsDead();
        }

        float GetDistance2d(Unit *unit, WorldObject* wo);

        float GetDistance2d(Unit *unit, float x, float y);

        DeathState GetDeathState(Unit *unit)
        {
            return unit->GetDeathState();
        }

        bool isSpawned(GameObject *go)
        {
            return go->isSpawned();
        }

        bool IsAlive(Unit *unit)
        {
            return unit->IsAlive();
        }

        bool isMoving(Unit *unit);

        bool IsInCombat(Unit *unit)
        {
            return unit->IsInCombat();
        }

        bool IsFrozen(Unit *unit)
        {
            return unit->IsFrozen();
        }

        bool IsInRoots(Unit *unit)
        {
            return unit->IsInRoots();
        }

        bool IsCharmed(Unit *unit)
        {
            return unit->GetCharmerGuid() && unit->GetCharmerGuid().IsPlayer();
        }

        bool IsFeared(Unit *unit)
        {
            return unit->IsFeared();
        }

        bool IsInFront(Unit *unit, WorldObject const* target, float distance,  float arc /*= M_PI_F*/)
        {
            return unit->IsWithinDist(target, distance) && unit->HasInArc(target, arc);
        }

        HostileRefManager& GetHostileRefManager(Unit *unit)
        {
            return unit->GetHostileRefManager();
        }

        ThreatManager& GetThreatManager(Unit *unit)
        {
            return unit->GetThreatManager();
        }

        void SendPacket(Player *player, WorldPacket &packet)
        {
            player->GetSession()->SendPacket(&packet);
        }

        void SendMessageToSet(Player *player, WorldPacket &packet, bool self)
        {
            player->SendMessageToSet(&packet, self);
        }

        SpellEntry const* LookupSpellInfo(uint32 spellId)
        {
            return sSpellMgr.GetSpellEntry(spellId);
        }

        SpellRangeEntry const* LookupSpellRangeEntry(uint32 rangeIndex)
        {
            return sSpellRangeStore.LookupEntry(rangeIndex);
        }

        uint32 GetSpellInfoRows()
        {
            return sSpellMgr.GetMaxSpellId();
        }

        bool IsWithinLOSInMap(Player* bot, WorldObject *wo)
        {
            return bot->IsWithinLOSInMap(wo);
        }

        bool IsWithinStaticLOSInMap(Player* bot, WorldObject* wo) const
        {
            return wo->IsInMap(bot) ? ai::WorldPosition(wo).IsInStaticLineOfSight(ai::WorldPosition(bot)) : false;
        }

        bool IsDistanceLessThan(float dist1, float dist2);
        bool IsDistanceGreaterThan(float dist1, float dist2);
        bool IsDistanceGreaterOrEqualThan(float dist1, float dist2);
        bool IsDistanceLessOrEqualThan(float dist1, float dist2);

        void SetFacingTo(Unit* unit, float angle, bool force = false);
        void SetFacingTo(Unit* unit, WorldObject* wo, bool force = false) {SetFacingTo(unit, unit->GetAngle(wo), force);}

        bool IsFriendlyTo(Unit* bot, Unit* to);
        bool IsFriendlyTo(WorldObject* bot, Unit* to);
        bool IsHostileTo(Unit* bot, Unit* to);
        bool IsHostileTo(WorldObject* bot, Unit* to);

        bool IsSpellReady(Player* bot, uint32 spell, uint32 itemId = 0);

        bool IsUnderwater(Unit *unit);
        FactionTemplateEntry const* GetFactionTemplateEntry(Unit *unit);
        Unit* GetChaseTarget(Unit* target);
        float GetChaseAngle(Unit* target);
        float GetChaseOffset(Unit* target);

        BattleGroundTypeId BgTemplateId(BattleGroundQueueTypeId queueTypeId)
        {
            return BattleGroundTypeId(queueTypeId);
        }
#ifndef MANGOSBOT_ZERO
        ArenaType BgArenaType(BattleGroundQueueTypeId queueTypeId)
        {
#ifdef MANGOS
            return sBattleGroundMgr.BGArenaType(queueTypeId);
#endif
#ifdef CMANGOS
            return sBattleGroundMgr.BgArenaType(queueTypeId);
#endif
        }
#endif

        uint32 GetAreaId(WorldObject* wo)
        {
            return sTerrainMgr.GetAreaId(wo->GetMapId(), wo->GetPositionX(), wo->GetPositionY(), wo->GetPositionZ());
        }
};

#define sServerFacade ServerFacade::instance()

#endif
