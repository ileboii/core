
#include "playerbot/playerbot.h"
#include "Objects/DynamicObject.h"
#include "AoeValues.h"

#include "playerbot/PlayerbotAIConfig.h"
#include "playerbot/ServerFacade.h"
using namespace ai;

std::list<ObjectGuid> AoeCountValue::FindMaxDensity(Player* bot, float range)
{
    if (!bot)
        return std::list<ObjectGuid>();

    PlayerbotAI* ai = bot->GetPlayerbotAI();
    std::list<ObjectGuid> const units = *ai->GetAiObjectContext()->GetValue<std::list<ObjectGuid>>("attackers");
    if (units.empty())
        return std::list<ObjectGuid>();

    // Prefetch units once so we don't pay for GetUnit() 2*N^2 times and filter by range to bot.
    struct CachedUnit { ObjectGuid guid; Unit* unit; float x; float y; };
    std::vector<CachedUnit> cached;
    cached.reserve(units.size());
    for (ObjectGuid const& guid : units)
    {
        Unit* unit = ai->GetUnit(guid);
        if (!unit)
            continue;

        float const distanceToPlayer = sServerFacade.GetDistance2d(unit, bot);
        if (!sServerFacade.IsDistanceLessOrEqualThan(distanceToPlayer, range))
            continue;

        cached.push_back({ guid, unit, unit->GetPositionX(), unit->GetPositionY() });
    }

    if (cached.empty())
        return std::list<ObjectGuid>();

    float const clusterRadius = sPlayerbotAIConfig.aoeRadius * 2.0f;
    float const clusterRadiusSq = clusterRadius * clusterRadius;

    size_t maxCount = 0;
    size_t maxIndex = 0;
    std::vector<std::vector<size_t>> clusters(cached.size());

    for (size_t i = 0; i < cached.size(); ++i)
    {
        // Can't beat the current best even if every remaining unit clusters around i.
        if (cached.size() - i <= maxCount)
            break;

        auto& bucket = clusters[i];
        bucket.reserve(cached.size());
        float const xi = cached[i].x;
        float const yi = cached[i].y;
        for (size_t j = 0; j < cached.size(); ++j)
        {
            float const dx = xi - cached[j].x;
            float const dy = yi - cached[j].y;
            if (dx * dx + dy * dy <= clusterRadiusSq)
                bucket.push_back(j);
        }

        if (bucket.size() > maxCount)
        {
            maxCount = bucket.size();
            maxIndex = i;
        }
    }

    if (!maxCount)
        return std::list<ObjectGuid>();

    std::list<ObjectGuid> result;
    for (size_t idx : clusters[maxIndex])
        result.push_back(cached[idx].guid);

    return result;
}

WorldLocation AoePositionValue::Calculate()
{
    std::list<ObjectGuid> group = AoeCountValue::FindMaxDensity(bot);
    if (group.empty())
        return WorldLocation();

    // Note: don't know where these values come from or even used.
    float x1, y1, x2, y2;
    for (std::list<ObjectGuid>::iterator i = group.begin(); i != group.end(); ++i)
    {
        Unit* unit = bot->GetPlayerbotAI()->GetUnit(*i);
        if (!unit)
            continue;

        if (i == group.begin() || x1 > unit->GetPositionX())
            x1 = unit->GetPositionX();
        if (i == group.begin() || x2 < unit->GetPositionX())
            x2 = unit->GetPositionX();
        if (i == group.begin() || y1 > unit->GetPositionY())
            y1 = unit->GetPositionY();
        if (i == group.begin() || y2 < unit->GetPositionY())
            y2 = unit->GetPositionY();
    }
    float x = (x1 + x2) / 2;
    float y = (y1 + y2) / 2;
    float z = bot->GetPositionZ() + CONTACT_DISTANCE;;
    bot->UpdateAllowedPositionZ(x, y, z);
    return WorldLocation(bot->GetMapId(), x, y, z, 0);
}

uint8 AoeCountValue::Calculate()
{
    return FindMaxDensity(bot).size();
}

bool HasAreaDebuffValue::Calculate()
{
    if (!GetTarget())
        return false;

    Unit* checkTarget = GetTarget();
    if (!checkTarget)
        return false;

    std::list<ObjectGuid> nearestDynObjects = *context->GetValue<std::list<ObjectGuid> >("nearest dynamic objects no los");
    if (nearestDynObjects.empty())
        return false;

    for (std::list<ObjectGuid>::iterator i = nearestDynObjects.begin(); i != nearestDynObjects.end(); ++i)
    {
        DynamicObject* go = checkTarget->GetMap()->GetDynamicObject(*i);
        if (!go)
            continue;

        SpellEntry const* spellProto = sSpellMgr.GetSpellEntry(go->GetSpellId());
        if (!spellProto)
            continue;

        if (IsPositiveSpell(spellProto->Id))
            continue;

        if (checkTarget->IsWithinDist(go, go->GetRadius()))
            return true;
    }

    return false;
}
