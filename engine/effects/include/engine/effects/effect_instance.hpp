#pragma once

#include <engine/effects/effect_definition.hpp>
#include <engine/scene/entity.hpp>
#include <engine/core/uuid.hpp>
#include <string>
#include <cstdint>

namespace engine::effects {

// ============================================================================
// Effect State
// ============================================================================

enum class EffectState : uint8_t {
    Pending,        // Not yet applied
    Active,         // Currently active
    Paused,         // Temporarily paused (duration doesn't advance)
    Expiring,       // About to expire (for visual fadeout)
    Expired,        // Duration ended
    Removed,        // Forcibly removed
    Blocked         // Application was blocked
};

// ============================================================================
// EffectInstance - Runtime instance of an effect
// ============================================================================

struct EffectInstance {
    core::UUID instance_id;                 // Unique instance ID
    std::string definition_id;              // Reference to EffectDefinition

    scene::Entity target = scene::NullEntity;   // Entity this is applied to
    scene::Entity source = scene::NullEntity;   // Entity that applied it

    EffectState state = EffectState::Pending;

    // Timing
    float duration = 0.0f;                  // Current max duration
    float remaining = 0.0f;                 // Time remaining
    float elapsed = 0.0f;                   // Total time active
    float tick_timer = 0.0f;                // Time until next tick

    // Stacking
    int stacks = 1;
    float intensity = 1.0f;                 // Effective multiplier (may scale with stacks)

    // Per-instance overrides
    float damage_multiplier = 1.0f;
    float heal_multiplier = 1.0f;
    float duration_multiplier = 1.0f;

    // Modifier tracking (IDs of applied stat modifiers)
    std::vector<core::UUID> applied_modifier_ids;

    // Application timestamp (for ordering)
    uint64_t apply_timestamp = 0;

    // Custom data for game logic
    std::unordered_map<std::string, float> custom_float_data;
    std::unordered_map<std::string, std::string> custom_string_data;

    // ========================================================================
    // State queries
    // ========================================================================

    bool is_active() const { return state == EffectState::Active; }
    bool is_expired() const { return state == EffectState::Expired || state == EffectState::Removed; }
    bool is_permanent() const { return duration <= 0.0f; }
    bool should_tick() const;
    bool can_refresh() const;
    bool can_add_stack() const;

    // ========================================================================
    // Time queries
    // ========================================================================

    float get_remaining_percent() const;
    float get_elapsed_percent() const;

    // ========================================================================
    // Stack helpers
    // ========================================================================

    void add_stack(int count = 1);
    void remove_stack(int count = 1);
    void set_stacks(int count);
    void refresh_duration();
    void extend_duration(float amount);

    // ========================================================================
    // Update
    // ========================================================================

    // Returns true if still active
    bool update(float dt);

    // Check if ready to tick (and reset timer)
    bool consume_tick();

    // ========================================================================
    // Definition access
    // ========================================================================

    const EffectDefinition* get_definition() const;

    // ========================================================================
    // Factory
    // ========================================================================

    static EffectInstance create(const std::string& definition_id,
                                 scene::Entity target,
                                 scene::Entity source = scene::NullEntity);
};

// ============================================================================
// Effect Application Result
// ============================================================================

enum class ApplyResult : uint8_t {
    Applied,            // New effect applied
    Refreshed,          // Duration refreshed
    Extended,           // Duration extended
    Stacked,            // Stack added
    StackedAndRefreshed,
    AlreadyAtMax,       // At max stacks, couldn't apply
    Blocked,            // Blocked by immunity or other effect
    TargetInvalid,      // Target entity invalid
    DefinitionNotFound, // Effect definition doesn't exist
    Failed              // Generic failure
};

struct ApplyResultInfo {
    ApplyResult result;
    EffectInstance* instance = nullptr;     // Pointer to the instance (if applied)
    int new_stack_count = 0;
    float new_duration = 0.0f;
    std::string blocked_by;                 // What blocked it (if blocked)
};

// ============================================================================
// Removal Reason
// ============================================================================

enum class RemovalReason : uint8_t {
    Expired,            // Duration ended naturally
    Dispelled,          // Removed by dispel ability
    Purged,             // Removed by purge
    Replaced,           // Replaced by another effect
    Cancelled,          // Manually cancelled
    Death,              // Owner died
    SourceDeath,        // Source entity died
    StacksDepleted,     // All stacks removed
    GameLogic           // Game-specific removal
};

} // namespace engine::effects
