#pragma once
#include "playerbot/strategy/Value.h"
#include <ctime>
#include <optional>
#include <string>
#include <string_view>

namespace ai
{
    enum class TravelDestinationPurpose : uint32;

    enum class GuildActivity
    {
        None,
        Orders,
        Meeting
    };

    class GuildSchedule
    {
    public:
        static GuildSchedule Parse(std::string_view motd);

        GuildActivity GetActivity(std::time_t now) const;
        std::string const& GetMeetingLocation() const { return meetingLocation; }

    private:
        struct Window
        {
            int start;
            int end;

            bool Contains(int minute, int leadTime = 0) const;
        };

        static std::optional<Window> ParseWindow(std::string_view body, std::string_view& location);

        std::optional<Window> farm;
        std::optional<Window> meeting;
        std::string meetingLocation;
    };

    class GuildScheduleValue : public CalculatedValue<GuildActivity>
    {
    public:
        GuildScheduleValue(PlayerbotAI* ai) : CalculatedValue<GuildActivity>(ai, "guild schedule", 2) {}

        GuildActivity Calculate() override;
        static GuildActivity GetActivity(PlayerbotAI* ai);
        static std::string GetMeetingLocation(PlayerbotAI* ai);

    private:
        std::string motd;
        GuildSchedule schedule;
    };

    bool GuildOrdersAllowed(PlayerbotAI* ai);
    bool HasGuildQuestItems(Player const* bot, Quest const* quest);
    bool CanCompleteGuildQuest(Player const* bot, Quest const* quest);
    TravelDestinationPurpose GetGuildQuestTravelPurpose(Player const* bot, Quest const* quest);
    void ResetGuildQuestOrders(PlayerbotAI* ai);
    inline constexpr std::string_view GuildOrderCastPrefix = "guild order ";

    enum class GuildOrderType : uint8
    {
        None = 0,
        Farm,
        Kill,
        Explore,
        Craft,
        AuctionHouse,
        QuestReward
    };

    struct GuildOrder
    {
        GuildOrderType type = GuildOrderType::None;
        std::string target;
        uint32 amount = 0;
        uint32 questId = 0;
        uint32 rewardItemId = 0;
        uint32 itemId = 0;
        int8 rewardChoiceIndex = -1;
        bool isQuestPrerequisite = false;

        bool IsValid() const { return type != GuildOrderType::None && !target.empty(); }
        bool IsTravelOrder() const { return type == GuildOrderType::Farm || type == GuildOrderType::Kill || type == GuildOrderType::Explore || type == GuildOrderType::AuctionHouse || type == GuildOrderType::QuestReward; }
        bool IsCraftOrder() const { return type == GuildOrderType::Craft; }
        bool IsQuestRewardOrder() const { return type == GuildOrderType::QuestReward; }
        bool IsReadyQuestReward() const { return IsQuestRewardOrder() && !isQuestPrerequisite; }
        bool NeedsMoreOfReward(Player const* player) const { return rewardItemId && player->GetItemCount(rewardItemId) < amount; }

        std::string GetTypeName() const
        {
            switch (type)
            {
            case GuildOrderType::Farm:         return "Farm";
            case GuildOrderType::Kill:         return "Kill";
            case GuildOrderType::Explore:      return "Explore";
            case GuildOrderType::Craft:        return "Craft";
            case GuildOrderType::AuctionHouse: return "AuctionHouse";
            case GuildOrderType::QuestReward:  return "QuestReward";
            default:                           return "None";
            }
        }
    };

    class GuildOrderValue : public CalculatedValue<GuildOrder>
    {
    public:
        GuildOrderValue(PlayerbotAI* ai) : CalculatedValue<GuildOrder>(ai, "guild order", 60) {}

        GuildOrder Get() override;
        GuildOrder Calculate() override;

        static uint32 FindItemByName(const std::string& name);
    private:
        static std::string TrimWhitespace(const std::string& str);
        static bool ParseOrderPrefix(const std::string& note, const std::string& prefix, std::string& outBody);
        bool useShareList = false;
    };

