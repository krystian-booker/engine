#pragma once

#include <engine/stats/stat_definition.hpp>
#include <engine/stats/stat_modifier.hpp>
#include <engine/scene/entity.hpp>
#include <string>

namespace engine::stats {

// ============================================================================
// Stat Change Events
// ============================================================================

// Base value changed
struct StatBaseChangedEvent {
    scene::Entity entity;
    StatType stat;
    float old_value;
    float new_value;
    float delta;                // new_value - old_value
};

// Final (calculated) value changed
struct StatFinalChangedEvent {
    scene::Entity entity;
    StatType stat;
    float old_value;
    float new_value;
    float delta;
};

// Current resource value changed (Health, Stamina, Mana)
struct ResourceChangedEvent {
    scene::Entity entity;
    StatType resource;
    float old_value;
    float new_value;
    float max_value;
    float delta;
    float old_percent;          // 0.0 - 1.0
    float new_percent;
};

// ============================================================================
// Resource Threshold Events
// ============================================================================

// Resource depleted (reached zero)
struct ResourceDepletedEvent {
    scene::Entity entity;
    StatType resource;
    float max_value;
};

// Resource filled (reached max)
struct ResourceFilledEvent {
    scene::Entity entity;
    StatType resource;
    float max_value;
};

// Resource crossed a percentage threshold
struct ResourceThresholdEvent {
    scene::Entity entity;
    StatType resource;
    float threshold;            // The threshold that was crossed (0.25, 0.5, 0.75)
    bool crossed_below;         // True if went below, false if went above
    float current_percent;
};

// ============================================================================
// Damage/Healing Events
// ============================================================================

// Health specifically was reduced
struct DamagedEvent {
    scene::Entity entity;
    scene::Entity source;       // Who/what caused it (NullEntity if environmental)
    float amount;               // Positive value
    float remaining_health;
    float max_health;
    std::string damage_source;  // "fire", "fall", "enemy_attack", etc.
    bool is_lethal;             // Would this kill the entity?
};

// Health specifically was increased
struct HealedEvent {
    scene::Entity entity;
    scene::Entity source;
    float amount;
    float remaining_health;
    float max_health;
    std::string heal_source;
};

// Entity died (health reached zero)
struct DeathEvent {
    scene::Entity entity;
    scene::Entity killer;       // NullEntity if environmental/self
    std::string cause;          // "fire_damage", "fall_damage", "enemy_attack"
    float overkill_amount;      // How much over zero
};

// Entity was revived
struct RevivedEvent {
    scene::Entity entity;
    scene::Entity reviver;      // Who revived them
    float revive_health;        // Health after revive
    float max_health;
};

// ============================================================================
// Modifier Events
// ============================================================================

// Modifier was added
struct ModifierAddedEvent {
    scene::Entity entity;
    StatType stat;
    StatModifier modifier;
    float old_final_value;
    float new_final_value;
};

// Modifier was removed
struct ModifierRemovedEvent {
    scene::Entity entity;
    StatType stat;
    StatModifier modifier;
    float old_final_value;
    float new_final_value;
    bool expired;               // True if removed due to duration expiring
};

// Modifier expired (subset of removed)
struct ModifierExpiredEvent {
    scene::Entity entity;
    StatType stat;
    StatModifier modifier;
};

// ============================================================================
// Level/Experience Events (for RPG systems)
// ============================================================================

struct LevelUpEvent {
    scene::Entity entity;
    int old_level;
    int new_level;
};

struct ExperienceGainedEvent {
    scene::Entity entity;
    float amount;
    float total_experience;
    float experience_to_next_level;
    std::string source;         // "enemy_kill", "quest_complete", etc.
};

// ============================================================================
// Stat System Events
// ============================================================================

// Stats component was initialized
struct StatsInitializedEvent {
    scene::Entity entity;
    std::string preset_name;    // Empty if not from preset
};

// Stats were recalculated
struct StatsRecalculatedEvent {
    scene::Entity entity;
    int stats_changed;          // Number of stats that changed value
};

} // namespace engine::stats
