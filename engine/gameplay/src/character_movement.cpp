#include <engine/gameplay/character_movement.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/physics/physics_world.hpp>
#include <engine/physics/character_controller.hpp>
#include <engine/stats/stat_component.hpp>
#include <engine/render/animation_state_machine.hpp>
#include <engine/reflect/type_registry.hpp>
#include <algorithm>
#include <cmath>

namespace engine::gameplay {

// ============================================================================
// Movement State String Conversion
// ============================================================================

const char* movement_state_to_string(MovementState state) {
    switch (state) {
        case MovementState::Idle:             return "Idle";
        case MovementState::Walking:          return "Walking";
        case MovementState::Running:          return "Running";
        case MovementState::Sprinting:        return "Sprinting";
        case MovementState::Crouching:        return "Crouching";
        case MovementState::CrouchWalking:    return "CrouchWalking";
        case MovementState::Sliding:          return "Sliding";
        case MovementState::Jumping:          return "Jumping";
        case MovementState::Falling:          return "Falling";
        case MovementState::Landing:          return "Landing";
        case MovementState::Climbing:         return "Climbing";
        case MovementState::Mantling:         return "Mantling";
        case MovementState::Swimming:         return "Swimming";
        case MovementState::SwimmingUnderwater: return "SwimmingUnderwater";
        case MovementState::Diving:           return "Diving";
        case MovementState::Surfacing:        return "Surfacing";
        case MovementState::Treading:         return "Treading";
        default:                              return "Unknown";
    }
}

// ============================================================================
// CharacterMovementComponent Implementation
// ============================================================================

bool CharacterMovementComponent::can_sprint() const {
    if (movement_locked) return false;
    if (sprint_cooldown_remaining > 0.0f) return false;
    if (is_crouching() || is_sliding() || is_mantling()) return false;
    if (!is_grounded()) return false;

    // Input check - must have forward movement intent
    if (glm::length(input_direction) < 0.1f) return false;

    return true;
}

bool CharacterMovementComponent::can_slide() const {
    if (movement_locked) return false;
    if (slide_cooldown_remaining > 0.0f) return false;
    if (is_sliding() || is_mantling() || is_airborne()) return false;

    // Slide requires sprint state if configured
    if (settings.slide_requires_sprint && !is_sprinting()) return false;

    return true;
}

bool CharacterMovementComponent::can_mantle() const {
    if (movement_locked) return false;
    if (is_mantling() || is_sliding()) return false;

    // Can only mantle from jumping/falling or when pressing jump near ledge
    return is_airborne() || (is_grounded() && wants_jump);
}

float CharacterMovementComponent::get_target_speed() const {
    switch (state) {
        case MovementState::Idle:
        case MovementState::Crouching:
        case MovementState::Landing:
            return 0.0f;

        case MovementState::Walking:
            return settings.walk_speed;

        case MovementState::Running:
            return settings.run_speed;

        case MovementState::Sprinting:
            return settings.sprint_speed;

        case MovementState::CrouchWalking:
            return settings.crouch_speed;

        case MovementState::Sliding:
            return settings.slide_speed;

        case MovementState::Jumping:
        case MovementState::Falling:
            // Air control uses run speed as base
            return settings.run_speed;

        case MovementState::Mantling:
        case MovementState::Climbing:
            return 0.0f;  // Handled separately

        default:
            return 0.0f;
    }
}

float CharacterMovementComponent::get_speed_normalized() const {
    if (settings.sprint_speed <= 0.0f) return 0.0f;
    return current_speed / settings.sprint_speed;
}

// ============================================================================
// Movement System
// ============================================================================

namespace {

// Helper to check if input is significant
bool has_movement_input(const CharacterMovementComponent& movement) {
    return glm::length(movement.input_direction) > 0.1f;
}

// Update crouch interpolation
void update_crouch_amount(CharacterMovementComponent& movement, float dt) {
    float target = movement.is_crouching() ? 1.0f : 0.0f;
    float speed = 1.0f / movement.settings.crouch_transition_time;

    if (movement.crouch_amount < target) {
        movement.crouch_amount = std::min(target, movement.crouch_amount + speed * dt);
    } else if (movement.crouch_amount > target) {
        movement.crouch_amount = std::max(target, movement.crouch_amount - speed * dt);
    }
}

// Update cooldowns
void update_cooldowns(CharacterMovementComponent& movement, float dt) {
    if (movement.slide_cooldown_remaining > 0.0f) {
        movement.slide_cooldown_remaining -= dt;
    }
    if (movement.sprint_cooldown_remaining > 0.0f) {
        movement.sprint_cooldown_remaining -= dt;
    }
}

// Process grounded state transitions
void process_grounded_states(
    CharacterMovementComponent& movement,
    stats::StatsComponent* stats,
    bool is_physics_grounded,
    float dt
) {
    // Handle landing recovery
    if (movement.state == MovementState::Landing) {
        if (movement.state_time >= movement.settings.landing_recovery_time) {
            movement.set_state(MovementState::Idle);
        }
        return;
    }

    // Check for crouch request
    if (movement.wants_crouch) {
        if (has_movement_input(movement)) {
            movement.set_state(MovementState::CrouchWalking);
        } else {
            movement.set_state(MovementState::Crouching);
        }
        return;
    }

    // Check for slide (crouch while sprinting)
    if (movement.wants_crouch && movement.can_slide()) {
        movement.set_state(MovementState::Sliding);
        return;
    }

    // Handle sliding state
    if (movement.state == MovementState::Sliding) {
        if (movement.state_time >= movement.settings.slide_duration) {
            movement.slide_cooldown_remaining = movement.settings.slide_cooldown;
            if (movement.wants_crouch) {
                movement.set_state(MovementState::Crouching);
            } else {
                movement.set_state(MovementState::Idle);
            }
        }
        return;
    }

    // Normal movement state transitions
    if (!has_movement_input(movement)) {
        // No input - go idle
        movement.set_state(MovementState::Idle);
        return;
    }

    // Has movement input
    bool can_sprint_now = movement.can_sprint() && movement.wants_sprint;

    // Check stamina for sprint
    if (can_sprint_now && movement.settings.sprint_requires_stamina && stats) {
        float current_stamina = stats->get_current(stats::StatType::Stamina);
        if (current_stamina < movement.settings.sprint_stamina_threshold) {
            can_sprint_now = false;
            movement.sprint_cooldown_remaining = movement.settings.sprint_cooldown;
        }
    }

    if (can_sprint_now) {
        movement.set_state(MovementState::Sprinting);

        // Drain stamina while sprinting
        if (stats && movement.settings.sprint_requires_stamina) {
            stats->modify_current(stats::StatType::Stamina,
                                  -movement.settings.sprint_stamina_drain * dt);

            // Check if stamina depleted
            if (stats->is_depleted(stats::StatType::Stamina)) {
                movement.sprint_cooldown_remaining = movement.settings.sprint_cooldown;
            }
        }
    } else {
        // Running or walking based on input magnitude
        float input_magnitude = glm::length(movement.input_direction);
        if (input_magnitude > 0.5f) {
            movement.set_state(MovementState::Running);
        } else {
            movement.set_state(MovementState::Walking);
        }
    }
}

// Process airborne state transitions
void process_airborne_states(
    CharacterMovementComponent& movement,
    physics::CharacterController* controller,
    bool is_physics_grounded
) {
    // Check for landing
    if (is_physics_grounded) {
        movement.landing_time = movement.state_time;
        movement.set_state(MovementState::Landing);
        return;
    }

    // Check vertical velocity to determine jumping vs falling
    if (controller) {
        Vec3 velocity = controller->get_velocity();
        if (velocity.y < -0.1f && movement.state == MovementState::Jumping) {
            movement.set_state(MovementState::Falling);
        }
    }
}

// Process mantle state
void process_mantle_state(CharacterMovementComponent& movement, float dt) {
    movement.mantle_progress += dt / movement.settings.mantle_duration;

    if (movement.mantle_progress >= 1.0f) {
        movement.mantle_progress = 0.0f;
        movement.set_state(MovementState::Idle);
    }
}

// Update animator parameters
void update_animator_params(
    render::AnimatorComponent* animator,
    const CharacterMovementComponent& movement
) {
    if (!animator || !animator->state_machine) return;

    auto& sm = *animator->state_machine;

    // Speed parameter (0-1 normalized)
    sm.set_float("Speed", movement.get_speed_normalized());

    // Boolean state flags
    sm.set_bool("IsGrounded", movement.is_grounded());
    sm.set_bool("IsCrouching", movement.is_crouching());
    sm.set_bool("IsSprinting", movement.is_sprinting());
    sm.set_bool("IsSliding", movement.is_sliding());
    sm.set_bool("IsMantling", movement.is_mantling());

    // Crouch blend
    sm.set_float("CrouchAmount", movement.crouch_amount);
}

} // anonymous namespace

void character_movement_system(scene::World& world, double dt) {
    float delta = static_cast<float>(dt);

    auto view = world.view<CharacterMovementComponent>();

    for (auto entity : view) {
        auto& movement = view.get<CharacterMovementComponent>(entity);

        // Skip if movement is completely locked
        if (movement.movement_locked) {
            movement.state_time += delta;
            continue;
        }

        // Get optional components
        auto* controller = world.try_get<physics::CharacterControllerComponent>(entity);
        auto* stats = world.try_get<stats::StatsComponent>(entity);
        auto* animator = world.try_get<render::AnimatorComponent>(entity);

        // Get ground state from physics
        bool is_physics_grounded = false;
        if (controller && controller->controller) {
            is_physics_grounded = controller->controller->is_grounded();
        }

        // Update cooldowns
        update_cooldowns(movement, delta);

        // Process mantle state (special case)
        if (movement.state == MovementState::Mantling) {
            process_mantle_state(movement, delta);
        }
        // Process based on current grounded state
        else if (movement.is_grounded()) {
            // Check for jump transition to airborne
            if (movement.wants_jump && movement.state != MovementState::Sliding) {
                if (controller && controller->controller) {
                    controller->controller->jump();
                }
                movement.set_state(MovementState::Jumping);
            } else {
                process_grounded_states(movement, stats, is_physics_grounded, delta);
            }
        }
        else if (movement.is_airborne()) {
            process_airborne_states(movement, controller ? controller->controller.get() : nullptr, is_physics_grounded);
        }

        // Update crouch interpolation
        update_crouch_amount(movement, delta);

        // Calculate target speed and acceleration
        float target_speed = movement.get_target_speed();
        float accel = (target_speed > movement.current_speed)
                      ? movement.settings.acceleration
                      : movement.settings.deceleration;

        // Interpolate current speed
        if (movement.current_speed < target_speed) {
            movement.current_speed = std::min(target_speed, movement.current_speed + accel * delta);
        } else if (movement.current_speed > target_speed) {
            movement.current_speed = std::max(target_speed, movement.current_speed - accel * delta);
        }

        // Apply movement to physics controller
        if (controller && controller->controller && !movement.is_mantling()) {
            Vec3 move_dir = movement.input_direction;
            if (glm::length(move_dir) > 0.01f) {
                move_dir = glm::normalize(move_dir);
            }
            controller->controller->set_movement_input(move_dir * movement.current_speed);

            // Apply sprint speed modifier
            if (movement.is_sprinting()) {
                controller->controller->set_movement_speed(movement.settings.sprint_speed);
            } else if (movement.is_crouching()) {
                controller->controller->set_movement_speed(movement.settings.crouch_speed);
            } else {
                controller->controller->set_movement_speed(movement.settings.run_speed);
            }
        }

        // Update animator
        update_animator_params(animator, movement);

        // Update state timer
        movement.state_time += delta;

        // Clear one-shot inputs
        movement.wants_jump = false;
    }
}

// ============================================================================
// Mantle Detection
// ============================================================================

MantleCheckResult check_mantle(
    physics::PhysicsWorld& physics,
    const Vec3& position,
    const Vec3& forward,
    const MovementSettings& settings
) {
    MantleCheckResult result;

    // Normalize forward direction
    Vec3 dir = glm::normalize(Vec3(forward.x, 0.0f, forward.z));
    if (glm::length(dir) < 0.01f) {
        dir = Vec3(0.0f, 0.0f, 1.0f);
    }

    // Cast forward to find wall
    Vec3 wall_check_start = position + Vec3(0.0f, settings.mantle_min_height, 0.0f);

    auto wall_hit = physics.raycast(wall_check_start, dir, settings.mantle_check_distance, 0xFFFF);
    if (!wall_hit.hit) {
        return result;  // No wall found
    }

    Vec3 wall_point = wall_hit.point;
    Vec3 wall_normal = wall_hit.normal;

    // Cast down from above to find ledge top
    Vec3 ledge_check_start = wall_point - dir * 0.1f;  // Step back slightly
    ledge_check_start.y = position.y + settings.mantle_max_height + 0.5f;  // Start above max height

    float ledge_check_distance = settings.mantle_max_height - settings.mantle_min_height + 0.5f;

    auto ledge_hit = physics.raycast(ledge_check_start, Vec3(0.0f, -1.0f, 0.0f), ledge_check_distance, 0xFFFF);
    if (!ledge_hit.hit) {
        return result;  // No ledge found
    }

    // Calculate ledge height relative to character
    float ledge_height = ledge_hit.point.y - position.y;

    // Check height is within range
    if (ledge_height < settings.mantle_min_height ||
        ledge_height > settings.mantle_max_height) {
        return result;
    }

    // Check there's room on top of the ledge
    Vec3 top_check_start = ledge_hit.point + Vec3(0.0f, 0.5f, 0.0f) + dir * 0.3f;

    auto top_hit = physics.raycast(top_check_start, Vec3(0.0f, -1.0f, 0.0f), 0.4f, 0xFFFF);
    if (!top_hit.hit) {
        return result;  // No floor on top of ledge (thin ledge)
    }

    // Success - populate result
    result.can_mantle = true;
    result.start_position = position;
    result.end_position = top_hit.point + Vec3(0.0f, 0.1f, 0.0f) + dir * 0.2f;
    result.height = ledge_height;
    result.ledge_normal = ledge_hit.normal;

    return result;
}

bool check_stand_obstruction(
    physics::PhysicsWorld& physics,
    const Vec3& position,
    float standing_height,
    float crouched_height
) {
    // Cast upward from crouch height to standing height
    Vec3 start = position + Vec3(0.0f, crouched_height, 0.0f);
    float distance = standing_height - crouched_height;

    auto hit = physics.raycast(start, Vec3(0.0f, 1.0f, 0.0f), distance, 0xFFFF);
    return hit.hit;  // If hit, there's obstruction
}

// ============================================================================
// Component Registration
// ============================================================================

void register_gameplay_components() {
    using namespace reflect;

    // Register MovementState enum
    TypeRegistry::instance().register_enum<MovementState>("MovementState", {
        {MovementState::Idle, "Idle"},
        {MovementState::Walking, "Walking"},
        {MovementState::Running, "Running"},
        {MovementState::Sprinting, "Sprinting"},
        {MovementState::Crouching, "Crouching"},
        {MovementState::CrouchWalking, "CrouchWalking"},
        {MovementState::Sliding, "Sliding"},
        {MovementState::Jumping, "Jumping"},
        {MovementState::Falling, "Falling"},
        {MovementState::Landing, "Landing"},
        {MovementState::Climbing, "Climbing"},
        {MovementState::Mantling, "Mantling"},
        {MovementState::Swimming, "Swimming"},
        {MovementState::SwimmingUnderwater, "SwimmingUnderwater"},
        {MovementState::Diving, "Diving"},
        {MovementState::Surfacing, "Surfacing"},
        {MovementState::Treading, "Treading"}
    });

    // Register CharacterMovementComponent
    TypeRegistry::instance().register_component<CharacterMovementComponent>(
        "CharacterMovementComponent",
        TypeMeta().set_display_name("Character Movement").set_category(TypeCategory::Component)
    );
}

} // namespace engine::gameplay