    class HasGuildOrderValue : public BoolCalculatedValue
    {
    public:
        HasGuildOrderValue(PlayerbotAI* ai) : BoolCalculatedValue(ai, "has guild order", 10) {}

        bool Calculate() override { return AI_VALUE(GuildOrder, "guild order").IsValid(); }
    };

    class HasGuildTravelOrderValue : public BoolCalculatedValue
    {
    public:
        HasGuildTravelOrderValue(PlayerbotAI* ai) : BoolCalculatedValue(ai, "has guild travel order", 10) {}

        bool Calculate() override
        {
            GuildOrder order = AI_VALUE(GuildOrder, "guild order");
            if (!order.IsTravelOrder())
                return false;

            // If a Farm order has an amount, check inventory count.
            if (order.type == GuildOrderType::Farm && order.amount > 0)
            {
                uint32 itemId = order.itemId ? order.itemId : GuildOrderValue::FindItemByName(order.target);
                if (itemId)
                {
                    uint32 currentCount = ai->GetInventoryItemsCountWithId(itemId);
                    if (currentCount >= order.amount)
                        return false;
                }
            }

            return true;
        }
    };

    class HasGuildCraftOrderValue : public BoolCalculatedValue
    {
    public:
        HasGuildCraftOrderValue(PlayerbotAI* ai) : BoolCalculatedValue(ai, "has guild craft order", 10) {}

        bool Calculate() override { return AI_VALUE(GuildOrder, "guild order").IsCraftOrder(); }
    };

    // Returns true if the bot has a QuestReward guild order whose quest is not yet in the quest log.
    class NeedsGuildQuestOrderAcceptValue : public BoolCalculatedValue
    {
    public:
        NeedsGuildQuestOrderAcceptValue(PlayerbotAI* ai) : BoolCalculatedValue(ai, "needs guild quest order accept", 5) {}

        bool Calculate() override
        {
            GuildOrder order = AI_VALUE(GuildOrder, "guild order");
            if (!order.IsQuestRewardOrder() || !order.NeedsMoreOfReward(bot))
                return false;

            Quest const* quest = sObjectMgr.GetQuestTemplate(order.questId);
            if (!HasGuildQuestItems(bot, quest) || quest->IsAutoComplete())
                return false;

            return bot->CanTakeQuest(quest, false) && bot->CanAddQuest(quest, false);
        }
    };

    class NeedsGuildQuestOrderTurnInValue : public BoolCalculatedValue
    {
    public:
        NeedsGuildQuestOrderTurnInValue(PlayerbotAI* ai) : BoolCalculatedValue(ai, "needs guild quest order turn in", 5) {}

        bool Calculate() override
        {
            GuildOrder order = AI_VALUE(GuildOrder, "guild order");
            if (!order.IsQuestRewardOrder() || !order.NeedsMoreOfReward(bot))
                return false;

            Quest const* quest = sObjectMgr.GetQuestTemplate(order.questId);
            return HasGuildQuestItems(bot, quest) &&
                (bot->CanRewardQuest(quest, false) || CanCompleteGuildQuest(bot, quest));
        }
    };

    class NeedsProfessionReagentsValue : public BoolCalculatedValue
    {
    public:
        NeedsProfessionReagentsValue(PlayerbotAI* ai) : BoolCalculatedValue(ai, "needs profession reagents", 120) {}

        bool Calculate() override;

        static std::vector<uint32> GetMissingReagents(PlayerbotAI* ai);
    };

    enum class GuildShareFilter : uint8
    {
        FILTER_CLASS = 0,
        FILTER_ALL,
        FILTER_MELEE,
        FILTER_RANGED,
        FILTER_TANK,
        FILTER_DPS,
        FILTER_HEAL,
    };

    struct GuildShareItemEntry
    {
        GuildShareFilter filter = GuildShareFilter::FILTER_CLASS;
        uint8 playerClass = 0;
        uint32 itemId = 0;
        uint32 amount = 0;

        bool MatchesPlayer(Player* player) const;
    };

    uint32 CountGuildFinishedItemDeficit(Player* bot, uint32 itemId, const std::vector<GuildShareItemEntry>& shareList);

