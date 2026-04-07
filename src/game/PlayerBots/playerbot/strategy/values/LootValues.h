#pragma once
#include "ItemUsageValue.h"

namespace ai
{ 
    //Cheat class copy to hack into the loot system
    class LootLootGroupAccess                               // A set of loot definitions for items (refs are not allowed)
    {
    public:
        LootStoreItemList ExplicitlyChanced;                // Entries with chances defined in DB
        LootStoreItemList EqualChanced;                     // Zero chances - every entry takes the same chance
    private:
        bool hasConditionalEqualChancedItem;                // Must match LootTemplate::LootGroup layout
    };

    class LootTemplateAccess
    {
    public:
        typedef std::vector<LootLootGroupAccess> LootGroups;
        LootStoreItemList Entries;                          // not grouped only
        LootGroups        Groups;                           // groups have own (optimized) processing, grouped entries go there
    };

    // vmangos Loot access - use Loot directly since it has public members
    typedef Loot LootAccess;

    //DropMap[itemId] = {entry}
    typedef std::unordered_multimap<uint32, int32> DropMap;    

    class ItemDropMapValue : public SingleCalculatedValue<DropMap*>
    {
    public:
        ItemDropMapValue(PlayerbotAI* ai) : SingleCalculatedValue(ai, "item drop map") {}

        virtual DropMap* Calculate() override;
#ifdef GenerateBotHelp
        virtual std::string GetHelpName() { return "item drop map"; } //Must equal iternal name
        virtual std::string GetHelpTypeName() { return "loot"; }
        virtual std::string GetHelpDescription()
        {
            return "This value returns all items and the items they contain as loot.";
        }
        virtual std::vector<std::string> GetUsedValues() { return {  }; }
#endif 
    };

     //Returns the loot map of all entries
    class DropMapValue : public SingleCalculatedValue<DropMap*>
    {
    public:
        DropMapValue(PlayerbotAI* ai) : SingleCalculatedValue(ai, "drop map") {}

        virtual ~DropMapValue() { delete value; }

        static LootTemplateAccess const* GetLootTemplate(ObjectGuid guid, LootType type = LOOT_CORPSE);

        virtual DropMap* Calculate() override;
#ifdef GenerateBotHelp
        virtual std::string GetHelpName() { return "drop map"; } //Must equal iternal name
        virtual std::string GetHelpTypeName() { return "loot"; }
        virtual std::string GetHelpDescription()
        {
            return "This value returns all creatures and game objects and the items they drop.";
        }
        virtual std::vector<std::string> GetUsedValues() { return {  }; }
#endif 
    };

    //Returns the entries that drop a specific item
    class ItemDropListValue : public SingleCalculatedValue<std::list<int32>>, public Qualified
    {
    public:
        ItemDropListValue(PlayerbotAI* ai) : SingleCalculatedValue(ai, "item drop list"), Qualified() {}

        virtual std::list<int32> Calculate() override;

#ifdef GenerateBotHelp
        virtual std::string GetHelpName() { return "item drop list"; } //Must equal iternal name
        virtual std::string GetHelpTypeName() { return "loot"; }
        virtual std::string GetHelpDescription()
        {
            return "This value returns all creatures or game objects that drop a specific item.";
        }
        virtual std::vector<std::string> GetUsedValues() { return { "drop map" }; }
#endif 
    };

    //Returns the items a specific entry can drop
    class EntryLootListValue : public SingleCalculatedValue<std::list<uint32>>, public Qualified
    {
    public:
        EntryLootListValue(PlayerbotAI* ai) : SingleCalculatedValue(ai, "entry loot list"), Qualified() {}
        virtual std::list<uint32> Calculate() override;

#ifdef GenerateBotHelp
        virtual std::string GetHelpName() { return "entry loot list"; } //Must equal iternal name
        virtual std::string GetHelpTypeName() { return "loot"; }
        virtual std::string GetHelpDescription()
        {
            return "This value returns all the items dropped by a specific creature or game object.";
        }
        virtual std::vector<std::string> GetUsedValues() { return { }; }
#endif 
    };

    class LootChanceValue : public SingleCalculatedValue<float>, public Qualified
    {
    public:
        LootChanceValue(PlayerbotAI* ai) : SingleCalculatedValue(ai, "loot chance"), Qualified() {}
        virtual float Calculate() override;

#ifdef GenerateBotHelp
        virtual std::string GetHelpName() { return "loot chance"; } //Must equal iternal name
        virtual std::string GetHelpTypeName() { return "loot"; }
        virtual std::string GetHelpDescription()
        {
            return "This value returns the chance a specific creature or game object will drop a certain item.";
        }
        virtual std::vector<std::string> GetUsedValues() { return { }; }
#endif 
    };

