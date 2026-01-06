#pragma once

#include <string>
#include <cstdint>
#include <unordered_map>

namespace engine::stats {

// ============================================================================
// StatType - Enum for all stat types
// ============================================================================

enum class StatType : uint8_t {
    // Resource stats (depletable)
    Health = 0,
    MaxHealth,
    HealthRegen,
    Stamina,
    MaxStamina,
    StaminaRegen,
    Mana,
    MaxMana,
    ManaRegen,

    // Primary attributes
    Strength,
    Dexterity,
    Intelligence,
    Vitality,
    Luck,
    Endurance,
    Agility,
    Wisdom,
    Charisma,

    // Combat stats
    PhysicalDamage,
    MagicDamage,
    PhysicalDefense,
    MagicDefense,
    CritChance,
    CritDamage,
    ArmorPenetration,
    MagicPenetration,

    // Movement and speed
    MoveSpeed,
    AttackSpeed,
    CastSpeed,
    CooldownReduction,

    // Defensive stats
    DodgeChance,
    BlockChance,
    BlockAmount,
    Poise,
    PoiseRegen,

    // Resistance stats
    FireResistance,
    IceResistance,
    LightningResistance,
    PoisonResistance,
    BleedResistance,

    // Misc
    ExperienceGain,
    GoldFind,
    ItemFind,
    CarryCapacity,

    // Reserved for game-specific stats
    Custom = 128,

    Count = 255
};

// ============================================================================
// StatCategory - Logical grouping for UI
// ============================================================================

enum class StatCategory : uint8_t {
    Resource,       // Health, Stamina, Mana
    Attribute,      // Str, Dex, Int, etc.
    Offense,        // Damage, Crit, Penetration
    Defense,        // Armor, Block, Dodge
    Resistance,     // Elemental resistances
    Utility,        // Movement, Cooldown, Experience
    Custom
};

// ============================================================================
// StatDefinition - Metadata about a stat type
// ============================================================================

struct StatDefinition {
    StatType type = StatType::Health;
    std::string internal_name;          // "max_health"
    std::string display_name;           // "Maximum Health"
    std::string abbreviation;           // "HP"
    std::string description;
    std::string icon_path;

    StatCategory category = StatCategory::Attribute;

    float default_value = 0.0f;
    float min_value = 0.0f;
    float max_value = 999999.0f;

    // For resource stats, which max stat limits this one
    StatType max_stat = StatType::Count;    // Count = no max

    // Display formatting
    bool is_percentage = false;         // Display as 50% vs 50
    int decimal_places = 0;             // 0 for integers, 1-2 for floats
    bool higher_is_better = true;       // For UI coloring

    // Derived stat calculation (stat = base_stat * multiplier + flat)
    StatType derived_from = StatType::Count;
    float derived_multiplier = 0.0f;
    float derived_flat = 0.0f;
};

// ============================================================================
// StatRegistry - Central registry for all stat definitions
// ============================================================================

class StatRegistry {
public:
    static StatRegistry& instance();

    // Delete copy/move
    StatRegistry(const StatRegistry&) = delete;
    StatRegistry& operator=(const StatRegistry&) = delete;
    StatRegistry(StatRegistry&&) = delete;
    StatRegistry& operator=(StatRegistry&&) = delete;

    // Registration
    void register_stat(const StatDefinition& def);
    void register_builtin_stats();

    // Lookup
    const StatDefinition* get_definition(StatType type) const;
    const StatDefinition* get_definition(const std::string& name) const;
    StatType get_type_by_name(const std::string& name) const;

    // Custom stat registration (returns assigned StatType)
    StatType register_custom_stat(const StatDefinition& def);

    // Queries
    std::vector<StatType> get_stats_by_category(StatCategory category) const;
    std::vector<StatType> get_all_registered_stats() const;
    bool is_registered(StatType type) const;

    // Category info
    std::string get_category_name(StatCategory category) const;

private:
    StatRegistry();
    ~StatRegistry() = default;

    std::unordered_map<StatType, StatDefinition> m_definitions;
    std::unordered_map<std::string, StatType> m_name_to_type;
    uint8_t m_next_custom_id = static_cast<uint8_t>(StatType::Custom);
};

// ============================================================================
// Global Access
// ============================================================================

inline StatRegistry& stat_registry() { return StatRegistry::instance(); }

// ============================================================================
// Helper Functions
// ============================================================================

// Get display string for a stat value
std::string format_stat_value(StatType type, float value);

// Check if stat is a resource (depletable)
bool is_resource_stat(StatType type);

// Check if stat is a max stat (MaxHealth, MaxStamina, MaxMana)
bool is_max_stat(StatType type);

// Get the current value stat for a max stat (MaxHealth -> Health)
StatType get_resource_stat(StatType max_stat);

// Get the max stat for a resource stat (Health -> MaxHealth)
StatType get_max_stat(StatType resource_stat);

} // namespace engine::stats
