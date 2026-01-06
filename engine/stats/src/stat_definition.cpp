#include <engine/stats/stat_definition.hpp>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace engine::stats {

// ============================================================================
// StatRegistry Implementation
// ============================================================================

StatRegistry& StatRegistry::instance() {
    static StatRegistry s_instance;
    return s_instance;
}

StatRegistry::StatRegistry() {
    register_builtin_stats();
}

void StatRegistry::register_stat(const StatDefinition& def) {
    m_definitions[def.type] = def;
    m_name_to_type[def.internal_name] = def.type;
}

void StatRegistry::register_builtin_stats() {
    // Resource stats
    register_stat({StatType::Health, "health", "Health", "HP", "Current health points",
                   "", StatCategory::Resource, 100.0f, 0.0f, 999999.0f, StatType::MaxHealth});
    register_stat({StatType::MaxHealth, "max_health", "Maximum Health", "Max HP", "Maximum health points",
                   "", StatCategory::Resource, 100.0f, 1.0f, 999999.0f});
    register_stat({StatType::HealthRegen, "health_regen", "Health Regeneration", "HP/s", "Health regenerated per second",
                   "", StatCategory::Resource, 0.0f, 0.0f, 9999.0f});

    register_stat({StatType::Stamina, "stamina", "Stamina", "SP", "Current stamina points",
                   "", StatCategory::Resource, 100.0f, 0.0f, 999999.0f, StatType::MaxStamina});
    register_stat({StatType::MaxStamina, "max_stamina", "Maximum Stamina", "Max SP", "Maximum stamina points",
                   "", StatCategory::Resource, 100.0f, 1.0f, 999999.0f});
    register_stat({StatType::StaminaRegen, "stamina_regen", "Stamina Regeneration", "SP/s", "Stamina regenerated per second",
                   "", StatCategory::Resource, 5.0f, 0.0f, 9999.0f});

    register_stat({StatType::Mana, "mana", "Mana", "MP", "Current mana points",
                   "", StatCategory::Resource, 100.0f, 0.0f, 999999.0f, StatType::MaxMana});
    register_stat({StatType::MaxMana, "max_mana", "Maximum Mana", "Max MP", "Maximum mana points",
                   "", StatCategory::Resource, 100.0f, 0.0f, 999999.0f});
    register_stat({StatType::ManaRegen, "mana_regen", "Mana Regeneration", "MP/s", "Mana regenerated per second",
                   "", StatCategory::Resource, 1.0f, 0.0f, 9999.0f});

    // Primary attributes
    register_stat({StatType::Strength, "strength", "Strength", "STR", "Physical power, affects melee damage",
                   "", StatCategory::Attribute, 10.0f, 0.0f, 999.0f});
    register_stat({StatType::Dexterity, "dexterity", "Dexterity", "DEX", "Agility and precision, affects attack speed and crit",
                   "", StatCategory::Attribute, 10.0f, 0.0f, 999.0f});
    register_stat({StatType::Intelligence, "intelligence", "Intelligence", "INT", "Mental acuity, affects magic damage and mana",
                   "", StatCategory::Attribute, 10.0f, 0.0f, 999.0f});
    register_stat({StatType::Vitality, "vitality", "Vitality", "VIT", "Constitution, affects health and defense",
                   "", StatCategory::Attribute, 10.0f, 0.0f, 999.0f});
    register_stat({StatType::Luck, "luck", "Luck", "LCK", "Fortune, affects item drops and critical hits",
                   "", StatCategory::Attribute, 10.0f, 0.0f, 999.0f});
    register_stat({StatType::Endurance, "endurance", "Endurance", "END", "Physical stamina, affects stamina pool",
                   "", StatCategory::Attribute, 10.0f, 0.0f, 999.0f});
    register_stat({StatType::Agility, "agility", "Agility", "AGI", "Speed and evasion",
                   "", StatCategory::Attribute, 10.0f, 0.0f, 999.0f});
    register_stat({StatType::Wisdom, "wisdom", "Wisdom", "WIS", "Magical insight, affects mana regen",
                   "", StatCategory::Attribute, 10.0f, 0.0f, 999.0f});
    register_stat({StatType::Charisma, "charisma", "Charisma", "CHA", "Social influence, affects prices and dialogue",
                   "", StatCategory::Attribute, 10.0f, 0.0f, 999.0f});

    // Combat stats
    register_stat({StatType::PhysicalDamage, "physical_damage", "Physical Damage", "P.ATK", "Base physical attack power",
                   "", StatCategory::Offense, 10.0f, 0.0f, 99999.0f});
    register_stat({StatType::MagicDamage, "magic_damage", "Magic Damage", "M.ATK", "Base magic attack power",
                   "", StatCategory::Offense, 10.0f, 0.0f, 99999.0f});
    register_stat({StatType::PhysicalDefense, "physical_defense", "Physical Defense", "P.DEF", "Reduces physical damage taken",
                   "", StatCategory::Defense, 10.0f, 0.0f, 99999.0f});
    register_stat({StatType::MagicDefense, "magic_defense", "Magic Defense", "M.DEF", "Reduces magic damage taken",
                   "", StatCategory::Defense, 10.0f, 0.0f, 99999.0f});

    StatDefinition crit_chance = {StatType::CritChance, "crit_chance", "Critical Chance", "CRIT%", "Chance to deal critical damage",
                                   "", StatCategory::Offense, 5.0f, 0.0f, 100.0f};
    crit_chance.is_percentage = true;
    crit_chance.decimal_places = 1;
    register_stat(crit_chance);

    StatDefinition crit_damage = {StatType::CritDamage, "crit_damage", "Critical Damage", "CDMG%", "Critical hit damage multiplier",
                                   "", StatCategory::Offense, 150.0f, 100.0f, 1000.0f};
    crit_damage.is_percentage = true;
    register_stat(crit_damage);

    register_stat({StatType::ArmorPenetration, "armor_penetration", "Armor Penetration", "PEN", "Ignores enemy physical defense",
                   "", StatCategory::Offense, 0.0f, 0.0f, 99999.0f});
    register_stat({StatType::MagicPenetration, "magic_penetration", "Magic Penetration", "M.PEN", "Ignores enemy magic defense",
                   "", StatCategory::Offense, 0.0f, 0.0f, 99999.0f});

    // Movement and speed
    StatDefinition move_speed = {StatType::MoveSpeed, "move_speed", "Movement Speed", "SPD%", "Movement speed multiplier",
                                  "", StatCategory::Utility, 100.0f, 0.0f, 500.0f};
    move_speed.is_percentage = true;
    register_stat(move_speed);

    StatDefinition attack_speed = {StatType::AttackSpeed, "attack_speed", "Attack Speed", "AS%", "Attack speed multiplier",
                                    "", StatCategory::Offense, 100.0f, 0.0f, 500.0f};
    attack_speed.is_percentage = true;
    register_stat(attack_speed);

    StatDefinition cast_speed = {StatType::CastSpeed, "cast_speed", "Cast Speed", "CS%", "Spell casting speed multiplier",
                                  "", StatCategory::Offense, 100.0f, 0.0f, 500.0f};
    cast_speed.is_percentage = true;
    register_stat(cast_speed);

    StatDefinition cooldown_reduction = {StatType::CooldownReduction, "cooldown_reduction", "Cooldown Reduction", "CDR%", "Reduces ability cooldowns",
                                          "", StatCategory::Utility, 0.0f, 0.0f, 80.0f};
    cooldown_reduction.is_percentage = true;
    cooldown_reduction.decimal_places = 1;
    register_stat(cooldown_reduction);

    // Defensive stats
    StatDefinition dodge_chance = {StatType::DodgeChance, "dodge_chance", "Dodge Chance", "DODGE%", "Chance to completely avoid an attack",
                                    "", StatCategory::Defense, 0.0f, 0.0f, 75.0f};
    dodge_chance.is_percentage = true;
    dodge_chance.decimal_places = 1;
    register_stat(dodge_chance);

    StatDefinition block_chance = {StatType::BlockChance, "block_chance", "Block Chance", "BLK%", "Chance to block incoming attacks",
                                    "", StatCategory::Defense, 0.0f, 0.0f, 100.0f};
    block_chance.is_percentage = true;
    block_chance.decimal_places = 1;
    register_stat(block_chance);

    register_stat({StatType::BlockAmount, "block_amount", "Block Amount", "BLK", "Damage blocked when blocking",
                   "", StatCategory::Defense, 0.0f, 0.0f, 99999.0f});
    register_stat({StatType::Poise, "poise", "Poise", "POISE", "Current poise (stagger resistance)",
                   "", StatCategory::Defense, 100.0f, 0.0f, 9999.0f});
    register_stat({StatType::PoiseRegen, "poise_regen", "Poise Regeneration", "POISE/s", "Poise regenerated per second",
                   "", StatCategory::Defense, 10.0f, 0.0f, 9999.0f});

    // Resistance stats
    StatDefinition fire_res = {StatType::FireResistance, "fire_resistance", "Fire Resistance", "FIRE%", "Reduces fire damage taken",
                                "", StatCategory::Resistance, 0.0f, -100.0f, 100.0f};
    fire_res.is_percentage = true;
    register_stat(fire_res);

    StatDefinition ice_res = {StatType::IceResistance, "ice_resistance", "Ice Resistance", "ICE%", "Reduces ice damage taken",
                               "", StatCategory::Resistance, 0.0f, -100.0f, 100.0f};
    ice_res.is_percentage = true;
    register_stat(ice_res);

    StatDefinition lightning_res = {StatType::LightningResistance, "lightning_resistance", "Lightning Resistance", "LTNG%", "Reduces lightning damage taken",
                                     "", StatCategory::Resistance, 0.0f, -100.0f, 100.0f};
    lightning_res.is_percentage = true;
    register_stat(lightning_res);

    StatDefinition poison_res = {StatType::PoisonResistance, "poison_resistance", "Poison Resistance", "POIS%", "Reduces poison damage and duration",
                                  "", StatCategory::Resistance, 0.0f, -100.0f, 100.0f};
    poison_res.is_percentage = true;
    register_stat(poison_res);

    StatDefinition bleed_res = {StatType::BleedResistance, "bleed_resistance", "Bleed Resistance", "BLD%", "Reduces bleed damage buildup",
                                 "", StatCategory::Resistance, 0.0f, -100.0f, 100.0f};
    bleed_res.is_percentage = true;
    register_stat(bleed_res);

    // Misc
    StatDefinition exp_gain = {StatType::ExperienceGain, "experience_gain", "Experience Gain", "EXP%", "Experience gained multiplier",
                                "", StatCategory::Utility, 100.0f, 0.0f, 1000.0f};
    exp_gain.is_percentage = true;
    register_stat(exp_gain);

    StatDefinition gold_find = {StatType::GoldFind, "gold_find", "Gold Find", "GOLD%", "Gold dropped multiplier",
                                 "", StatCategory::Utility, 100.0f, 0.0f, 1000.0f};
    gold_find.is_percentage = true;
    register_stat(gold_find);

    StatDefinition item_find = {StatType::ItemFind, "item_find", "Item Find", "ITEM%", "Item drop rate multiplier",
                                 "", StatCategory::Utility, 100.0f, 0.0f, 1000.0f};
    item_find.is_percentage = true;
    register_stat(item_find);

    register_stat({StatType::CarryCapacity, "carry_capacity", "Carry Capacity", "CAP", "Maximum weight that can be carried",
                   "", StatCategory::Utility, 100.0f, 0.0f, 99999.0f});
}