    typedef std::unordered_map<ItemUsage, std::vector<uint32>> itemUsageMap;

    class EntryLootUsageValue : public CalculatedValue<itemUsageMap>, public Qualified
    {
    public:
        EntryLootUsageValue(PlayerbotAI* ai) : CalculatedValue(ai, "entry loot usage",2), Qualified() {}
        virtual itemUsageMap Calculate() override;

#ifdef GenerateBotHelp
        virtual std::string GetHelpName() { return "entry loot usage" ; } //Must equal iternal name
        virtual std::string GetHelpTypeName() { return "loot"; }
        virtual std::string GetHelpDescription()
        {
            return "This value returns all the items a creature or game object drops and if the bot thinks this item is useful somehow.";
        }
        virtual std::vector<std::string> GetUsedValues() { return { "entry loot list", "item usage" }; }
#endif 
    };

    class HasUpgradeValue : public BoolCalculatedValue, public Qualified
    {
    public:
        HasUpgradeValue(PlayerbotAI* ai) : BoolCalculatedValue(ai, "has upgrade", 2), Qualified() {}
        virtual bool Calculate() override;

#ifdef GenerateBotHelp
        virtual std::string GetHelpName() { return "has upgrade"; } //Must equal iternal name
        virtual std::string GetHelpTypeName() { return "loot"; }
        virtual std::string GetHelpDescription()
        {
            return "This value checks if a specific creature or game object drops an item that is an equipment upgrade for the bot.";
        }
        virtual std::vector<std::string> GetUsedValues() { return {"entry loot list"}; }
#endif 
    };


    //Move to inventoryValues.h at some point
    class StackSpaceForItem : public Uint32CalculatedValue, public Qualified
    {
    public:
        StackSpaceForItem(PlayerbotAI* ai) : Uint32CalculatedValue(ai, "stack space for item", 2), Qualified() {}
        virtual uint32 Calculate() override;

#ifdef GenerateBotHelp
        virtual std::string GetHelpName() { return "stack space for item"; } //Must equal iternal name
        virtual std::string GetHelpTypeName() { return "item"; }
        virtual std::string GetHelpDescription()
        {
            return "This value returns the number of items of a specific type it can store.\n"
                "Without a player bots are limited to 80% bag space and will only have space for items in existing stacks";
        }
        virtual std::vector<std::string> GetUsedValues() { return { "bag space" , "inventory items" }; }
#endif 
    };

    class ShouldLootObject : public BoolCalculatedValue, public Qualified
    {
    public:
        ShouldLootObject(PlayerbotAI* ai) : BoolCalculatedValue(ai, "should loot object"), Qualified() {}
        virtual bool Calculate() override;

#ifdef GenerateBotHelp
        virtual std::string GetHelpName() { return "should loot object"; } //Must equal iternal name
        virtual std::string GetHelpTypeName() { return "loot"; }
        virtual std::string GetHelpDescription()
        {
            return "This value checks if an lootable object might hold something the bot can loot.\n"
                "It returns true if the object has unknown loot, gold or an item it is allowed to loot and can store in an empty space or a stack of similar items.";
        }
        virtual std::vector<std::string> GetUsedValues() { return { "stack space for item" }; }
#endif 
    };

    typedef std::unordered_multimap<ObjectGuid, uint32> LootRollMap;

    class ActiveRolls : public ManualSetValue<LootRollMap>
    {
    public:
        ActiveRolls(PlayerbotAI* ai) : ManualSetValue(ai, {}, "active rolls") {}
        static void CleanUp(Player* bot, LootRollMap& value, ObjectGuid guid = ObjectGuid(), uint32 slot = 0);
        virtual std::string Format() override;

#ifdef GenerateBotHelp
        virtual std::string GetHelpName() { return "active rolls"; } //Must equal iternal name
        virtual std::string GetHelpTypeName() { return "loot"; }
        virtual std::string GetHelpDescription()
        {
            return "This value contains the active rolls a bot has.\n"
                "This value is filled and emptied when bots see and do rolls.";
        }
        virtual std::vector<std::string> GetUsedValues() { return { }; }
#endif 
    };
}

