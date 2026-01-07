#include <engine/inventory/item_instance.hpp>
#include <engine/core/log.hpp>
#include <algorithm>
#include <random>
#include <ctime>

namespace engine::inventory {

// ============================================================================
// ItemInstance
// ============================================================================

const ItemDefinition* ItemInstance::get_definition() const {
    return item_registry().get(definition_id);
}

bool ItemInstance::is_stackable() const {
    const auto* def = get_definition();
    return def && def->is_stackable();
}

bool ItemInstance::can_stack_with(const ItemInstance& other) const {
    if (definition_id != other.definition_id) return false;
    if (!is_stackable()) return false;

    // Items with random modifiers can't stack
    if (!random_modifiers.empty() || !other.random_modifiers.empty()) return false;

    // Items with different custom data can't stack
    if (custom_data != other.custom_data) return false;

    // Bound items can't stack with unbound
    if (is_bound != other.is_bound) return false;

    return true;
}

bool ItemInstance::is_equipment() const {
    const auto* def = get_definition();
    return def && def->is_equipment();
}

bool ItemInstance::is_consumable() const {
    const auto* def = get_definition();
    return def && def->is_consumable();
}

bool ItemInstance::is_broken() const {
    return has_durability() && current_durability <= 0;
}

int ItemInstance::add_stack(int amount) {
    const auto* def = get_definition();
    if (!def) return amount;

    int space = def->max_stack - stack_count;
    int to_add = std::min(amount, space);
    stack_count += to_add;
    return amount - to_add;  // Return overflow
}

int ItemInstance::remove_stack(int amount) {
    int to_remove = std::min(amount, stack_count);
    stack_count -= to_remove;
    return to_remove;
}

ItemInstance ItemInstance::split(int amount) {
    ItemInstance result = *this;
    int actual = std::min(amount, stack_count - 1);  // Keep at least 1

    if (actual <= 0) {
        result.stack_count = 0;
        return result;
    }

    result.stack_count = actual;
    result.instance_id = core::UUID::generate();  // New unique ID
    stack_count -= actual;

    return result;
}

int ItemInstance::get_stack_space() const {
    const auto* def = get_definition();
    if (!def) return 0;
    return def->max_stack - stack_count;
}

bool ItemInstance::reduce_durability(int amount) {
    if (!has_durability()) return true;

    current_durability = std::max(0, current_durability - amount);
    return current_durability > 0;
}

void ItemInstance::repair(int amount) {
    if (!has_durability()) return;
    current_durability = std::min(max_durability, current_durability + amount);
}

void ItemInstance::repair_full() {
    if (has_durability()) {
        current_durability = max_durability;
    }
}

float ItemInstance::get_durability_percent() const {
    if (!has_durability() || max_durability <= 0) return 1.0f;
    return static_cast<float>(current_durability) / static_cast<float>(max_durability);
}

std::vector<stats::StatModifier> ItemInstance::get_all_modifiers() const {
    std::vector<stats::StatModifier> result;

    const auto* def = get_definition();
    if (!def) return result;

    // Base stat bonuses from definition
    for (const auto& [stat, value] : def->stat_bonuses) {
        stats::StatModifier mod;
        mod.stat = stat;
        mod.type = stats::ModifierType::Flat;
        mod.value = value;
        mod.source = stats::ModifierSource::Equipment;
        mod.source_id = definition_id;
        result.push_back(mod);
    }

    // Scaling stats based on item level
    for (const auto& [stat, value] : def->stat_scaling) {
        float scaled_value = value * static_cast<float>(item_level);
        float quality_mult = 1.0f + (static_cast<float>(quality) - 50.0f) / 100.0f;  // 50% = 1x
        scaled_value *= quality_mult;

        stats::StatModifier mod;
        mod.stat = stat;
        mod.type = stats::ModifierType::Flat;
        mod.value = scaled_value;
        mod.source = stats::ModifierSource::Equipment;
        mod.source_id = definition_id + "_scaling";
        result.push_back(mod);
    }

    // Random modifiers
    for (const auto& rand_mod : random_modifiers) {
        stats::StatModifier mod;
        mod.stat = rand_mod.stat;
        mod.type = rand_mod.modifier_type;
        mod.value = rand_mod.value;
        mod.source = stats::ModifierSource::Equipment;
        mod.source_id = definition_id + "_random";
        result.push_back(mod);
    }

    return result;
}

float ItemInstance::get_stat_bonus(stats::StatType stat) const {
    float total = 0.0f;
    for (const auto& mod : get_all_modifiers()) {
        if (mod.stat == stat && mod.type == stats::ModifierType::Flat) {
            total += mod.value;
        }
    }
    return total;
}

std::string ItemInstance::get_display_name() const {
    const auto* def = get_definition();
    if (!def) return "Unknown Item";

    std::string name;

    // Add prefix from random modifiers
    for (const auto& mod : random_modifiers) {
        if (!mod.prefix.empty()) {
            name = mod.prefix + " ";
            break;  // Only use first prefix
        }
    }

    name += def->display_name;

    // Add suffix from random modifiers
    for (const auto& mod : random_modifiers) {
        if (!mod.suffix.empty()) {
            name += " " + mod.suffix;
            break;  // Only use first suffix
        }
    }

    return name;
}

std::vector<std::string> ItemInstance::get_tooltip_lines() const {
    std::vector<std::string> lines;

    const auto* def = get_definition();
    if (!def) {
        lines.push_back("Unknown Item");
        return lines;
    }

    // Name and rarity
    lines.push_back(get_display_name());
    lines.push_back(get_rarity_name(def->rarity));

    // Item type/slot
    if (def->is_equipment()) {
        lines.push_back("");  // Blank line
        // Could add slot name here
    }

    // Stats
    auto mods = get_all_modifiers();
    if (!mods.empty()) {
        lines.push_back("");
        for (const auto& mod : mods) {
            char sign = mod.value >= 0 ? '+' : '-';
            std::string stat_name = "Stat";  // Would use stat_registry().get_name(mod.stat)
            lines.push_back(std::string(1, sign) + std::to_string(static_cast<int>(std::abs(mod.value))) + " " + stat_name);
        }
    }

    // Durability
    if (has_durability()) {
        lines.push_back("");
        lines.push_back("Durability: " + std::to_string(current_durability) + "/" + std::to_string(max_durability));
    }

    // Description
    if (!def->description.empty()) {
        lines.push_back("");
        lines.push_back(def->description);
    }

    // Lore
    if (!def->lore.empty()) {
        lines.push_back("");
        lines.push_back("\"" + def->lore + "\"");
    }

    // Value
    if (def->base_value > 0) {
        lines.push_back("");
        lines.push_back("Sell: " + std::to_string(def->base_value));
    }

    return lines;
}

// ============================================================================
// Factory Methods
// ============================================================================

ItemInstance ItemInstance::create(const std::string& definition_id, int count) {
    ItemInstance item;
    item.instance_id = core::UUID::generate();
    item.definition_id = definition_id;
    item.stack_count = count;
    item.item_level = 1;
    item.quality = 50;

    const auto* def = item_registry().get(definition_id);
    if (def && def->max_durability > 0) {
        item.max_durability = def->max_durability;
        item.current_durability = def->max_durability;
    }

    item.created_timestamp = static_cast<uint64_t>(std::time(nullptr));
    item.acquired_timestamp = item.created_timestamp;

    return item;
}

ItemInstance ItemInstance::create_with_level(const std::string& definition_id, int level, int quality) {
    ItemInstance item = create(definition_id, 1);
    item.item_level = level;
    item.quality = quality;
    return item;
}

ItemInstance ItemInstance::create_random(const std::string& definition_id, int level, int modifier_count) {
    ItemInstance item = create_with_level(definition_id, level);

    if (modifier_count <= 0) return item;

    // Random number generator
    static std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));

    // Available stats for random modifiers
    std::vector<stats::StatType> available_stats = {
        stats::StatType::Strength,
        stats::StatType::Dexterity,
        stats::StatType::Intelligence,
        stats::StatType::Vitality,
        stats::StatType::PhysicalDamage,
        stats::StatType::MagicDamage,
        stats::StatType::PhysicalDefense,
        stats::StatType::MagicDefense,
        stats::StatType::CritChance,
        stats::StatType::CritDamage,
        stats::StatType::AttackSpeed,
        stats::StatType::MoveSpeed
    };

    std::uniform_int_distribution<size_t> stat_dist(0, available_stats.size() - 1);
    std::uniform_int_distribution<int> tier_dist(0, 4);
    std::uniform_real_distribution<float> value_dist(0.5f, 1.5f);

    for (int i = 0; i < modifier_count; ++i) {
        ItemRandomModifier mod;
        mod.stat = available_stats[stat_dist(rng)];
        mod.modifier_type = stats::ModifierType::Flat;
        mod.tier = static_cast<ModifierTier>(tier_dist(rng));

        // Base value scaled by tier and level
        float tier_mult = 1.0f + static_cast<float>(static_cast<int>(mod.tier)) * 0.5f;
        mod.value = static_cast<float>(level) * tier_mult * value_dist(rng);

        item.random_modifiers.push_back(mod);
    }

    return item;
}

