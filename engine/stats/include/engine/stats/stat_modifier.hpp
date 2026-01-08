#pragma once

#include <engine/stats/stat_definition.hpp>
#include <engine/core/uuid.hpp>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace engine::stats {

// ============================================================================
// ModifierType - How the modifier affects the stat
// ============================================================================

enum class ModifierType : uint8_t {
    Flat,           // +10 (applied first)
    PercentAdd,     // +10% (additive with other PercentAdd)
    PercentMult,    // *1.10 (multiplicative, applied last)
    Override        // Set to exact value (ignores other modifiers)
};

// ============================================================================
// ModifierSource - What caused this modifier
// ============================================================================

enum class ModifierSource : uint8_t {
    Base,           // Innate/Base value
    Equipment,      // From equipped item
    Effect,         // From status effect/buff/debuff
    Skill,          // From passive skill/ability
    Aura,           // From nearby entity
    Environment,    // From world zone/weather
    Temporary,      // Short-term boost
    Permanent,      // Permanent upgrade
    Debug,          // Debug/cheat modifier
    Custom
};

// ============================================================================
// StatModifier - A single modification to a stat
// ============================================================================

struct StatModifier {
    core::UUID id;                      // Unique ID for this modifier instance
    StatType stat = StatType::Health;
    ModifierType type = ModifierType::Flat;
    ModifierSource source = ModifierSource::Temporary;

    float value = 0.0f;
    int priority = 0;                   // Order within same type (higher = later)

    std::string source_id;              // "equipment:iron_sword", "effect:poison"
    std::string source_name;            // Human-readable source name

    float duration = -1.0f;             // -1 = permanent, >0 = seconds remaining
    float elapsed = 0.0f;               // Time since applied

    bool is_hidden = false;             // Don't show in UI
    bool is_stackable = true;           // Can have multiple from same source

    // Optional condition for when modifier applies
    // Returns true if modifier should be active
    std::function<bool()> condition;

    // Constructor helpers
    static StatModifier flat(StatType stat, float value, const std::string& source = "");
    static StatModifier percent_add(StatType stat, float value, const std::string& source = "");
    static StatModifier percent_mult(StatType stat, float value, const std::string& source = "");
    static StatModifier override_val(StatType stat, float value, const std::string& source = "");

    // Check if modifier has expired
    bool is_expired() const { return duration > 0.0f && elapsed >= duration; }

    // Check if modifier is permanent
    bool is_permanent() const { return duration < 0.0f; }

    // Check if condition is met (or no condition)
    bool is_active() const { return !condition || condition(); }

    // Get remaining duration (-1 if permanent)
    float get_remaining() const {
        if (duration < 0.0f) return -1.0f;
        return std::max(0.0f, duration - elapsed);
    }

    // Update elapsed time, returns true if still active
    bool update(float dt) {
        if (duration > 0.0f) {
            elapsed += dt;
            return elapsed < duration;
        }
        return true;
    }
};

// ============================================================================
// ModifierStack - Collection of modifiers for calculation
// ============================================================================

struct ModifierStack {
    std::vector<StatModifier> flat;
    std::vector<StatModifier> percent_add;
    std::vector<StatModifier> percent_mult;
    StatModifier* override_modifier = nullptr;

    void add(const StatModifier& mod);
    void clear();
    bool empty() const;
    size_t total_count() const;
};

// ============================================================================
// Modifier Calculation
// ============================================================================

// Calculate final stat value from base and modifiers
// Formula: (base + sum(flat)) * (1 + sum(percent_add)) * product(1 + percent_mult)
// If override exists, returns override value
float calculate_stat_value(float base_value, const ModifierStack& modifiers);

// Calculate just the modifier delta (without base)
float calculate_modifier_delta(float base_value, const ModifierStack& modifiers);

// ============================================================================
// ModifierBuilder - Fluent API for creating modifiers
// ============================================================================

class ModifierBuilder {
public:
    ModifierBuilder& stat(StatType type);
    ModifierBuilder& flat(float value);
    ModifierBuilder& percent_add(float value);
    ModifierBuilder& percent_mult(float value);
    ModifierBuilder& override_value(float value);
    ModifierBuilder& source(ModifierSource src, const std::string& id);
    ModifierBuilder& duration(float seconds);
    ModifierBuilder& permanent();
    ModifierBuilder& priority(int p);
    ModifierBuilder& hidden();
    ModifierBuilder& condition(std::function<bool()> cond);

    StatModifier build() const;

private:
    StatModifier m_modifier;
};

// ============================================================================
// Convenience Functions
// ============================================================================

// Create a builder
inline ModifierBuilder modifier() { return ModifierBuilder{}; }

// Quick creation helpers
StatModifier make_flat_modifier(StatType stat, float value, const std::string& source = "");
StatModifier make_percent_modifier(StatType stat, float percent, const std::string& source = "");
StatModifier make_timed_modifier(StatType stat, float value, float duration, const std::string& source = "");

} // namespace engine::stats
