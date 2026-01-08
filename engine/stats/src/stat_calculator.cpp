#include <engine/stats/stat_calculator.hpp>
#include <cmath>
#include <random>
#include <algorithm>

namespace engine::stats {

// ============================================================================
// StatCalculator Implementation
// ============================================================================

float StatCalculator::calculate(float base_value, const std::vector<StatModifier>& modifiers) {
    ModifierStack stack;
    for (const auto& mod : modifiers) {
        stack.add(mod);
    }
    return calculate(base_value, stack);
}

float StatCalculator::calculate(float base_value, const ModifierStack& stack) {
    return calculate_stat_value(base_value, stack);
}

float StatCalculator::calculate_derived(const StatsComponent& stats, StatType derived_stat) {
    const StatDefinition* def = stat_registry().get_definition(derived_stat);
    if (!def || def->derived_from == StatType::Count) {
        return stats.get(derived_stat);
    }

    float source_value = stats.get(def->derived_from);
    return source_value * def->derived_multiplier + def->derived_flat;
}

StatCalculator::ModifierBreakdown StatCalculator::get_breakdown(const StatsComponent& stats, StatType stat) {
    ModifierBreakdown breakdown;
    breakdown.base_value = stats.get_base(stat);
    breakdown.flat_total = 0.0f;
    breakdown.percent_add_total = 0.0f;
    breakdown.percent_mult_total = 1.0f;
    breakdown.has_override = false;
    breakdown.override_value = 0.0f;

    float percent_add_normalized = 0.0f;

    const auto& mods = stats.get_modifiers(stat);
    for (const auto& mod : mods) {
        if (!mod.is_active() || mod.is_expired()) continue;

        std::string source_name = mod.source_name.empty() ? mod.source_id : mod.source_name;

        switch (mod.type) {
            case ModifierType::Flat:
                breakdown.flat_total += mod.value;
                breakdown.sources.emplace_back(source_name, mod.value);
                break;
            case ModifierType::PercentAdd:
                breakdown.percent_add_total += mod.value;
                percent_add_normalized += (mod.value > 1.0f ? mod.value * 0.01f : mod.value);
                breakdown.sources.emplace_back(source_name + " (%)", mod.value);
                break;
            case ModifierType::PercentMult:
                {
                    float value = (mod.value > 1.0f ? mod.value * 0.01f : mod.value);
                    breakdown.percent_mult_total *= (1.0f + value);
                    breakdown.sources.emplace_back(source_name + " (x)", mod.value);
                }
                break;
            case ModifierType::Override:
                breakdown.has_override = true;
                breakdown.override_value = mod.value;
                breakdown.sources.emplace_back(source_name + " [OVERRIDE]", mod.value);
                break;
        }
    }

    if (breakdown.has_override) {
        breakdown.final_value = breakdown.override_value;
    } else {
        float result = breakdown.base_value + breakdown.flat_total;
        result *= (1.0f + percent_add_normalized);
        result *= breakdown.percent_mult_total;
        breakdown.final_value = result;
    }

    return breakdown;
}

float StatCalculator::get_modifier_contribution(const StatsComponent& stats, StatType stat) {
    return stats.get(stat) - stats.get_base(stat);
}

float StatCalculator::get_modifier_percent_change(const StatsComponent& stats, StatType stat) {
    float base = stats.get_base(stat);
    if (base <= 0.0f) return 0.0f;
    return (stats.get(stat) - base) / base * 100.0f;
}

// ============================================================================
// StatQuery Implementation
// ============================================================================

float StatQuery::get(scene::World& world, scene::Entity entity, StatType stat) {
    if (!world.valid(entity)) return 0.0f;

    auto* stats = world.try_get<StatsComponent>(entity);
    return stats ? stats->get(stat) : 0.0f;
}

float StatQuery::get_current(scene::World& world, scene::Entity entity, StatType resource) {
    if (!world.valid(entity)) return 0.0f;

    auto* stats = world.try_get<StatsComponent>(entity);
    return stats ? stats->get_current(resource) : 0.0f;
}

float StatQuery::get_percent(scene::World& world, scene::Entity entity, StatType resource) {
    if (!world.valid(entity)) return 0.0f;

    auto* stats = world.try_get<StatsComponent>(entity);
    return stats ? stats->get_percent(resource) : 0.0f;
}

bool StatQuery::has(scene::World& world, scene::Entity entity, StatType stat) {
    if (!world.valid(entity)) return false;

    auto* stats = world.try_get<StatsComponent>(entity);
    return stats && stats->has(stat);
}

float StatQuery::compare(scene::World& world, scene::Entity a, scene::Entity b, StatType stat) {
    return get(world, a, stat) - get(world, b, stat);
}

scene::Entity StatQuery::find_highest(scene::World& world, StatType stat) {
    scene::Entity highest = scene::NullEntity;
    float highest_value = std::numeric_limits<float>::lowest();

    auto view = world.view<StatsComponent>();
    for (auto entity : view) {
        float value = view.get<StatsComponent>(entity).get(stat);
        if (value > highest_value) {
            highest_value = value;
            highest = entity;
        }
    }

    return highest;
}

scene::Entity StatQuery::find_lowest(scene::World& world, StatType stat) {
    scene::Entity lowest = scene::NullEntity;
    float lowest_value = std::numeric_limits<float>::max();

    auto view = world.view<StatsComponent>();
    for (auto entity : view) {
        float value = view.get<StatsComponent>(entity).get(stat);
        if (value < lowest_value) {
            lowest_value = value;
            lowest = entity;
        }
    }

    return lowest;
}

std::vector<scene::Entity> StatQuery::filter_by_stat(scene::World& world, StatType stat, StatFilter filter) {
    std::vector<scene::Entity> result;

    auto view = world.view<StatsComponent>();
    for (auto entity : view) {
        float value = view.get<StatsComponent>(entity).get(stat);
        if (filter(value)) {
            result.push_back(entity);
        }
    }

    return result;
}

// ============================================================================
// StatOperations Implementation
// ============================================================================

void StatOperations::set_base(scene::World& world, scene::Entity entity, StatType stat, float value) {
    if (!world.valid(entity)) return;

    auto* stats = world.try_get<StatsComponent>(entity);
    if (stats) {
        stats->set_base(stat, value);
    }
}

void StatOperations::add_base(scene::World& world, scene::Entity entity, StatType stat, float delta) {
    if (!world.valid(entity)) return;

    auto* stats = world.try_get<StatsComponent>(entity);
    if (stats) {
        stats->add_base(stat, delta);
    }
}

void StatOperations::add_modifier(scene::World& world, scene::Entity entity, const StatModifier& mod) {
    if (!world.valid(entity)) return;

    auto* stats = world.try_get<StatsComponent>(entity);
    if (stats) {
        stats->add_modifier(mod);
    }
}

int StatOperations::remove_modifiers(scene::World& world, scene::Entity entity, const std::string& source_id) {
    if (!world.valid(entity)) return 0;

    auto* stats = world.try_get<StatsComponent>(entity);
    return stats ? stats->remove_modifiers_by_source(source_id) : 0;
}

float StatOperations::modify_resource(scene::World& world, scene::Entity entity, StatType resource, float delta) {
    if (!world.valid(entity)) return 0.0f;

    auto* stats = world.try_get<StatsComponent>(entity);
    return stats ? stats->modify_current(resource, delta) : 0.0f;
}

float StatOperations::damage(scene::World& world, scene::Entity entity, float amount) {
    return modify_resource(world, entity, StatType::Health, -std::abs(amount));
}

float StatOperations::heal(scene::World& world, scene::Entity entity, float amount) {
    return modify_resource(world, entity, StatType::Health, std::abs(amount));
}

float StatOperations::consume_stamina(scene::World& world, scene::Entity entity, float amount) {
    return modify_resource(world, entity, StatType::Stamina, -std::abs(amount));
}

float StatOperations::consume_mana(scene::World& world, scene::Entity entity, float amount) {
    return modify_resource(world, entity, StatType::Mana, -std::abs(amount));
}

bool StatOperations::can_afford(scene::World& world, scene::Entity entity, StatType resource, float cost) {
    return StatQuery::get_current(world, entity, resource) >= cost;
}

float StatOperations::transfer_resource(scene::World& world, scene::Entity from, scene::Entity to,
                                         StatType resource, float amount) {
    // Take from source
    float taken = -modify_resource(world, from, resource, -std::abs(amount));

    // Give to target
    float given = modify_resource(world, to, resource, taken);

    return given;
}

// ============================================================================
// StatScaling Implementation
// ============================================================================

float StatScaling::scale(float base, int level, float growth, ScaleType type) {
    switch (type) {
        case ScaleType::Linear:
            return base + (static_cast<float>(level) * growth);

        case ScaleType::Exponential:
            return base * std::pow(growth, static_cast<float>(level));

        case ScaleType::Logarithmic:
            return base + std::log(static_cast<float>(level + 1)) * growth;

        case ScaleType::Diminishing:
            return base + growth * (1.0f - std::exp(-static_cast<float>(level) / 10.0f));

        default:
            return base;
    }
}

float StatScaling::calculate_from_attributes(const StatsComponent& stats,
                                              const std::vector<AttributeScaling>& scalings) {
    float result = 0.0f;
    for (const auto& scaling : scalings) {
        result += stats.get(scaling.attribute) * scaling.multiplier + scaling.flat_bonus;
    }
    return result;
}

float StatScaling::calculate_damage(const StatsComponent& attacker, bool is_magic) {
    if (is_magic) {
        float base = attacker.get(StatType::MagicDamage);
        float intel = attacker.get(StatType::Intelligence);
        return base + intel * 0.5f;
    } else {
        float base = attacker.get(StatType::PhysicalDamage);
        float str = attacker.get(StatType::Strength);
        return base + str * 0.5f;
    }
}

float StatScaling::calculate_defense(const StatsComponent& defender, bool is_magic) {
    if (is_magic) {
        return defender.get(StatType::MagicDefense);
    } else {
        return defender.get(StatType::PhysicalDefense);
    }
}

float StatScaling::calculate_damage_reduction(float defense) {
    // Diminishing returns formula
    // At 100 defense = 50% reduction
    // At 200 defense = 67% reduction
    // At 500 defense = 83% reduction
    return defense / (defense + 100.0f);
}

bool StatScaling::roll_crit(const StatsComponent& stats) {
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(0.0f, 100.0f);

    float crit_chance = stats.get(StatType::CritChance);
    return dist(rng) < crit_chance;
}

float StatScaling::apply_crit(float damage, const StatsComponent& stats) {
    float crit_multiplier = stats.get(StatType::CritDamage) / 100.0f;  // CritDamage is stored as 150 for 1.5x
    return damage * crit_multiplier;
}

// ============================================================================
// Regeneration
// ============================================================================

float calculate_regen(const StatsComponent& stats, const RegenRate& rate, float dt) {
    float regen_per_second = rate.base_rate;

    if (rate.regen_stat != StatType::Count) {
        regen_per_second += stats.get(rate.regen_stat);
    }

    return regen_per_second * dt;
}

} // namespace engine::stats
