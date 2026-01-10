#pragma once

#include <engine/core/math.hpp>
#include <engine/scene/entity.hpp>
#include <cstdint>

namespace engine::physics { class PhysicsWorld; }
namespace engine::scene { class World; }

namespace engine::gameplay {

using namespace engine::core;

// ============================================================================
// Movement States
// ============================================================================

enum class MovementState : uint8_t {
    Idle,           // Standing still
    Walking,        // Slow movement
    Running,        // Normal movement
    Sprinting,      // Fast movement (drains stamina)
    Crouching,      // Standing crouch
    CrouchWalking,  // Moving while crouched
    Sliding,        // Momentum-based slide
    Jumping,        // Rising in the air
    Falling,        // Descending
    Landing,        // Brief state on ground contact
    Climbing,       // On a ladder or wall
    Mantling,       // Climbing over a ledge

    // Water states
    Swimming,           // On water surface, moving
    SwimmingUnderwater, // Submerged and moving
    Diving,             // Transitioning from surface to underwater
    Surfacing,          // Transitioning from underwater to surface
    Treading            // Stationary on water surface
};

// Convert state to string for debugging
const char* movement_state_to_string(MovementState state);

// ============================================================================
// Movement Settings
// ============================================================================

struct MovementSettings {
    // Speed values (units per second)
    float walk_speed = 2.0f;
    float run_speed = 5.0f;
    float sprint_speed = 8.0f;
    float crouch_speed = 1.5f;

    // Speed transition
    float acceleration = 20.0f;         // How fast to reach target speed
    float deceleration = 30.0f;         // How fast to stop
    float turn_speed = 10.0f;           // Rotation speed (radians/sec)

    // Sprint settings
    bool sprint_requires_stamina = true;
    float sprint_stamina_drain = 20.0f;     // Stamina per second
    float sprint_stamina_threshold = 10.0f; // Min stamina to start sprinting
    float sprint_cooldown = 0.5f;           // Cooldown after stamina depleted

    // Crouch settings
    float crouch_height_ratio = 0.6f;   // Height multiplier when crouched
    float crouch_transition_time = 0.2f; // Time to crouch/stand

    // Slide settings
    bool slide_requires_sprint = true;
    float slide_speed = 10.0f;
    float slide_duration = 0.8f;
    float slide_friction = 5.0f;
    float slide_cooldown = 1.0f;
    float slide_input_window = 0.2f;    // Time window to trigger slide after crouch

    // Jump settings (note: physics CharacterController handles actual jump)
    float landing_recovery_time = 0.1f; // Brief pause after landing

    // Mantle settings
    float mantle_check_distance = 1.0f;
    float mantle_min_height = 0.5f;     // Minimum height to trigger mantle
    float mantle_max_height = 2.2f;     // Maximum height to mantle
    float mantle_duration = 0.5f;       // Time to complete mantle
    bool mantle_auto_trigger = true;    // Auto-mantle when jumping at ledge
};

// ============================================================================
// Water Movement Settings
// ============================================================================

struct WaterMovementSettings {
    // Speed values (units per second)
    float swim_speed = 3.0f;                // Surface swimming speed
    float swim_sprint_speed = 5.0f;         // Fast surface swimming
    float underwater_speed = 2.5f;          // Submerged movement speed
    float underwater_sprint_speed = 4.0f;   // Fast underwater movement
    float vertical_swim_speed = 2.0f;       // Ascending/descending speed
    float dive_speed = 3.0f;                // Speed when diving down
    float surface_speed = 4.0f;             // Speed when surfacing

    // Transition settings
    float surface_detection_offset = 0.3f;  // Height above water to detect surface
    float submerge_depth = 1.5f;            // Depth considered fully underwater
    float dive_transition_time = 0.3f;      // Time to transition to underwater
    float surface_transition_time = 0.4f;   // Time to transition to surface

    // Breath settings
    bool breath_enabled = true;
    float max_breath = 30.0f;               // Seconds of breath underwater
    float breath_recovery_rate = 2.0f;      // Breath per second while above water
    float drowning_damage_rate = 10.0f;     // Damage per second when out of breath
    float drowning_damage_interval = 1.0f;  // Time between drowning damage ticks

    // Stamina interaction
    float stamina_drain_underwater = 5.0f;  // Stamina per second while underwater
    float stamina_drain_sprint = 10.0f;     // Stamina per second when sprint-swimming

    // Physics modifiers
    float water_drag = 2.0f;                // Movement resistance in water
    float buoyancy_force = 1.0f;            // Upward force at surface
    float gravity_underwater = 0.1f;        // Reduced gravity while submerged

    // Input settings
    bool can_dive = true;                   // Allow diving underwater
    bool auto_surface = true;               // Auto-surface when out of breath
    float dive_input_threshold = 0.5f;      // Crouch input threshold to dive
};

// ============================================================================
// Mantle Detection Result
// ============================================================================

struct MantleCheckResult {
    bool can_mantle = false;
    Vec3 start_position{0.0f};      // Where mantle begins
    Vec3 end_position{0.0f};        // Where to end up after mantle
    float height = 0.0f;            // Height of the ledge
    Vec3 ledge_normal{0.0f, 1.0f, 0.0f};  // Normal of the ledge surface
};

// ============================================================================
// Character Movement Component
// ============================================================================

struct CharacterMovementComponent {
    MovementSettings settings;
    WaterMovementSettings water_settings;

