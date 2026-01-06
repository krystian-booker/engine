#pragma once

#include <engine/stats/stat_definition.hpp>
#include <engine/stats/stat_modifier.hpp>
#include <engine/stats/stat_component.hpp>
#include <engine/scene/entity.hpp>
#include <engine/scene/world.hpp>
#include <functional>
#include <vector>

namespace engine::stats {

// ============================================================================
// StatCalculator - Utility for stat calculations
// ============================================================================

class StatCalculator {
public:
    // Calculate final stat value from base and modifiers
    static float calculate(float base_value, const std::vector<StatModifier>& modifiers);

    // Calculate with modifier stack
    static float calculate(float base_value, const ModifierStack& stack);

    // Calculate derived stat (e.g., PhysicalDamage from Strength)
    static float calculate_derived(const StatsComponent& stats, StatType derived_stat);

    // Get modifier breakdown for UI
    struct ModifierBreakdown {
        float base_value;
        float flat_total;
        float percent_add_total;
        float percent_mult_total;
        float final_value;
        bool has_override;
        float override_value;
        std::vector<std::pair<std::string, float>> sources;  // source_name -> contribution
    };

    static ModifierBreakdown get_breakdown(const StatsComponent& stats, StatType stat);

    // Calculate total modifier contribution (final - base)
    static float get_modifier_contribution(const StatsComponent& stats, StatType stat);

    // Calculate percentage change from base
    static float get_modifier_percent_change(const StatsComponent& stats, StatType stat);
};

// ============================================================================
// StatQuery - Query and compare stats across entities
// ============================================================================

class StatQuery {
public:
    // Get stat from entity (returns 0 if no StatsComponent)
    static float get(scene::World& world, scene::Entity entity, StatType stat);

    // Get current resource value
    static float get_current(scene::World& world, scene::Entity entity, StatType resource);

    // Get resource percentage
    static float get_percent(scene::World& world, scene::Entity entity, StatType resource);

    // Check if entity has stat
    static bool has(scene::World& world, scene::Entity entity, StatType stat);

    // Compare stats between entities
    static float compare(scene::World& world,
                        scene::Entity a, scene::Entity b,
                        StatType stat);  // Returns a - b

    // Find entity with highest stat in view
    static scene::Entity find_highest(scene::World& world, StatType stat);

    // Find entity with lowest stat in view
    static scene::Entity find_lowest(scene::World& world, StatType stat);

    // Filter entities by stat threshold
    using StatFilter = std::function<bool(float)>;
    static std::vector<scene::Entity> filter_by_stat(scene::World& world,
                                                      StatType stat,
                                                      StatFilter filter);
};

// ============================================================================
// StatOperations - Modify stats on entities
// ============================================================================

class StatOperations {
public:
    // Set base stat
    static void set_base(scene::World& world, scene::Entity entity,
                        StatType stat, float value);

    // Add to base stat
    static void add_base(scene::World& world, scene::Entity entity,
                        StatType stat, float delta);

    // Add modifier to entity
    static void add_modifier(scene::World& world, scene::Entity entity,
                            const StatModifier& mod);

    // Remove modifiers by source
    static int remove_modifiers(scene::World& world, scene::Entity entity,
                               const std::string& source_id);

    // Modify resource (damage/heal)
    static float modify_resource(scene::World& world, scene::Entity entity,
                                StatType resource, float delta);

    // Damage entity (shorthand for modify_resource with Health)
    static float damage(scene::World& world, scene::Entity entity, float amount);

    // Heal entity
    static float heal(scene::World& world, scene::Entity entity, float amount);

    // Consume stamina
    static float consume_stamina(scene::World& world, scene::Entity entity, float amount);

    // Consume mana
    static float consume_mana(scene::World& world, scene::Entity entity, float amount);

    // Check if can afford resource cost
    static bool can_afford(scene::World& world, scene::Entity entity,
                          StatType resource, float cost);

    // Transfer resource between entities
    static float transfer_resource(scene::World& world,
                                   scene::Entity from, scene::Entity to,
                                   StatType resource, float amount);
};

// ============================================================================
// StatScaling - Level and attribute scaling calculations
// ============================================================================

class StatScaling {
public:
    // Scale type for growth curves
    enum class ScaleType {
        Linear,     // base + (level * growth)
        Exponential, // base * pow(growth, level)
        Logarithmic, // base + log(level) * growth
        Diminishing  // base + growth * (1 - exp(-level/rate))
    };

    // Calculate scaled value
    static float scale(float base, int level, float growth, ScaleType type = ScaleType::Linear);

    // Calculate stat from attribute (e.g., PhysicalDamage from Strength)
    struct AttributeScaling {
        StatType attribute;
        float multiplier;
        float flat_bonus;
    };

    static float calculate_from_attributes(const StatsComponent& stats,
                                          const std::vector<AttributeScaling>& scalings);

    // Common scaling formulas
    static float calculate_damage(const StatsComponent& attacker, bool is_magic = false);
    static float calculate_defense(const StatsComponent& defender, bool is_magic = false);
    static float calculate_damage_reduction(float defense);  // Returns 0.0-1.0

    // Critical hit calculations
    static bool roll_crit(const StatsComponent& stats);
    static float apply_crit(float damage, const StatsComponent& stats);
};

// ============================================================================
// Regeneration Rates
// ============================================================================

struct RegenRate {
    StatType resource;          // Which resource to regen
    StatType regen_stat;        // Stat that determines rate (e.g., HealthRegen)
    float base_rate = 0.0f;     // Base per-second rate if no regen stat
    float delay_after_damage = 0.0f;  // Delay before regen starts
};

// Calculate regen amount for a time period
float calculate_regen(const StatsComponent& stats, const RegenRate& rate, float dt);

} // namespace engine::stats
