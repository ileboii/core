
#include "playerbot/playerbot.h"
#include "RtiAction.h"
#include "playerbot/PlayerbotAIConfig.h"
#include "playerbot/strategy/values/RtiTargetValue.h"

using namespace ai;

bool RtiAction::Execute(Event& event)
{
    Player* requester = event.getOwner() ? event.getOwner() : GetMaster();
    std::string text = event.getParam();
    std::string type = "rti";
    if (text == "cc" || text.find("cc ") == 0)
    {
        type = "rti cc";
        text = text == "cc" ? "?" : text.substr(3);
    }

    if (type == "rti" && (text.empty() || text == "?"))
    {
        std::ostringstream outRti; outRti << "rti" << ": ";
        AppendRti(outRti, "rti");
        ai->TellPlayer(requester, outRti);

        std::ostringstream outRtiCc; outRtiCc << "rti cc" << ": ";
        AppendRti(outRtiCc, "rti cc");
        ai->TellPlayer(requester, outRtiCc);
        return true;
    }

    if (text == "?")
    {
        std::ostringstream out; out << type << ": ";
        AppendRti(out, type);
        ai->TellPlayer(requester, out);
        return true;
    }

    std::vector<int> indices;
    std::string normalized;
    if (!RtiTargetValue::ParseOrder(text, indices, &normalized))
    {
        ai->TellError(requester, "Use a comma-separated order of unique icons (star, circle, diamond, triangle, moon, square, cross, skull), or none");
        return false;
    }

    context->GetValue<std::string>(type)->Set(normalized);
    std::ostringstream out; out << type << " set to: ";
    AppendRti(out, type);
    ai->TellPlayer(requester, out);
    return true;
}

void RtiAction::AppendRti(std::ostringstream & out, std::string type)
{
    out << AI_VALUE(std::string, type);

    std::ostringstream n; n << type << " target";
    Unit* target = AI_VALUE(Unit*, n.str());
    if (target)
        out << " (" << target->GetName() << ")";
}

bool MarkRtiAction::Execute(Event& event)
{
    Group *group = bot->GetGroup();
    if (!group) return false;

    if (bot->InBattleGround())
        return false;

    Unit* target = NULL;
    std::list<ObjectGuid> attackers = ai->GetAiObjectContext()->GetValue<std::list<ObjectGuid>>("possible attack targets")->Get();
    for (std::list<ObjectGuid>::iterator i = attackers.begin(); i != attackers.end(); ++i)
    {
        Unit* unit = ai->GetUnit(*i);
        if (!unit)
            continue;

        // do not mark players
        if (unit->IsPlayer())
            continue;

        bool marked = false;
        for (int i = 0; i < 8; i++)
        {
            ObjectGuid guid = group->GetTargetWithIcon((RaidTargetIcon)i);
            if (guid == unit->GetObjectGuid())
            {
                marked = true;
                break;
            }
        }

        if (marked) continue;

        if (!target || (int)target->GetHealth() > (int)unit->GetHealth()) target = unit;
    }

    if (!target) return false;

    std::vector<int> indices = RtiTargetValue::GetRtiIndices(AI_VALUE(std::string, "rti"));
    if (indices.empty())
        return false;

    int index = indices.front();
#ifndef MANGOSBOT_TWO
    group->SetTargetIcon(index, target->GetObjectGuid());
#else
    group->SetTargetIcon(index, bot->GetObjectGuid(), target->GetObjectGuid());
#endif
    return true;
}
