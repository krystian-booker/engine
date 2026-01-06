#pragma once

#include <engine/stats/stat_modifier.hpp>
#include <engine/core/uuid.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace engine::effects {

// ============================================================================
// Effect Category
// ============================================================================

enum class EffectCategory : uint8_t {
    Buff,           // Positive effect
    Debuff,         // Negative effect
    Neutral,        // Neither positive nor negative
    Passive,        // Always active (e.g., from skill)
    Aura            // Applied to nearby entities
};

// ============================================================================
// Stack Behavior - How multiple applications are handled
// ============================================================================

enum class StackBehavior : uint8_t {
    None,               // Cannot have multiple, new application is rejected
    Refresh,            // Refresh duration to max
    RefreshExtend,      // Add duration up to a max
    Intensity,          // Increase intensity/stacks
    IntensityRefresh,   // Increase stacks AND refresh duration
    Independent         // Each application is tracked separately
};

// ============================================================================
// Effect Flags
// ============================================================================

enum class EffectFlags : uint32_t {
    None            = 0,
    Dispellable     = 1 << 0,       // Can be removed by dispel
    Purgeable       = 1 << 1,       // Can be removed by purge
    Hidden          = 1 << 2,       // Don't show in UI
    Persistent      = 1 << 3,       // Survives death
    Unique          = 1 << 4,       // Only one instance globally
    Stackable       = 1 << 5,       // Can stack (with behavior)
    Refreshable     = 1 << 6,       // Duration can be refreshed
    NoSave          = 1 << 7,       // Don't save to disk
    Inheritable     = 1 << 8,       // Can be passed to summoned entities
};

