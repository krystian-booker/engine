#include <engine/inventory/item_definition.hpp>
#include <engine/core/log.hpp>
#include <algorithm>

namespace engine::inventory {

// ============================================================================
// ItemDefinition
// ============================================================================

bool ItemDefinition::has_tag(const std::string& tag) const {
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

// ============================================================================
// ItemRegistry
// ============================================================================

ItemRegistry& ItemRegistry::instance() {
    static ItemRegistry s_instance;
    return s_instance;
}

void ItemRegistry::register_item(const ItemDefinition& def) {
    if (def.item_id.empty()) {
        core::log_error("inventory", "Cannot register item with empty ID");
        return;
    }

    if (m_items.contains(def.item_id)) {
        core::log_warning("inventory", "Overwriting existing item definition: {}", def.item_id);
    }

    m_items[def.item_id] = def;
    core::log_debug("inventory", "Registered item: {} ({})", def.item_id, def.display_name);
}

void ItemRegistry::load_items(const std::string& path) {
    // TODO: Load items from JSON file
    core::log_info("inventory", "Loading items from: {}", path);
}

const ItemDefinition* ItemRegistry::get(const std::string& item_id) const {
    auto it = m_items.find(item_id);
    if (it != m_items.end()) {
        return &it->second;
    }
    return nullptr;
}

bool ItemRegistry::exists(const std::string& item_id) const {
    return m_items.contains(item_id);
}

std::vector<std::string> ItemRegistry::get_all_item_ids() const {
    std::vector<std::string> result;
    result.reserve(m_items.size());
    for (const auto& [id, def] : m_items) {
        result.push_back(id);
    }
    return result;
}

std::vector<std::string> ItemRegistry::get_items_by_type(ItemType type) const {
    std::vector<std::string> result;
    for (const auto& [id, def] : m_items) {
        if (def.type == type) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<std::string> ItemRegistry::get_items_by_rarity(ItemRarity rarity) const {
    std::vector<std::string> result;
    for (const auto& [id, def] : m_items) {
        if (def.rarity == rarity) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<std::string> ItemRegistry::get_items_by_slot(EquipmentSlot slot) const {
    std::vector<std::string> result;
    for (const auto& [id, def] : m_items) {
        if (def.slot == slot) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<std::string> ItemRegistry::get_items_by_tag(const std::string& tag) const {
    std::vector<std::string> result;
    for (const auto& [id, def] : m_items) {
        if (def.has_tag(tag)) {
            result.push_back(id);
        }
    }
    return result;
}

void ItemRegistry::clear() {
    m_items.clear();
    core::log_info("inventory", "Cleared item registry");
}

// ============================================================================
// ItemBuilder
// ============================================================================

ItemBuilder& ItemBuilder::id(const std::string& item_id) {
    m_def.item_id = item_id;
    return *this;
}

ItemBuilder& ItemBuilder::name(const std::string& display_name) {
    m_def.display_name = display_name;
    return *this;
}

ItemBuilder& ItemBuilder::description(const std::string& desc) {
    m_def.description = desc;
    return *this;
}

ItemBuilder& ItemBuilder::lore(const std::string& text) {
    m_def.lore = text;
    return *this;
}

ItemBuilder& ItemBuilder::icon(const std::string& path) {
    m_def.icon_path = path;
    return *this;
}

ItemBuilder& ItemBuilder::mesh(const std::string& path) {
    m_def.mesh_path = path;
    return *this;
}

ItemBuilder& ItemBuilder::type(ItemType t) {
    m_def.type = t;
    return *this;
}

ItemBuilder& ItemBuilder::rarity(ItemRarity r) {
    m_def.rarity = r;
    return *this;
}

ItemBuilder& ItemBuilder::equipment(EquipmentSlot slot) {
    m_def.slot = slot;
    m_def.type = ItemType::Equipment;
    return *this;
}

ItemBuilder& ItemBuilder::weapon(WeaponType wtype) {
    m_def.weapon_type = wtype;
    m_def.type = ItemType::Equipment;
    return *this;
}

ItemBuilder& ItemBuilder::armor(ArmorType atype) {
    m_def.armor_type = atype;
    m_def.type = ItemType::Equipment;
    return *this;
}

ItemBuilder& ItemBuilder::stack(int max) {
    m_def.max_stack = max;
    return *this;
}

ItemBuilder& ItemBuilder::weight(float w) {
    m_def.weight = w;
    return *this;
}

ItemBuilder& ItemBuilder::value(int sell, int buy) {
    m_def.base_value = sell;
    m_def.buy_price = buy > 0 ? buy : sell * 2;  // Default markup
    return *this;
}

ItemBuilder& ItemBuilder::stat(stats::StatType stat, float value) {
    m_def.stat_bonuses.emplace_back(stat, value);
    return *this;
}

ItemBuilder& ItemBuilder::require(stats::StatType stat, float min_value, const std::string& desc) {
    ItemRequirement req;
    req.stat = stat;
    req.min_value = min_value;
    req.description = desc.empty() ?
        ("Requires " + std::to_string(static_cast<int>(min_value))) : desc;
    m_def.requirements.push_back(req);
    return *this;
}

ItemBuilder& ItemBuilder::effect(const std::string& effect_id) {
    m_def.apply_effects.push_back(effect_id);
    return *this;
}

ItemBuilder& ItemBuilder::heal(stats::StatType stat, float amount) {
    m_def.instant_heals.emplace_back(stat, amount);
    return *this;
}

ItemBuilder& ItemBuilder::tag(const std::string& t) {
    m_def.tags.push_back(t);
    return *this;
}

ItemBuilder& ItemBuilder::quest_item() {
    m_def.is_quest_item = true;
    m_def.is_tradeable = false;
    m_def.is_sellable = false;
    m_def.is_droppable = false;
    return *this;
}

ItemBuilder& ItemBuilder::unique() {
    m_def.is_unique = true;
    return *this;
}

ItemBuilder& ItemBuilder::durability(int max, bool breaks) {
    m_def.max_durability = max;
    m_def.breaks_when_depleted = breaks;
    return *this;
}

ItemDefinition ItemBuilder::build() const {
    return m_def;
}

void ItemBuilder::register_item() const {
    item_registry().register_item(m_def);
}

// ============================================================================
// Rarity Helpers
// ============================================================================

std::string get_rarity_name(ItemRarity rarity) {
    switch (rarity) {
        case ItemRarity::Common:    return "Common";
        case ItemRarity::Uncommon:  return "Uncommon";
        case ItemRarity::Rare:      return "Rare";
        case ItemRarity::Epic:      return "Epic";
        case ItemRarity::Legendary: return "Legendary";
        case ItemRarity::Unique:    return "Unique";
        case ItemRarity::Artifact:  return "Artifact";
        default:                    return "Unknown";
    }
}

uint32_t get_rarity_color(ItemRarity rarity) {
    switch (rarity) {
        case ItemRarity::Common:    return 0xFFFFFFFF;  // White
        case ItemRarity::Uncommon:  return 0xFF00FF00;  // Green
        case ItemRarity::Rare:      return 0xFF0088FF;  // Blue
        case ItemRarity::Epic:      return 0xFFAA00FF;  // Purple
        case ItemRarity::Legendary: return 0xFFFF8800;  // Orange
        case ItemRarity::Unique:    return 0xFFFFD700;  // Gold
        case ItemRarity::Artifact:  return 0xFFFF0000;  // Red
        default:                    return 0xFFFFFFFF;
    }
}

} // namespace engine::inventory
