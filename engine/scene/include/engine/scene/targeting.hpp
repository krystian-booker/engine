#pragma once

#include <engine/scene/entity.hpp>
#include <engine/scene/world.hpp>
#include <engine/core/math.hpp>
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <cstdint>

namespace engine::scene {

// ============================================================================
// Components
// ============================================================================

// Makes an entity targetable by players and AI
struct TargetableComponent {
    bool enabled = true;                        // Can be targeted when enabled

    // Target point configuration
    Vec3 target_point_offset{0.0f, 1.0f, 0.0f}; // Local offset for aim point (e.g., chest height)
    float target_size = 1.0f;                   // Affects selection radius/priority

    // Priority and filtering
    float target_priority = 1.0f;               // Higher = preferred when multiple in range
    std::string faction = "enemy";              // For friend/foe filtering

    // Valid targeting distance
    float min_target_distance = 1.0f;           // Minimum distance to target
    float max_target_distance = 30.0f;          // Maximum lock-on range

    // Visibility requirements
    bool requires_line_of_sight = true;         // Must be visible to target
    bool target_through_walls = false;          // Override LOS for special targets

    // State (managed by system)
    bool is_targeted = false;                   // Currently being targeted
    Entity targeted_by = NullEntity;            // Who is targeting this entity

    // UI indicator settings
    bool show_indicator_when_available = true;  // Show indicator when in range
    bool show_indicator_when_targeted = true;   // Show indicator when locked on
};

// Component for entities that can target others (player, AI)
struct TargeterComponent {
    Entity current_target = NullEntity;         // Currently locked target
    bool lock_on_active = false;                // Is lock-on mode active

    // Lock-on configuration
    float max_lock_distance = 20.0f;            // Maximum lock-on range
    float lock_angle = 60.0f;                   // Cone angle for target acquisition (degrees)
    float lock_on_height_tolerance = 5.0f;      // Vertical tolerance for targeting

    // Faction filtering
    std::vector<std::string> target_factions = {"enemy"};

    // Target switching
    float switch_cooldown = 0.2f;               // Minimum time between target switches
    float time_since_switch = 0.0f;
    bool allow_switch_while_locked = true;      // Can switch targets while locked on

    // Soft lock (aim assist without full lock-on)
    bool soft_lock_enabled = true;
    float soft_lock_range = 10.0f;              // Soft lock detection range
    float soft_lock_angle = 30.0f;              // Narrower cone for soft lock
    float soft_lock_strength = 0.5f;            // 0-1, how much to assist aim
    Entity soft_lock_target = NullEntity;       // Current soft lock target

    // Lock-on persistence
    float lock_break_distance = 25.0f;          // Distance at which lock breaks
    float lock_break_time = 3.0f;               // Time out of sight before lock breaks
    float time_target_not_visible = 0.0f;       // Tracking visibility loss
    bool break_lock_on_death = true;            // Break lock when target dies

    // Helpers
    bool has_target() const { return current_target != NullEntity; }
    bool has_soft_lock() const { return soft_lock_target != NullEntity; }
};

// UI indicator component for targets
struct TargetIndicatorComponent {
    bool show_indicator = true;
    bool show_health_bar = true;
    bool show_name = false;
    bool show_distance = false;

    // Colors for different states
    Vec4 locked_color{1.0f, 0.2f, 0.2f, 1.0f};      // Hard lock-on color
    Vec4 soft_lock_color{1.0f, 1.0f, 0.3f, 0.7f};   // Soft lock/aim assist color
    Vec4 available_color{1.0f, 1.0f, 1.0f, 0.5f};   // Available to target color

    // Indicator size and position (computed each frame)
    float indicator_size = 48.0f;
    float indicator_scale = 1.0f;                    // Animation scale
    Vec2 screen_position{0.0f, 0.0f};
    bool is_on_screen = true;
    float distance_to_camera = 0.0f;
};

// ============================================================================
// Target Candidate
// ============================================================================

struct TargetCandidate {
    Entity entity = NullEntity;
    float distance = 0.0f;                      // Distance from targeter
    float angle = 0.0f;                         // Angle from forward direction
    float score = 0.0f;                         // Combined priority score
    Vec3 target_point;                          // World position to aim at
    bool in_line_of_sight = true;
    bool is_current_target = false;             // Is the current locked target
};

// Direction for target switching
enum class SwitchDirection : uint8_t {
    Left,
    Right,
    Up,
    Down,
    Nearest,
    Farthest,
    Next,       // Cycle to next target
    Previous    // Cycle to previous target
};

// Reason for losing a target
enum class TargetLostReason : uint8_t {
    Manual,             // Player manually unlocked
    OutOfRange,         // Target too far away
    LineOfSightLost,    // Lost visibility for too long
    TargetDied,         // Target entity destroyed/killed
    TargetDisabled,     // Targetable component disabled
    SwitchedTarget      // Switched to different target
};

// ============================================================================
// Events
// ============================================================================

struct TargetAcquiredEvent {
    Entity targeter;
    Entity target;
    bool is_hard_lock;                          // Hard lock vs soft lock
};

struct TargetLostEvent {
    Entity targeter;
    Entity previous_target;
    TargetLostReason reason;
};

struct TargetSwitchedEvent {
    Entity targeter;
    Entity old_target;
    Entity new_target;
    SwitchDirection direction;
};

// ============================================================================
// Targeting System
// ============================================================================

// Callback for target changes
using TargetChangedCallback = std::function<void(Entity targeter, Entity old_target, Entity new_target)>;

// Line of sight check function
using LineOfSightCheck = std::function<bool(World&, const Vec3&, const Vec3&, Entity exclude)>;

class TargetingSystem {
public:
    static TargetingSystem& instance();

