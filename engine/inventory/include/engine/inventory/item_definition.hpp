#pragma once

#include <engine/stats/stat_definition.hpp>
#include <engine/stats/stat_modifier.hpp>
#include <engine/core/uuid.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace engine::inventory {

// ============================================================================
// Item Type
// ============================================================================

enum class ItemType : uint8_t {
    Consumable,     // Potions, food, scrolls
    Equipment,      // Weapons, armor, accessories
    Material,       // Crafting materials
    Quest,          // Quest items
    Key,            // Keys, passes, tickets
    Currency,       // Gold, gems, special currencies
    Ammo,           // Arrows, bullets
    Misc            // Other items
};

// ============================================================================
// Item Rarity
// ============================================================================

enum class ItemRarity : uint8_t {
    Common,
    Uncommon,
    Rare,
    Epic,
    Legendary,
    Unique,         // One-of-a-kind items
    Artifact        // Story/special items
};

// ============================================================================
// Equipment Slot
// ============================================================================

enum class EquipmentSlot : uint8_t {
    None = 0,
    MainHand,
    OffHand,
    TwoHand,        // Uses both hand slots
    Head,
    Chest,
    Hands,
    Legs,
    Feet,
    Neck,
    Ring1,
    Ring2,
    Belt,
    Back,           // Cape/cloak
    Accessory1,
    Accessory2,
    Count
};

// ============================================================================
// Weapon Type
// ============================================================================

enum class WeaponType : uint8_t {
    None,
    Sword,
    Axe,
    Mace,
    Dagger,
    Spear,
    Staff,
    Bow,
    Crossbow,
    Shield,
    TwoHandedSword,
    TwoHandedAxe,
    TwoHandedMace,
    Polearm,
    Wand,
    Fist
};

// ============================================================================
// Armor Type
// ============================================================================

enum class ArmorType : uint8_t {
    None,
    Cloth,
    Light,
    Medium,
    Heavy,
    Shield
};

// ============================================================================
// Item Requirement
// ============================================================================

struct ItemRequirement {
    stats::StatType stat;
    float min_value;
    std::string description;    // "Level 10", "Strength 20"
};

// ============================================================================
// Item Definition
// ============================================================================

struct ItemDefinition {
    std::string item_id;                    // Unique identifier
    std::string display_name;
    std::string description;
    std::string lore;                       // Flavor text
    std::string icon_path;
    std::string mesh_path;                  // For world/equipped display
    std::string drop_vfx;
    std::string pickup_sfx;

    ItemType type = ItemType::Misc;
    ItemRarity rarity = ItemRarity::Common;

    // Equipment specific
    EquipmentSlot slot = EquipmentSlot::None;
    WeaponType weapon_type = WeaponType::None;
    ArmorType armor_type = ArmorType::None;

    // Stacking
    int max_stack = 1;
    bool is_stackable() const { return max_stack > 1; }

    // Weight/Value
    float weight = 0.0f;
    int base_value = 0;                     // Base sell price
    int buy_price = 0;                      // 0 = cannot buy

    // Equipment stats (base bonuses)
    std::vector<std::pair<stats::StatType, float>> stat_bonuses;

    // Scaling stats (added per item level/quality)
    std::vector<std::pair<stats::StatType, float>> stat_scaling;

    // Requirements to use/equip
    std::vector<ItemRequirement> requirements;

    // Consumable effects
    std::vector<std::string> apply_effects;                     // Effect IDs to apply
    std::vector<std::pair<stats::StatType, float>> instant_heals;  // Instant stat changes

    // Tags for filtering
    std::vector<std::string> tags;

    // Flags
    bool is_unique = false;                 // Only one can exist in inventory
    bool is_quest_item = false;             // Cannot drop/sell/destroy
    bool is_tradeable = true;
    bool is_sellable = true;
    bool is_droppable = true;
    bool destroys_on_use = true;            // Consumables: destroyed after use

    // Durability (0 = indestructible)
    int max_durability = 0;
    bool breaks_when_depleted = false;

    // Level range for random drops
    int min_level = 1;
    int max_level = 100;

    // ========================================================================
    // Helpers
    // ========================================================================

    bool is_equipment() const { return type == ItemType::Equipment; }
    bool is_consumable() const { return type == ItemType::Consumable; }
    bool is_weapon() const { return weapon_type != WeaponType::None; }
    bool is_armor() const { return armor_type != ArmorType::None; }
    bool has_requirements() const { return !requirements.empty(); }
    bool has_tag(const std::string& tag) const;
};

// ============================================================================
// Item Definition Registry
// ============================================================================

class ItemRegistry {
public:
    static ItemRegistry& instance();

    // Delete copy/move
    ItemRegistry(const ItemRegistry&) = delete;
    ItemRegistry& operator=(const ItemRegistry&) = delete;

    // Registration
    void register_item(const ItemDefinition& def);
    void load_items(const std::string& path);

    // Lookup
    const ItemDefinition* get(const std::string& item_id) const;
    bool exists(const std::string& item_id) const;

    // Queries
    std::vector<std::string> get_all_item_ids() const;
    std::vector<std::string> get_items_by_type(ItemType type) const;
    std::vector<std::string> get_items_by_rarity(ItemRarity rarity) const;
    std::vector<std::string> get_items_by_slot(EquipmentSlot slot) const;
    std::vector<std::string> get_items_by_tag(const std::string& tag) const;

    // Clear (for hot reload)
    void clear();

private:
    ItemRegistry() = default;
    ~ItemRegistry() = default;

    std::unordered_map<std::string, ItemDefinition> m_items;
};

// ============================================================================
// Global Access
// ============================================================================

inline ItemRegistry& item_registry() { return ItemRegistry::instance(); }

// ============================================================================
// Item Builder
// ============================================================================

class ItemBuilder {
public:
    ItemBuilder& id(const std::string& item_id);
    ItemBuilder& name(const std::string& display_name);
    ItemBuilder& description(const std::string& desc);
    ItemBuilder& lore(const std::string& text);
    ItemBuilder& icon(const std::string& path);
    ItemBuilder& mesh(const std::string& path);
    ItemBuilder& type(ItemType t);
    ItemBuilder& rarity(ItemRarity r);
    ItemBuilder& equipment(EquipmentSlot slot);
    ItemBuilder& weapon(WeaponType wtype);
    ItemBuilder& armor(ArmorType atype);
    ItemBuilder& stack(int max);
    ItemBuilder& weight(float w);
    ItemBuilder& value(int sell, int buy = 0);
    ItemBuilder& stat(stats::StatType stat, float value);
    ItemBuilder& require(stats::StatType stat, float min_value, const std::string& desc = "");
    ItemBuilder& effect(const std::string& effect_id);
    ItemBuilder& heal(stats::StatType stat, float amount);
    ItemBuilder& tag(const std::string& t);
    ItemBuilder& quest_item();
    ItemBuilder& unique();
    ItemBuilder& durability(int max, bool breaks = true);

    ItemDefinition build() const;
    void register_item() const;

private:
    ItemDefinition m_def;
};

// ============================================================================
// Convenience
// ============================================================================

inline ItemBuilder item() { return ItemBuilder{}; }

// ============================================================================
// Rarity Helpers
// ============================================================================

std::string get_rarity_name(ItemRarity rarity);
uint32_t get_rarity_color(ItemRarity rarity);  // RGBA

} // namespace engine::inventory