inline EffectFlags operator|(EffectFlags a, EffectFlags b) {
    return static_cast<EffectFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline EffectFlags operator&(EffectFlags a, EffectFlags b) {
    return static_cast<EffectFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool has_flag(EffectFlags flags, EffectFlags flag) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

// ============================================================================
// Effect Trigger - When effect behaviors activate
// ============================================================================

enum class EffectTrigger : uint8_t {
    OnApply,            // When effect is first applied
    OnRefresh,          // When effect is refreshed
    OnTick,             // On each tick interval
    OnExpire,           // When duration ends naturally
    OnRemove,           // When forcibly removed
    OnStack,            // When stack count increases
    OnMaxStacks,        // When reaching max stacks
    OnDamageDealt,      // When the affected entity deals damage
    OnDamageTaken,      // When the affected entity takes damage
    OnHeal,             // When healed
    OnKill,             // When killing an enemy
    OnDeath,            // When dying
    OnMove,             // While moving
    OnAttack,           // When attacking
    OnCast,             // When casting a spell
};

// ============================================================================
// Effect Definition - Template for creating effect instances
// ============================================================================

struct EffectDefinition {
    std::string effect_id;                  // Unique identifier
    std::string display_name;               // "Poison", "Strength Boost"
    std::string description;                // Full description
    std::string icon_path;                  // UI icon

    EffectCategory category = EffectCategory::Buff;
    EffectFlags flags = EffectFlags::Dispellable | EffectFlags::Stackable;

    // Duration and stacking
    float base_duration = 10.0f;            // 0 = permanent until removed
    float max_duration = 30.0f;             // Max extended duration
    StackBehavior stacking = StackBehavior::RefreshExtend;
    int max_stacks = 1;

    // Tick behavior (for DoT/HoT)
    float tick_interval = 0.0f;             // 0 = no ticking
    bool tick_on_apply = false;             // Tick immediately when applied

    // Stat modifiers (applied while effect is active)
    std::vector<stats::StatModifier> stat_modifiers;

    // Damage over time
    float damage_per_tick = 0.0f;
    std::string damage_type = "physical";   // For resistance calculation

    // Healing over time
    float heal_per_tick = 0.0f;

    // Resource drain/restore per tick
    std::vector<std::pair<stats::StatType, float>> resource_per_tick;

    // Effects that this grants immunity to
    std::vector<std::string> grants_immunity;

    // Effects that this removes when applied
    std::vector<std::string> removes_effects;

    // Effects that prevent this from being applied
    std::vector<std::string> blocked_by;

    // Tags for categorization and filtering
    std::vector<std::string> tags;          // "poison", "fire", "crowd_control"

    // Visual/Audio
    std::string apply_vfx;                  // VFX on application
    std::string tick_vfx;                   // VFX each tick
    std::string expire_vfx;                 // VFX on expiration
    std::string loop_vfx;                   // Continuous VFX while active
    std::string apply_sfx;
    std::string tick_sfx;
    std::string loop_sfx;

    // Priority for dispel ordering (higher = harder to remove)
    int dispel_priority = 0;

    // For scaling effects
    float intensity_per_stack = 1.0f;       // Multiplier per stack
    bool scale_duration_with_stacks = false;

    // ========================================================================
    // Helper methods
    // ========================================================================

    bool is_buff() const { return category == EffectCategory::Buff; }
    bool is_debuff() const { return category == EffectCategory::Debuff; }
    bool is_dispellable() const { return has_flag(flags, EffectFlags::Dispellable); }
    bool is_hidden() const { return has_flag(flags, EffectFlags::Hidden); }
    bool has_ticking() const { return tick_interval > 0.0f; }
    bool is_permanent() const { return base_duration <= 0.0f; }
    bool can_stack() const { return max_stacks > 1; }
    bool has_tag(const std::string& tag) const;
};

// ============================================================================
// Effect Definition Registry
// ============================================================================

class EffectRegistry {
public:
    static EffectRegistry& instance();

    // Delete copy/move
    EffectRegistry(const EffectRegistry&) = delete;
    EffectRegistry& operator=(const EffectRegistry&) = delete;

    // Registration
    void register_effect(const EffectDefinition& def);
    void load_effects(const std::string& path);

    // Lookup
    const EffectDefinition* get(const std::string& effect_id) const;
    bool exists(const std::string& effect_id) const;

    // Queries
    std::vector<std::string> get_all_effect_ids() const;
    std::vector<std::string> get_effects_by_category(EffectCategory category) const;
    std::vector<std::string> get_effects_by_tag(const std::string& tag) const;

    // Clear all (for hot reload)
    void clear();

private:
    EffectRegistry() = default;
    ~EffectRegistry() = default;

    std::unordered_map<std::string, EffectDefinition> m_effects;
};

// ============================================================================
// Global Access
// ============================================================================

inline EffectRegistry& effect_registry() { return EffectRegistry::instance(); }

// ============================================================================
// Effect Builder - Fluent API for creating definitions
// ============================================================================

class EffectBuilder {
public:
    EffectBuilder& id(const std::string& effect_id);
    EffectBuilder& name(const std::string& display_name);
    EffectBuilder& description(const std::string& desc);
    EffectBuilder& icon(const std::string& path);
    EffectBuilder& category(EffectCategory cat);
    EffectBuilder& buff();
    EffectBuilder& debuff();
    EffectBuilder& duration(float seconds);
    EffectBuilder& permanent();
    EffectBuilder& stacking(StackBehavior behavior, int max_stacks = 1);
    EffectBuilder& tick(float interval);
    EffectBuilder& damage_per_tick(float amount, const std::string& type = "physical");
    EffectBuilder& heal_per_tick(float amount);
    EffectBuilder& stat_modifier(stats::StatType stat, stats::ModifierType type, float value);
    EffectBuilder& grants_immunity(const std::string& effect_id);
    EffectBuilder& removes(const std::string& effect_id);
    EffectBuilder& blocked_by(const std::string& effect_id);
    EffectBuilder& tag(const std::string& t);
    EffectBuilder& dispellable(bool value = true);
    EffectBuilder& hidden(bool value = true);
    EffectBuilder& vfx(const std::string& apply, const std::string& loop = "");
    EffectBuilder& sfx(const std::string& apply, const std::string& loop = "");

    EffectDefinition build() const;
    void register_effect() const;

private:
    EffectDefinition m_def;
};

// ============================================================================
// Convenience
// ============================================================================

inline EffectBuilder effect() { return EffectBuilder{}; }

} // namespace engine::effects