    // Delete copy/move
    TargetingSystem(const TargetingSystem&) = delete;
    TargetingSystem& operator=(const TargetingSystem&) = delete;

    // ========================================================================
    // Target Acquisition
    // ========================================================================

    // Find the best target for a targeter entity
    std::optional<TargetCandidate> find_best_target(
        World& world,
        Entity targeter,
        const Vec3& position,
        const Vec3& forward
    );

    // Find all valid targets sorted by score
    std::vector<TargetCandidate> find_all_targets(
        World& world,
        Entity targeter,
        const Vec3& position,
        const Vec3& forward,
        float max_distance = 0.0f  // 0 = use component setting
    );

    // Check if a specific entity can be targeted
    std::optional<TargetCandidate> can_target(
        World& world,
        Entity targeter,
        Entity target,
        const Vec3& position,
        const Vec3& forward
    );

    // ========================================================================
    // Lock-On Management
    // ========================================================================

    // Engage lock-on to a target
    void lock_on(World& world, Entity targeter, Entity target);

    // Disengage lock-on
    void unlock(World& world, Entity targeter);

    // Toggle lock-on (lock to best target if unlocked, unlock if locked)
    void toggle_lock_on(World& world, Entity targeter, const Vec3& position, const Vec3& forward);

    // Check if targeter is locked on
    bool is_locked_on(World& world, Entity targeter) const;

    // Get current lock-on target
    Entity get_current_target(World& world, Entity targeter) const;

    // ========================================================================
    // Target Switching
    // ========================================================================

    // Switch to a different target
    Entity switch_target(
        World& world,
        Entity targeter,
        const Vec3& position,
        const Vec3& forward,
        SwitchDirection direction
    );

    // Switch to next/previous target in list
    Entity cycle_target(World& world, Entity targeter, const Vec3& position, const Vec3& forward, bool next);

    // ========================================================================
    // Soft Lock / Aim Assist
    // ========================================================================

    // Get soft lock target (for aim assist)
    Entity get_soft_lock_target(World& world, Entity targeter) const;

    // Calculate aim assist direction
    Vec3 get_aim_assist_direction(
        World& world,
        Entity targeter,
        const Vec3& current_aim_direction,
        float assist_strength = -1.0f  // -1 = use component setting
    );

    // Get target point for aiming (accounts for soft/hard lock)
    Vec3 get_target_point(World& world, Entity targeter) const;

    // ========================================================================
    // Validation
    // ========================================================================

    // Check if current target is still valid
    bool validate_target(World& world, Entity targeter, const Vec3& position, const Vec3& forward);

    // ========================================================================
    // Callbacks and Configuration
    // ========================================================================

    // Set callback for target changes
    void set_on_target_changed(TargetChangedCallback callback);

    // Set custom line of sight check (defaults to physics raycast)
    void set_line_of_sight_check(LineOfSightCheck check);

    // Configuration
    void set_default_max_distance(float distance) { m_default_max_distance = distance; }
    void set_default_lock_angle(float angle) { m_default_lock_angle = angle; }

private:
    TargetingSystem();
    ~TargetingSystem() = default;

    // Internal helpers
    TargetCandidate evaluate_target(
        World& world,
        Entity targeter,
        Entity target,
        const TargeterComponent& targeter_comp,
        const TargetableComponent& targetable,
        const Vec3& position,
        const Vec3& forward
    );

    float calculate_score(
        const TargetCandidate& candidate,
        const TargeterComponent& targeter,
        const TargetableComponent& targetable,
        bool is_current_target
    );

    bool default_line_of_sight_check(World& world, const Vec3& from, const Vec3& to, Entity exclude);

    bool is_faction_targetable(const TargeterComponent& targeter, const std::string& faction) const;

    Vec3 get_target_world_point(World& world, Entity target, const TargetableComponent& targetable) const;

    void notify_target_changed(Entity targeter, Entity old_target, Entity new_target);

    // State
    TargetChangedCallback m_on_target_changed;
    LineOfSightCheck m_line_of_sight_check;

    float m_default_max_distance = 20.0f;
    float m_default_lock_angle = 60.0f;
};

// Convenience accessor
inline TargetingSystem& targeting() {
    return TargetingSystem::instance();
}

// ============================================================================
// ECS Systems
// ============================================================================

// Update targeting state (PreUpdate phase)
void targeting_system(World& world, double dt);

// Update soft lock targets (PreUpdate phase, after targeting_system)
void soft_lock_system(World& world, double dt);

// Update target indicators (PreRender phase)
void target_indicator_system(World& world, double dt);

// ============================================================================
// Registration
// ============================================================================

// Register targeting components with reflection
void register_targeting_components();

} // namespace engine::scene
