#include <engine/stats/stat_modifier.hpp>
#include <algorithm>
#include <numeric>

namespace engine::stats {

// ============================================================================
// StatModifier Implementation
// ============================================================================

StatModifier StatModifier::flat(StatType stat, float value, const std::string& source) {
    StatModifier mod;
    mod.id = core::UUID::generate();
    mod.stat = stat;
    mod.type = ModifierType::Flat;
    mod.value = value;
    mod.source_id = source;
    return mod;
}

StatModifier StatModifier::percent_add(StatType stat, float value, const std::string& source) {
    StatModifier mod;
    mod.id = core::UUID::generate();
    mod.stat = stat;
    mod.type = ModifierType::PercentAdd;
    mod.value = value;
    mod.source_id = source;
    return mod;
}

StatModifier StatModifier::percent_mult(StatType stat, float value, const std::string& source) {
    StatModifier mod;
    mod.id = core::UUID::generate();
    mod.stat = stat;
    mod.type = ModifierType::PercentMult;
    mod.value = value;
    mod.source_id = source;
    return mod;
}

StatModifier StatModifier::override_val(StatType stat, float value, const std::string& source) {
    StatModifier mod;
    mod.id = core::UUID::generate();
    mod.stat = stat;
    mod.type = ModifierType::Override;
    mod.value = value;
    mod.source_id = source;
    return mod;
}

// ============================================================================
// ModifierStack Implementation
// ============================================================================

void ModifierStack::add(const StatModifier& mod) {
    // Only consider active modifiers
    if (!mod.is_active() || mod.is_expired()) return;

    switch (mod.type) {
        case ModifierType::Flat:
            flat.push_back(mod);
            break;
        case ModifierType::PercentAdd:
            percent_add.push_back(mod);
            break;
        case ModifierType::PercentMult:
            percent_mult.push_back(mod);
            break;
        case ModifierType::Override:
            // Only keep highest priority override
            if (!override_modifier || mod.priority > override_modifier->priority) {
                override_modifier = const_cast<StatModifier*>(&mod);
            }
            break;
    }
}

void ModifierStack::clear() {
    flat.clear();
    percent_add.clear();
    percent_mult.clear();
    override_modifier = nullptr;
}

bool ModifierStack::empty() const {
    return flat.empty() && percent_add.empty() && percent_mult.empty() && !override_modifier;
}

size_t ModifierStack::total_count() const {
    return flat.size() + percent_add.size() + percent_mult.size() + (override_modifier ? 1 : 0);
}

// ============================================================================
// Modifier Calculation
// ============================================================================

float calculate_stat_value(float base_value, const ModifierStack& modifiers) {
    // If override exists, return override value
    if (modifiers.override_modifier) {
        return modifiers.override_modifier->value;
    }

    // Start with base
    float result = base_value;

    // Add all flat modifiers
    for (const auto& mod : modifiers.flat) {
        result += mod.value;
    }

    // Sum all additive percent modifiers, then apply once
    float percent_add_total = 0.0f;
    for (const auto& mod : modifiers.percent_add) {
        percent_add_total += mod.value;
    }
    result *= (1.0f + percent_add_total / 100.0f);

    // Apply multiplicative percent modifiers
    for (const auto& mod : modifiers.percent_mult) {
        result *= (1.0f + mod.value / 100.0f);
    }

    return result;
}

float calculate_modifier_delta(float base_value, const ModifierStack& modifiers) {
    return calculate_stat_value(base_value, modifiers) - base_value;
}

// ============================================================================
// ModifierBuilder Implementation
// ============================================================================

ModifierBuilder& ModifierBuilder::stat(StatType type) {
    m_modifier.stat = type;
    return *this;
}

ModifierBuilder& ModifierBuilder::flat(float value) {
    m_modifier.type = ModifierType::Flat;
    m_modifier.value = value;
    return *this;
}

ModifierBuilder& ModifierBuilder::percent_add(float value) {
    m_modifier.type = ModifierType::PercentAdd;
    m_modifier.value = value;
    return *this;
}

ModifierBuilder& ModifierBuilder::percent_mult(float value) {
    m_modifier.type = ModifierType::PercentMult;
    m_modifier.value = value;
    return *this;
}

ModifierBuilder& ModifierBuilder::override_value(float value) {
    m_modifier.type = ModifierType::Override;
    m_modifier.value = value;
    return *this;
}

ModifierBuilder& ModifierBuilder::source(ModifierSource src, const std::string& id) {
    m_modifier.source = src;
    m_modifier.source_id = id;
    return *this;
}

ModifierBuilder& ModifierBuilder::duration(float seconds) {
    m_modifier.duration = seconds;
    return *this;
}

ModifierBuilder& ModifierBuilder::permanent() {
    m_modifier.duration = -1.0f;
    return *this;
}

ModifierBuilder& ModifierBuilder::priority(int p) {
    m_modifier.priority = p;
    return *this;
}

ModifierBuilder& ModifierBuilder::hidden() {
    m_modifier.is_hidden = true;
    return *this;
}

ModifierBuilder& ModifierBuilder::condition(std::function<bool()> cond) {
    m_modifier.condition = std::move(cond);
    return *this;
}

StatModifier ModifierBuilder::build() const {
    StatModifier result = m_modifier;
    result.id = core::UUID::generate();
    return result;
}

// ============================================================================
// Convenience Functions
// ============================================================================

StatModifier make_flat_modifier(StatType stat, float value, const std::string& source) {
    return StatModifier::flat(stat, value, source);
}

StatModifier make_percent_modifier(StatType stat, float percent, const std::string& source) {
    return StatModifier::percent_add(stat, percent, source);
}

StatModifier make_timed_modifier(StatType stat, float value, float duration, const std::string& source) {
    StatModifier mod = StatModifier::flat(stat, value, source);
    mod.duration = duration;
    return mod;
}

} // namespace engine::stats