const StatDefinition* StatRegistry::get_definition(StatType type) const {
    auto it = m_definitions.find(type);
    return it != m_definitions.end() ? &it->second : nullptr;
}

const StatDefinition* StatRegistry::get_definition(const std::string& name) const {
    auto type_it = m_name_to_type.find(name);
    if (type_it == m_name_to_type.end()) return nullptr;
    return get_definition(type_it->second);
}

StatType StatRegistry::get_type_by_name(const std::string& name) const {
    auto it = m_name_to_type.find(name);
    return it != m_name_to_type.end() ? it->second : StatType::Count;
}

StatType StatRegistry::register_custom_stat(const StatDefinition& def) {
    StatDefinition custom_def = def;
    custom_def.type = static_cast<StatType>(m_next_custom_id++);
    register_stat(custom_def);
    return custom_def.type;
}

std::vector<StatType> StatRegistry::get_stats_by_category(StatCategory category) const {
    std::vector<StatType> result;
    for (const auto& [type, def] : m_definitions) {
        if (def.category == category) {
            result.push_back(type);
        }
    }
    return result;
}

std::vector<StatType> StatRegistry::get_all_registered_stats() const {
    std::vector<StatType> result;
    result.reserve(m_definitions.size());
    for (const auto& [type, def] : m_definitions) {
        result.push_back(type);
    }
    return result;
}