// ============================================================================
// ItemInstanceBuilder
// ============================================================================

ItemInstanceBuilder& ItemInstanceBuilder::from(const std::string& definition_id) {
    m_item = ItemInstance::create(definition_id, 1);
    return *this;
}

ItemInstanceBuilder& ItemInstanceBuilder::count(int amount) {
    m_item.stack_count = amount;
    return *this;
}

ItemInstanceBuilder& ItemInstanceBuilder::level(int lvl) {
    m_item.item_level = lvl;
    return *this;
}

ItemInstanceBuilder& ItemInstanceBuilder::quality(int q) {
    m_item.quality = q;
    return *this;
}

ItemInstanceBuilder& ItemInstanceBuilder::durability(int current, int max) {
    m_item.current_durability = current;
    m_item.max_durability = max > 0 ? max : current;
    return *this;
}

ItemInstanceBuilder& ItemInstanceBuilder::modifier(stats::StatType stat, float value, ModifierTier tier) {
    ItemRandomModifier mod;
    mod.stat = stat;
    mod.modifier_type = stats::ModifierType::Flat;
    mod.value = value;
    mod.tier = tier;
    m_item.random_modifiers.push_back(mod);
    return *this;
}

ItemInstanceBuilder& ItemInstanceBuilder::socket(const std::string& gem_id) {
    m_item.socket_gems.push_back(gem_id);
    return *this;
}

