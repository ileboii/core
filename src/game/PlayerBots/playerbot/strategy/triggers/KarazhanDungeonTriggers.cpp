
#include "playerbot/playerbot.h"
#include "KarazhanDungeonTriggers.h"
#include "GenericTriggers.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"

using namespace ai;

bool NetherspiteBeamsCheatNeedRefreshTrigger::IsActive()
{
    //Checking that is portal phase
    std::list<Creature*> creatures;
    MaNGOS::AllCreaturesOfEntryInRange u_check(bot, 17369, 100.0f);
    MaNGOS::CreatureListSearcher<MaNGOS::AllCreaturesOfEntryInRange> searcher(creatures, u_check);
    Cell::VisitAllObjects(bot, searcher, 100);

    if (creatures.empty())
        return false;

    //Checking that is Netherspite target
    return AI_VALUE2(bool, "has aggro", "current target");
}