bool StatRegistry::is_registered(StatType type) const {
    return m_definitions.find(type) != m_definitions.end();
}

std::string StatRegistry::get_category_name(StatCategory category) const {
    switch (category) {
        case StatCategory::Resource: return "Resources";
        case StatCategory::Attribute: return "Attributes";
        case StatCategory::Offense: return "Offense";
        case StatCategory::Defense: return "Defense";
        case StatCategory::Resistance: return "Resistances";
        case StatCategory::Utility: return "Utility";
        case StatCategory::Custom: return "Custom";
        default: return "Unknown";
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string format_stat_value(StatType type, float value) {
    const StatDefinition* def = stat_registry().get_definition(type);
    if (!def) {
        return std::to_string(static_cast<int>(value));
    }

    std::ostringstream oss;
    if (def->decimal_places == 0) {
        oss << static_cast<int>(std::round(value));
    } else {
        oss << std::fixed << std::setprecision(def->decimal_places) << value;
    }

    if (def->is_percentage) {
        oss << "%";
    }

    return oss.str();
}

bool is_resource_stat(StatType type) {
    return type == StatType::Health ||
           type == StatType::Stamina ||
           type == StatType::Mana ||
           type == StatType::Poise;
}

bool is_max_stat(StatType type) {
    return type == StatType::MaxHealth ||
           type == StatType::MaxStamina ||
           type == StatType::MaxMana;
}

StatType get_resource_stat(StatType max_stat) {
    switch (max_stat) {
        case StatType::MaxHealth: return StatType::Health;
        case StatType::MaxStamina: return StatType::Stamina;
        case StatType::MaxMana: return StatType::Mana;
        default: return StatType::Count;
    }
}

StatType get_max_stat(StatType resource_stat) {
    switch (resource_stat) {
        case StatType::Health: return StatType::MaxHealth;
        case StatType::Stamina: return StatType::MaxStamina;
        case StatType::Mana: return StatType::MaxMana;
        default: return StatType::Count;
    }
}

} // namespace engine::stats
