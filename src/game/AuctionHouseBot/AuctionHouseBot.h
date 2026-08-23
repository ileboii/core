#ifndef AUCTION_HOUSE_BOT_H
#define AUCTION_HOUSE_BOT_H

#include "Utilities/Random.h"
#include "AuctionHouseMgr.h"
#include "Config/Config.h"
#include "ItemPrototype.h"
#include "SharedDefines.h"
#include "LootMgr.h"

// VMaNGOS: AuctionHouseType enum and MAX_AUCTION_HOUSE_TYPE not present in core,
// define locally for the AuctionHouseBot.
enum AuctionHouseBotHouseType
{
    AHBOT_HOUSE_ALLIANCE = 0,
    AHBOT_HOUSE_HORDE    = 1,
    AHBOT_HOUSE_NEUTRAL  = 2,
    MAX_AHBOT_HOUSE_TYPE = 3
};

struct AuctionHouseBotItemData
{
    uint32 Value = 0;
    uint32 AddChance = 0;
    uint32 MinAmount = 0;
    uint32 MaxAmount = 0;
};

struct AuctionHouseBotStatusInfoPerType
{
    uint32 ItemsCount;
    uint32 QualityInfo[MAX_ITEM_QUALITY];
};

typedef AuctionHouseBotStatusInfoPerType AuctionHouseBotStatusInfo[MAX_AHBOT_HOUSE_TYPE];

class AuctionHouseBot
{
    public:
        AuctionHouseBot();
        ~AuctionHouseBot();

        void Initialize();
        void SetConfigFileName(const std::string& filename) { m_configFileName = filename; }
        void Update();

        // Following methods are mainly used for ingame/console commands
        bool ReloadAllConfig();
        void Rebuild(bool all);
        void PrepareStatusInfos(AuctionHouseBotStatusInfo& statusInfo) const;
        void SetItemData(uint32 item, AuctionHouseBotItemData& itemData, bool reset = false);
        AuctionHouseBotItemData GetItemData(uint32 item);

    private:
        // VMaNGOS: houseId mapping helpers
        static uint32 GetHouseIdForType(uint32 houseType);
        AuctionHouseEntry const* GetAHEntryForType(uint32 houseType) const;

        uint32 GetMinMaxConfig(const char* config, uint32 minValue, uint32 maxValue, uint32 defaultValue);
        void ParseLootConfig(char const* fieldname, std::vector<int32>& lootConfig);
        void FillUintVectorFromQuery(char const* query, std::vector<uint32>& lootTemplates);
        void ParseItemValueConfig(char const* fieldname, std::vector<uint32>& itemValues);
        void AddLootToItemMap(LootStore* store, std::vector<int32>& lootConfig, std::vector<uint32>& lootTemplates, std::unordered_map<uint32, uint32>& itemMap);
        uint32 CalculateBuyoutPrice(ItemPrototype const* prototype);
        uint32 ValueWithVariance(uint32 itemValue) { return (uint32) (itemValue + ((int32) urand(0, m_valueVariance * 2 + 1) - (int32) m_valueVariance) * (int32) (itemValue / 100)); };

        std::string m_configFileName;
        Config m_ahBotCfg;

        int32 m_houseAction;

        uint32 m_chanceSell;
        uint32 m_chanceBuy;

        std::vector<int32> m_creatureLootNormalConfig;
        std::vector<int32> m_creatureLootRareConfig;
        std::vector<int32> m_creatureLootEliteConfig;
        std::vector<int32> m_creatureLootRareEliteConfig;
        std::vector<int32> m_creatureLootWorldBossConfig;
        std::vector<int32> m_disenchantLootConfig;
        std::vector<int32> m_fishingLootConfig;
        std::vector<int32> m_gameobjectLootConfig;
        std::vector<int32> m_skinningLootConfig;
        std::vector<int32> m_professionItemsConfig;

        std::vector<std::vector<uint32>> m_itemValue = std::vector<std::vector<uint32>>(MAX_ITEM_QUALITY, std::vector<uint32>(MAX_ITEM_CLASS));
        bool m_vendorValue;
        uint32 m_valueVariance;
        uint32 m_auctionBidMin;
        uint32 m_auctionBidMax;
        uint32 m_auctionTimeMin;
        uint32 m_auctionTimeMax;
        uint32 m_buyValue;

        std::vector<uint32> m_creatureLootNormalTemplates;
        std::vector<uint32> m_creatureLootRareTemplates;
        std::vector<uint32> m_creatureLootEliteTemplates;
        std::vector<uint32> m_creatureLootRareEliteTemplates;
        std::vector<uint32> m_creatureLootWorldBossTemplates;
        std::vector<uint32> m_disenchantLootTemplates;
        std::vector<uint32> m_fishingLootTemplates;
        std::vector<uint32> m_gameobjectLootTemplates;
        std::vector<uint32> m_skinningLootTemplates;
        std::vector<uint32> m_professionItems;

        std::unordered_set<uint32> m_vendorItems;

        std::unordered_map<uint32, AuctionHouseBotItemData> m_itemData;
};

#define sAuctionHouseBot MaNGOS::Singleton<AuctionHouseBot>::Instance()

#endif
