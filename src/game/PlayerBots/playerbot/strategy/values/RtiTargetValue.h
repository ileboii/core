#pragma once
#include "playerbot/PlayerbotAIConfig.h"
#include "playerbot/ServerFacade.h"
#include "playerbot/strategy/Value.h"
#include "Group.h"
#include "TargetValue.h"
#include <array>
#include <cctype>
#include <vector>

namespace ai
{
    class RtiTargetValue : public TargetValue
    {
    public:
        RtiTargetValue(PlayerbotAI* ai, std::string type = "rti", std::string name = "rti target") : type(type), TargetValue(ai,name)
        {}

    public:
        static int GetRtiIndex(const std::string& rti)
        {
            int index = -1;
            if(rti == "star") index = 0;
            else if(rti == "circle") index = 1;
            else if(rti == "diamond") index = 2;
            else if(rti == "triangle") index = 3;
            else if(rti == "moon") index = 4;
            else if(rti == "square") index = 5;
            else if(rti == "cross") index = 6;
            else if(rti == "skull") index = 7;
            return index;
        }

        static bool ParseOrder(const std::string& text, std::vector<int>& indices, std::string* normalized = nullptr)
        {
            std::vector<int> parsed;
            std::array<bool, 8> used = {};
            std::string order;

            size_t start = 0;
            while (start <= text.size())
            {
                size_t end = text.find(',', start);
                std::string icon = text.substr(start, end == std::string::npos ? end : end - start);
                size_t first = icon.find_first_not_of(" \t\r\n");
                if (first == std::string::npos)
                    return false;
                icon = icon.substr(first, icon.find_last_not_of(" \t\r\n") - first + 1);
                for (char& c : icon)
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

                if (icon == "none")
                {
                    if (!parsed.empty() || end != std::string::npos)
                        return false;
                    indices.clear();
                    if (normalized)
                        *normalized = "none";
                    return true;
                }

                int index = GetRtiIndex(icon);
                if (index < 0 || used[index])
                    return false;
                used[index] = true;
                parsed.push_back(index);
                if (!order.empty())
                    order += ',';
                order += icon;

                if (end == std::string::npos)
                    break;
                start = end + 1;
            }

            indices = parsed;
            if (normalized)
                *normalized = order;
            return true;
        }

        static std::vector<int> GetRtiIndices(const std::string& order)
        {
            std::vector<int> indices;
            ParseOrder(order, indices);
            return indices;
        }

        Unit *Calculate() override
        {
            Group *group = bot->GetGroup();
            if(!group)
                return NULL;

            std::vector<int> indices = GetRtiIndices(AI_VALUE(std::string, type));
            if (indices.empty())
                return NULL;

            std::list<ObjectGuid> attackers = context->GetValue<std::list<ObjectGuid>>("possible targets")->Get();
            for (int index : indices)
            {
                ObjectGuid guid = group->GetTargetWithIcon((RaidTargetIcon)index);
                if (!guid || std::find(attackers.begin(), attackers.end(), guid) == attackers.end())
                    continue;

                Unit* unit = ai->GetUnit(guid);
                if (unit && !sServerFacade.UnitIsDead(unit) &&
                    bot->IsWithinDistInMap(unit, sPlayerbotAIConfig.sightDistance, false))
                    return unit;
            }

            return NULL;
        }

    private:
    std::string type;
    };

    class RtiCcTargetValue : public RtiTargetValue
    {
    public:
        RtiCcTargetValue(PlayerbotAI* ai, std::string name = "rti cc target") : RtiTargetValue(ai, "rti cc", name) {}
    };
}
