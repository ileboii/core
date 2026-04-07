//add here most rarely modified headers to speed up debug build compilation
#include "WorldSocket.h"
#include "Common.h"

// vmangos is vanilla WoW (expansion 0)
#define MANGOSBOT_ZERO 1
#define MAX_EXPANSION 0

#include <future>

// Core game systems
#include "MapManager.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "SQLStorages.h"
#include "Opcodes.h"
#include "SharedDefines.h"
#include "GuildMgr.h"
#include "ObjectMgr.h"
#include "ScriptMgr.h"

// Heavy game headers (frequently used, rarely modified)
#include "Spell.h"
#include "SpellMgr.h"
#include "SpellAuras.h"
#include "WorldPacket.h"
#include "LootMgr.h"
#include "GossipDef.h"
#include "Chat.h"
#include "World.h"
#include "Unit.h"
#include "MotionMaster.h"
#include "Guild.h"
#include "Player.h"
#include "Group.h"
#include "Database/DatabaseEnv.h"

// Grid system (used in many playerbot files)
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"

// Boost headers - commented out for vmangos (no boost dependency)
// 
// 
// #include <boost/bimap.hpp>
// #include <boost/bimap/multiset_of.hpp>

// STL headers
#include <stack>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <memory>
#include <regex>
#include <future>
#include <numeric>

// Playerbot core
#include "playerbot/playerbot.h"

// Playerbot AI framework (included 40-60+ times each, rarely modified)
#include "playerbot/strategy/AiObject.h"
#include "playerbot/strategy/Value.h"
#include "playerbot/strategy/Action.h"
#include "playerbot/strategy/Trigger.h"
#include "playerbot/strategy/Strategy.h"
#include "playerbot/strategy/NamedObjectContext.h"
#include "playerbot/strategy/AiObjectContext.h"