ItemInstanceBuilder& ItemInstanceBuilder::enchant(const std::string& enchant_id) {
    m_item.enchantments.push_back(enchant_id);
    return *this;
}

ItemInstanceBuilder& ItemInstanceBuilder::bind() {
    m_item.is_bound = true;
    return *this;
}

ItemInstanceBuilder& ItemInstanceBuilder::custom(const std::string& key, const std::string& value) {
    m_item.custom_data[key] = value;
    return *this;
}

ItemInstance ItemInstanceBuilder::build() const {
    return m_item;
}

// ============================================================================
// LootTable
// ============================================================================

std::vector<ItemInstance> LootTable::roll(int player_level, float luck_bonus) const {
    std::vector<ItemInstance> result;

    static std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);

    // Check for nothing drop
    if (roll(rng) < nothing_chance) {
        return result;
    }

    // Calculate total weight of valid entries
    float total_weight = 0.0f;
    std::vector<const LootTableEntry*> valid_entries;

    for (const auto& entry : entries) {
        if (player_level >= entry.min_level && player_level <= entry.max_level) {
            valid_entries.push_back(&entry);
            total_weight += entry.weight * (1.0f + luck_bonus);
        }
    }

    if (valid_entries.empty() || total_weight <= 0.0f) {
        return result;
    }

    // Determine number of drops
    std::uniform_int_distribution<int> drop_count_dist(guaranteed_drops, max_drops);
    int num_drops = drop_count_dist(rng);

    // Roll for each drop
    for (int i = 0; i < num_drops; ++i) {
        float roll_value = roll(rng) * total_weight;
        float cumulative = 0.0f;

        for (const auto* entry : valid_entries) {
            cumulative += entry->weight * (1.0f + luck_bonus);
            if (roll_value <= cumulative) {
                // Selected this entry
                std::uniform_int_distribution<int> count_dist(entry->min_count, entry->max_count);
                int count = count_dist(rng);

                int item_level = player_level;
                int quality = 50 + static_cast<int>(entry->quality_bonus);

                ItemInstance item = ItemInstance::create_with_level(entry->item_id, item_level, quality);
                item.stack_count = count;

                result.push_back(std::move(item));
                break;
            }
        }
    }

    return result;
}

} // namespace engine::inventory
