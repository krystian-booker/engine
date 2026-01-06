#pragma once

#include <engine/inventory/item_definition.hpp>
#include <engine/stats/stat_modifier.hpp>
#include <engine/core/uuid.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace engine::inventory {

// ============================================================================
// Random Modifier Tier
// ============================================================================

enum class ModifierTier : uint8_t {
    Minor,      // Small bonus
    Lesser,
    Normal,
    Greater,
    Major       // Large bonus
};

// ============================================================================
// Item Random Modifier
// ============================================================================

struct ItemRandomModifier {
    stats::StatType stat;
    stats::ModifierType modifier_type;
    float value;
    ModifierTier tier;
    std::string prefix;         // "Sturdy" for defense bonus
    std::string suffix;         // "of Power" for strength bonus
};

// ============================================================================
// Item Instance - Runtime item with unique ID and modifiers
// ============================================================================

struct ItemInstance {
    core::UUID instance_id;             // Unique per item instance
    std::string definition_id;          // Reference to ItemDefinition

    int stack_count = 1;
    int item_level = 1;                 // For scaling stats
    int quality = 0;                    // 0-100, affects stat rolls

    // Durability
    int current_durability = -1;        // -1 = indestructible
    int max_durability = -1;

    // Random modifiers (for equipment)
    std::vector<ItemRandomModifier> random_modifiers;

    // Socket/enchant data
    std::vector<std::string> socket_gems;       // Item IDs of socketed gems
    std::vector<std::string> enchantments;      // Enchantment IDs

    // Custom data
    std::unordered_map<std::string, std::string> custom_data;

    // Binding
    bool is_bound = false;              // Soulbound to character
    core::UUID bound_to;                // Character UUID if bound

    // Timestamp
    uint64_t created_timestamp = 0;
    uint64_t acquired_timestamp = 0;

    // ========================================================================
    // Queries
    // ========================================================================

    const ItemDefinition* get_definition() const;
    bool is_valid() const { return !definition_id.empty(); }
    bool is_stackable() const;
    bool can_stack_with(const ItemInstance& other) const;
    bool is_equipment() const;
    bool is_consumable() const;
    bool is_broken() const;
    bool has_durability() const { return max_durability > 0; }

    // ========================================================================
    // Stack Operations
    // ========================================================================

    // Add to stack, returns overflow count
    int add_stack(int amount);

    // Remove from stack, returns actual removed
    int remove_stack(int amount);

    // Split stack, returns new instance with split amount
    ItemInstance split(int amount);

    // Get how many more can fit in this stack
    int get_stack_space() const;

    // ========================================================================
    // Durability
    // ========================================================================

    // Reduce durability, returns true if still usable
    bool reduce_durability(int amount = 1);

    // Repair durability
    void repair(int amount);
    void repair_full();

    // Get durability percentage (0.0 - 1.0)
    float get_durability_percent() const;

    // ========================================================================
    // Stats
    // ========================================================================

    // Get all stat modifiers (base + random)
    std::vector<stats::StatModifier> get_all_modifiers() const;

    // Get total bonus for a stat
    float get_stat_bonus(stats::StatType stat) const;

    // ========================================================================
    // Display
    // ========================================================================

    // Get display name (may include prefix/suffix from modifiers)
    std::string get_display_name() const;

    // Get tooltip lines
    std::vector<std::string> get_tooltip_lines() const;

    // ========================================================================
    // Factory
    // ========================================================================

    static ItemInstance create(const std::string& definition_id, int count = 1);
    static ItemInstance create_with_level(const std::string& definition_id, int level, int quality = 50);
    static ItemInstance create_random(const std::string& definition_id, int level, int modifier_count = 0);
};

// ============================================================================
// Item Instance Builder
// ============================================================================

class ItemInstanceBuilder {
public:
    ItemInstanceBuilder& from(const std::string& definition_id);
    ItemInstanceBuilder& count(int amount);
    ItemInstanceBuilder& level(int lvl);
    ItemInstanceBuilder& quality(int q);
    ItemInstanceBuilder& durability(int current, int max = -1);
    ItemInstanceBuilder& modifier(stats::StatType stat, float value, ModifierTier tier = ModifierTier::Normal);
    ItemInstanceBuilder& socket(const std::string& gem_id);
    ItemInstanceBuilder& enchant(const std::string& enchant_id);
    ItemInstanceBuilder& bind();
    ItemInstanceBuilder& custom(const std::string& key, const std::string& value);

    ItemInstance build() const;

private:
    ItemInstance m_item;
};

// ============================================================================
// Convenience
// ============================================================================

inline ItemInstanceBuilder create_item() { return ItemInstanceBuilder{}; }

// ============================================================================
// Loot Generation
// ============================================================================

struct LootTableEntry {
    std::string item_id;
    float weight = 1.0f;        // Relative drop chance
    int min_count = 1;
    int max_count = 1;
    int min_level = 1;
    int max_level = 100;
    float quality_bonus = 0.0f; // Added to roll quality
};

struct LootTable {
    std::string table_id;
    std::vector<LootTableEntry> entries;
    int guaranteed_drops = 0;   // Minimum items to drop
    int max_drops = 1;          // Maximum items to drop
    float nothing_chance = 0.0f; // Chance to drop nothing

    // Roll the table, returns item instances
    std::vector<ItemInstance> roll(int player_level = 1, float luck_bonus = 0.0f) const;
};

} // namespace engine::inventory