    class GuildShareListValue : public CalculatedValue<std::vector<GuildShareItemEntry>>
    {
    public:
        GuildShareListValue(PlayerbotAI* ai) : CalculatedValue<std::vector<GuildShareItemEntry>>(ai, "guild share list", 30) {}

        std::vector<GuildShareItemEntry> Calculate() override;

        static uint8 ParseClassName(const std::string& name);
        static GuildShareFilter ParseRoleFilter(const std::string& name);
    };

    // Represents an item a nearby guild member needs, paired with the receiver.
    struct GuildShareTarget
    {
        Player* receiver = nullptr;
        uint32 itemId = 0;
        uint32 amount = 0;

        bool IsValid() const { return receiver && itemId; }
    };

    class GuildShareTargetValue : public CalculatedValue<GuildShareTarget>
    {
    public:
        GuildShareTargetValue(PlayerbotAI* ai) : CalculatedValue<GuildShareTarget>(ai, "guild share target", 5) {}

        GuildShareTarget Calculate() override;
    };

    // Returns true if there is a valid guild share target nearby.
    class HasGuildShareTargetValue : public BoolCalculatedValue
    {
    public:
        HasGuildShareTargetValue(PlayerbotAI* ai) : BoolCalculatedValue(ai, "has guild share target", 5) {}

        bool Calculate() override { return AI_VALUE(GuildShareTarget, "guild share target").IsValid(); }
    };

    class GuildShareCraftOrderValue : public CalculatedValue<GuildOrder>
    {
    public:
        GuildShareCraftOrderValue(PlayerbotAI* ai) : CalculatedValue<GuildOrder>(ai, "guild share craft order", 60) {}

        GuildOrder Calculate() override;
    };

    class GuildShareFarmOrderValue : public CalculatedValue<GuildOrder>
    {
    public:
        GuildShareFarmOrderValue(PlayerbotAI* ai) : CalculatedValue<GuildOrder>(ai, "guild share farm order", 60) {}

        GuildOrder Calculate() override;
    };

    class GuildShareQuestRewardOrderValue : public CalculatedValue<GuildOrder>
    {
    public:
        GuildShareQuestRewardOrderValue(PlayerbotAI* ai) : CalculatedValue<GuildOrder>(ai, "guild share quest reward order", 10) {}

        GuildOrder Calculate() override;
    };

    struct GuildShareQuestWork
    {
        std::map<uint32, uint32> requiredItems;
        std::vector<GuildOrder> exchanges;
    };

    class GuildShareQuestWorkValue : public CalculatedValue<GuildShareQuestWork const*>
    {
    public:
        GuildShareQuestWorkValue(PlayerbotAI* ai) : CalculatedValue<GuildShareQuestWork const*>(ai, "guild share quest work", 30) {}

        GuildShareQuestWork const* Calculate() override;

    private:
        GuildShareQuestWork work;
    };

    class GuildShareQuestRewardItemValue : public CalculatedValue<uint32>
    {
    public:
        GuildShareQuestRewardItemValue(PlayerbotAI* ai) : CalculatedValue<uint32>(ai, "guild share quest reward item", 5) {}

        uint32 Calculate() override;
    };

    class PetitionSignsValue : public SingleCalculatedValue<uint8>
    {
    public:
        PetitionSignsValue(PlayerbotAI* ai) : SingleCalculatedValue<uint8>(ai, "petition signs") {}

        uint8 Calculate() override;
    };

    class CanHandInPetitionValue : public CalculatedValue<bool>
    {
    public:
        CanHandInPetitionValue(PlayerbotAI* ai) : CalculatedValue<bool>(ai, "can hand in guild petition") {}

        bool Calculate() override { return !bot->GetGuildId() && AI_VALUE2(uint32, "item count", chat->formatQItem(5863)) && AI_VALUE(uint8, "petition signs") >= sWorld.getConfig(CONFIG_UINT32_MIN_PETITION_SIGNS); };
    };

    class CanBuyTabard : public CalculatedValue<bool>
    {
    public:
        CanBuyTabard(PlayerbotAI* ai) : CalculatedValue<bool>(ai, "can buy tabard") {}

        bool Calculate() override;
    };
}
