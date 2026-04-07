#pragma once
#pragma once

// vmangos is vanilla WoW (expansion 0)
#ifndef MANGOSBOT_ZERO
#define MANGOSBOT_ZERO 1
#endif
#ifndef MAX_EXPANSION
#define MAX_EXPANSION 0
#endif

// Compatibility layer between cmangos playerbot code and vmangos core.
// cmangos uses AreaTableEntry from DBC with sAreaStore (DBCStorage).
// vmangos uses AreaEntry from SQL with sAreaStorage (SQLStorage).

#include "Map.h"         // AreaEntry, sAreaStorage
#include "SQLStorages.h" // sAreaStorage declaration
#include "ObjectMgr.h"   // sObjectMgr

// Type alias: cmangos AreaTableEntry -> vmangos AreaEntry
typedef AreaEntry AreaTableEntry;

// cmangos AreaTableEntry field compatibility:
//   cmangos: ID, zone, area_name[locale], mapid, flags, area_level, team
//   vmangos: Id, ZoneId, Name, MapId, Flags, AreaLevel, Team
//
// The playerbot code accesses AreaTableEntry members directly.
// We handle the field name differences via wrapper struct or macros below.

// Wrapper to emulate cmangos GetAreaEntryByAreaID
inline AreaTableEntry const* GetAreaEntryByAreaID(uint32 areaId)
{
    return sAreaStorage.LookupEntry<AreaEntry>(areaId);
}

// Wrapper class to emulate cmangos sAreaStore interface using vmangos sAreaStorage
struct AreaStoreCompat
{
    uint32 GetNumRows() const
    {
        return sAreaStorage.GetMaxEntry();
    }

    AreaTableEntry const* LookupEntry(uint32 id) const
    {
        return sAreaStorage.LookupEntry<AreaEntry>(id);
    }
};

// Provide a global sAreaStore-like object for playerbot code
static AreaStoreCompat sAreaStore;

// Wrapper class to emulate cmangos sFactionTemplateStore
struct FactionTemplateStoreCompat
{
    FactionTemplateEntry const* LookupEntry(uint32 id) const
    {
        return sObjectMgr.GetFactionTemplateEntry(id);
    }
};

static FactionTemplateStoreCompat sFactionTemplateStore;

// GuidSet typedef for playerbot code
typedef std::set<ObjectGuid> GuidSet;
typedef std::list<ObjectGuid> GuidList;
typedef std::vector<ObjectGuid> GuidVector;

// Item subclass consumable constants commented out in vmangos for vanilla
// but needed by playerbot ahbot code
#ifndef ITEM_SUBCLASS_POTION
#define ITEM_SUBCLASS_POTION            1
#define ITEM_SUBCLASS_ELIXIR            2
#define ITEM_SUBCLASS_FLASK             3
#define ITEM_SUBCLASS_SCROLL            4
#define ITEM_SUBCLASS_FOOD              5
#define ITEM_SUBCLASS_ITEM_ENHANCEMENT  6
#define ITEM_SUBCLASS_BANDAGE           7
#define ITEM_SUBCLASS_CONSUMABLE_OTHER  8
#endif

// sFactionStore compatibility wrapper
struct FactionStoreCompat
{
    FactionEntry const* LookupEntry(uint32 id) const
    {
        return sObjectMgr.GetFactionEntry(id);
    }
};

static FactionStoreCompat sFactionStore;

// Creature elite rank enum compat
#ifndef CREATURE_UNKNOWN
#define CREATURE_UNKNOWN 4
#endif

// sCreatureStorage compat - iterate over creature templates
struct CreatureStorageCompat
{
    uint32 GetMaxEntry() const { return 100000; /* reasonable max */ }
    template<typename T>
    T const* LookupEntry(uint32 id) const { return sObjectMgr.GetCreatureTemplate(id); }
};

static CreatureStorageCompat sCreatureStorage;

// sSkillLineAbilityStore compat
struct SkillLineAbilityStoreCompat
{
    uint32 GetNumRows() const { return sObjectMgr.GetMaxSkillLineAbilityId(); }
    SkillLineAbilityEntry const* LookupEntry(uint32 id) const { return sObjectMgr.GetSkillLineAbility(id); }
};

static SkillLineAbilityStoreCompat sSkillLineAbilityStore;


// Include WorldSession for handler declarations
#include "Server/WorldSession.h"
#include "Spells/SpellEntry.h"
using namespace Spells;

template<typename PacketType>
inline PacketType MakeTypedPacket(WorldPacket& wp)
{
    PacketType pkt;
    pkt.ReadFromWorldPacket(wp);
    return pkt;
}

inline NullClientPacket MakeNullPacket(WorldPacket& wp)
{
    NullClientPacket pkt(wp.GetOpcode());
    return pkt;
}

#include "Server/Packets/AuctionHouse.h"
#include "Server/Packets/Misc.h"
#include "Server/Packets/Quest.h"
#include "Server/Packets/Npc.h"
#include "Server/Packets/Item.h"
#include "Server/Packets/Loot.h"
#include "Server/Packets/Group.h"
#include "Server/Packets/Guild.h"
#include "Server/Packets/Trade.h"
#include "Server/Packets/Battleground.h"
#include "Server/Packets/Spell.h"
#include "Server/Packets/Pet.h"
#include "Server/Packets/Mail.h"
#include "Server/Packets/Chat.h"
#include "Server/Packets/Movement.h"
#include "Server/Packets/Channel.h"
#include "Server/Packets/Duel.h"

#include "Maps/PathFinder.h"
typedef PathInfo PathFinder;

#ifndef BG_AB_BANNER_ALLIANCE
#define BG_AB_BANNER_ALLIANCE 180058
#define BG_AB_BANNER_HORDE 180059
#define BG_AB_BANNER_CONTESTED_A 180060
#define BG_AB_BANNER_CONTESTED_H 180061
#define BG_AB_BANNER_STABLE 180087
#define BG_AB_BANNER_BLACKSMITH 180088
#define BG_AB_BANNER_FARM 180089
#define BG_AB_BANNER_LUMBER_MILL 180090
#define BG_AB_BANNER_MINE 180091
#endif

#ifndef BG_AB_NODE_STATUS_NEUTRAL
#define BG_AB_NODE_STATUS_NEUTRAL BG_AB_NODE_TYPE_NEUTRAL
#endif

#ifndef GO_WS_SILVERWING_FLAG
#define GO_WS_SILVERWING_FLAG 179830
#define GO_WS_WARSONG_FLAG 179831
#define GO_WS_SILVERWING_FLAG_DROP 179785
#define GO_WS_WARSONG_FLAG_DROP 179786
#endif

#ifndef BG_AV_NODE_STATUS_ALLY_OCCUPIED
#define BG_AV_NODE_STATUS_ALLY_OCCUPIED ALLIANCE_CONTROLLED
#define BG_AV_NODE_STATUS_ALLY_CONTESTED ALLIANCE_ASSAULTED
#define BG_AV_NODE_STATUS_HORDE_OCCUPIED HORDE_CONTROLLED
#define BG_AV_NODE_STATUS_HORDE_CONTESTED HORDE_ASSAULTED
#endif


static uint32 Buff_Entries[] = { 179871, 179904, 179905 };

#ifndef VISIBILITY_DISTANCE_SMALL
#define VISIBILITY_DISTANCE_SMALL 25.0f
#endif