    // Current state
    MovementState state = MovementState::Idle;
    MovementState previous_state = MovementState::Idle;

    // Input (normalized direction in world space)
    Vec3 input_direction{0.0f};
    bool wants_sprint = false;
    bool wants_crouch = false;
    bool wants_jump = false;

    // Current velocity and speed
    Vec3 desired_velocity{0.0f};
    float current_speed = 0.0f;

    // State timers
    float state_time = 0.0f;            // Time in current state
    float slide_cooldown_remaining = 0.0f;
    float sprint_cooldown_remaining = 0.0f;
    float mantle_progress = 0.0f;

    // Crouch interpolation (0 = standing, 1 = crouched)
    float crouch_amount = 0.0f;

    // Mantle data
    Vec3 mantle_start{0.0f};
    Vec3 mantle_end{0.0f};

    // Landing recovery
    float landing_time = 0.0f;

    // External flags (set by other systems)
    bool movement_locked = false;       // Completely prevent movement
    bool rotation_locked = false;       // Prevent rotation

    // Water state
    float current_breath = 30.0f;       // Current breath remaining (seconds)
    float water_depth = 0.0f;           // How deep in water (0 = at surface, negative = above)
    float water_surface_height = 0.0f;  // World Y of water surface
    float drowning_timer = 0.0f;        // Time since last drowning damage
    scene::Entity current_water_volume = scene::NullEntity;  // Active water volume
    bool wants_dive = false;            // Player input: dive underwater
    bool wants_surface = false;         // Player input: surface from underwater

    // ========================================================================
    // State Queries
    // ========================================================================

    bool is_grounded() const {
        return state == MovementState::Idle ||
               state == MovementState::Walking ||
               state == MovementState::Running ||
               state == MovementState::Sprinting ||
               state == MovementState::Crouching ||
               state == MovementState::CrouchWalking ||
               state == MovementState::Sliding ||
               state == MovementState::Landing;
    }

    bool is_moving() const {
        return state == MovementState::Walking ||
               state == MovementState::Running ||
               state == MovementState::Sprinting ||
               state == MovementState::CrouchWalking ||
               state == MovementState::Sliding;
    }

    bool is_airborne() const {
        return state == MovementState::Jumping ||
               state == MovementState::Falling;
    }

    bool is_sprinting() const {
        return state == MovementState::Sprinting;
    }

    bool is_crouching() const {
        return state == MovementState::Crouching ||
               state == MovementState::CrouchWalking;
    }

    bool is_sliding() const {
        return state == MovementState::Sliding;
    }

    bool is_mantling() const {
        return state == MovementState::Mantling;
    }

    bool is_in_water() const {
        return state == MovementState::Swimming ||
               state == MovementState::SwimmingUnderwater ||
               state == MovementState::Diving ||
               state == MovementState::Surfacing ||
               state == MovementState::Treading;
    }

    bool is_underwater() const {
        return state == MovementState::SwimmingUnderwater ||
               state == MovementState::Diving;
    }

    bool is_on_water_surface() const {
        return state == MovementState::Swimming ||
               state == MovementState::Treading ||
               state == MovementState::Surfacing;
    }

    bool is_drowning() const {
        return is_underwater() && current_breath <= 0.0f;
    }

    bool is_swimming() const {
        return state == MovementState::Swimming ||
               state == MovementState::SwimmingUnderwater;
    }

    // ========================================================================
    // Capability Queries
    // ========================================================================

    // Can start sprinting? (checks stamina, cooldowns, state)
    bool can_sprint() const;

    // Can start sliding? (checks sprint state, cooldowns)
    bool can_slide() const;

    // Can start mantling? (checks state, not position - that's check_mantle)
    bool can_mantle() const;

    // Can stand up from crouch? (external check needed for obstruction)
    bool wants_stand() const {
        return !wants_crouch && is_crouching();
    }

    // Can dive underwater from surface?
    bool can_dive() const {
        return water_settings.can_dive && is_on_water_surface();
    }

    // Can surface from underwater?
    bool can_surface() const {
        return is_underwater();
    }

    // ========================================================================
    // State Transitions
    // ========================================================================

    void set_state(MovementState new_state) {
        if (state != new_state) {
            previous_state = state;
            state = new_state;
            state_time = 0.0f;
        }
    }

    // ========================================================================
    // Speed Calculations
    // ========================================================================

    // Get target speed based on current state and settings
    float get_target_speed() const;

    // Get speed multiplier from state (for animation blending)
    float get_speed_normalized() const;
};

// ============================================================================
// Movement System Functions
// ============================================================================

// Main movement system - call in FixedUpdate phase
// Updates movement state based on input and physics
void character_movement_system(scene::World& world, double dt);

// Check if mantle is possible at the given position
MantleCheckResult check_mantle(
    physics::PhysicsWorld& physics,
    const Vec3& position,
    const Vec3& forward,
    const MovementSettings& settings
);

// Check if standing up from crouch would cause collision
bool check_stand_obstruction(
    physics::PhysicsWorld& physics,
    const Vec3& position,
    float standing_height,
    float crouched_height
);

// ============================================================================
// Component Registration
// ============================================================================

void register_gameplay_components();

} // namespace engine::gameplay
