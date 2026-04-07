#include "GuidPosition.h"
#include <numeric>
#include <iomanip>

#include "GameEventMgr.h"
#include "ObjectAccessor.h"
#include "PlayerbotAI.h"

using namespace ai;
using namespace MaNGOS;

GuidPosition::GuidPosition(std::string qualifier)
{
    std::stringstream b(qualifier);

    uint64 g;
    char p;
    b >> this->mapId >> p >> this->x >> p >> this->y >> p >> this->z >> p >> this->o;

    //if (b.tellp() == std::streampos(0))
    //    return;

    b >> p >> g;

    ObjectGuid guid(g);
    ObjectGuid::Set(guid);
}

std::string GuidPosition::to_string() const
{
    std::ostringstream b;
    char p = '|';
    b << this->getMapId() << p << this->x << p << this->y << p << this->z << p << this->o << p << GetRawValue();
    return b.str();
}

Creature* GuidPosition::GetCreature(uint32 instanceId) const
{
    if (!*this)
        return nullptr;

    if (!getMap(instanceId))
        return nullptr;

    return getMap(instanceId)->GetAnyTypeCreature(*this);
}

Unit* GuidPosition::GetUnit(uint32 instanceId) const
{
    if (!*this)
        return nullptr;

    if (IsPlayer())
        return sObjectAccessor.FindPlayer(*this);

    return GetCreature(instanceId);
}

GameObject* GuidPosition::GetGameObject(uint32 instanceId) const
{
    if (!*this)
        return nullptr;

    if (!getMap(instanceId))
        return nullptr;

    return getMap(instanceId)->GetGameObject(*this);
}

Player* GuidPosition::GetPlayer() const
{
    if (!*this)
        return nullptr;

    if (IsPlayer())
        return sObjectAccessor.FindPlayer(*this);

    return nullptr;
}

const FactionTemplateEntry* GuidPosition::GetFactionTemplateEntry() const
{
    if (IsPlayer() && GetPlayer())
        return GetPlayer()->GetFactionTemplateEntry();
    if (IsCreature()  && GetCreatureTemplate())
        return sFactionTemplateStore.LookupEntry(GetCreatureTemplate()->faction);
    if (IsGameObject() && GetGameObjectInfo())
        return sFactionTemplateStore.LookupEntry(GetGameObjectInfo()->faction);

    return nullptr;
}

const ReputationRank GuidPosition::GetReactionTo(const GuidPosition& other, uint32 instanceId) const
{
    if(other.IsUnit() && other.GetUnit(instanceId))
        if (other.GetUnit(instanceId)->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED))
        {
            if (const Player* unitPlayer = other.GetUnit(instanceId)->GetCharmerOrOwnerPlayerOrPlayerItself())
            {
                if (unitPlayer->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_CONTESTED_PVP) && GetFactionTemplateEntry()->IsContestedGuardFaction())
                    return REP_HOSTILE;

                if (const ReputationRank* rank = unitPlayer->GetReputationMgr().GetForcedRankIfAny(GetFactionTemplateEntry()))
                    return (*rank);

                const FactionEntry* unitFactionEntry = sFactionStore.LookupEntry(GetFactionTemplateEntry()->faction);
                return unitFactionEntry ? unitPlayer->GetReputationMgr().GetRank(unitFactionEntry) : REP_NEUTRAL;
            }
        }

    return PlayerbotAI::GetFactionReaction(GetFactionTemplateEntry(), other.GetFactionTemplateEntry());
}

bool GuidPosition::isDead(uint32 instanceId)
{
    if (!getMap(instanceId))
        return false;

    if (!getMap(instanceId)->IsLoaded(getX(), getY()))
        return false;

    if (IsUnit() && GetUnit(instanceId) && GetUnit(instanceId)->IsInWorld() && GetUnit(instanceId)->IsAlive())
        return false;

    if (IsGameObject() && GetGameObject(instanceId) && GetGameObject(instanceId)->IsInWorld())
        return false;

    return true;
}

uint16 GuidPosition::IsPartOfAPool()
{
    if (IsCreature())
        return sPoolMgr.IsPartOfAPool<Creature>(GetCounter());
    if (IsGameObject())
        return sPoolMgr.IsPartOfAPool<GameObject>(GetCounter());

    return 0;
}

uint16 GuidPosition::GetGameEventId()
{
    if (uint16 pool_id = IsPartOfAPool())
    {
        uint16 top_pool_id = sPoolMgr.IsPartOfTopPool<Pool>(pool_id);

        if (int16 event_id = sGameEventMgr.GetGameEventId<Pool>(top_pool_id))
            return event_id;
    }

    if (IsCreature())
        return sGameEventMgr.GetGameEventId<Creature>(GetCounter());
    if (IsGameObject())
        return sGameEventMgr.GetGameEventId<GameObject>(GetCounter());

    return 0;
}

bool GuidPosition::IsEventUnspawned()
{
    if (int16 event_id = GetGameEventId())
        if (!sGameEventMgr.IsActiveEvent(event_id))
            return true;
    
    return false;
}

std::string GuidPosition::print()
{
    std::ostringstream out;
    out << this->GetRawValue();
    out << ';' << mapId << std::fixed << std::setprecision(2);
    out << ';' << x;
    out << ';' << y;
    out << ';' << z;
    out << ';' << o;

    return out.str();
}
