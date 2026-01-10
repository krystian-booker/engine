#include <engine/companion/companion_ai.hpp>
#include <engine/companion/companion.hpp>
#include <engine/companion/formation.hpp>
#include <engine/companion/party_manager.hpp>
#include <engine/scene/transform.hpp>
#include <engine/navigation/nav_agent.hpp>
#include <engine/core/game_events.hpp>
#include <algorithm>

namespace engine::companion {

namespace {

// Get leader position and forward vector
bool get_leader_transform(
    scene::World& world,
    scene::Entity leader,
    Vec3& out_position,
    Vec3& out_forward
) {
    if (leader == scene::NullEntity) return false;

    auto* transform = world.try_get<scene::WorldTransform>(leader);
    if (!transform) return false;

    out_position = transform->position();
    Quat rot = transform->rotation();
    out_forward = rot * Vec3(0.0f, 0.0f, 1.0f);

    return true;
}

// Get companion position
Vec3 get_companion_position(scene::World& world, scene::Entity companion) {
    auto* transform = world.try_get<scene::WorldTransform>(companion);
    return transform ? transform->position() : Vec3(0.0f);
}

// Calculate target follow position based on formation
Vec3 calculate_target_position(
    scene::World& /*world*/,
    CompanionComponent& comp,
    const Vec3& leader_pos,
    const Vec3& leader_forward
) {
    // If in formation, use formation position
    if (comp.formation_slot >= 0) {
        const auto& formation = party_manager().get_formation();
        return calculate_formation_position(
            formation, comp.formation_slot, leader_pos, leader_forward
        );
    }

    // Otherwise, just follow behind
    return leader_pos - leader_forward * comp.follow_distance;
}

} // anonymous namespace

// ============================================================================
// Follow System
// ============================================================================

void companion_follow_system(scene::World& world, double /*dt*/) {
    auto view = world.view<CompanionComponent>();

    for (auto entity : view) {
        auto& comp = view.get<CompanionComponent>(entity);

        // Skip if not following
        if (comp.state != CompanionState::Following) {
            continue;
        }

        // Skip if no owner
        if (comp.owner == scene::NullEntity) {
            continue;
        }

        Vec3 leader_pos, leader_forward;
        if (!get_leader_transform(world, comp.owner, leader_pos, leader_forward)) {
            continue;
        }

        Vec3 companion_pos = get_companion_position(world, entity);
        Vec3 target_pos = calculate_target_position(world, comp, leader_pos, leader_forward);

        float distance_to_target = glm::distance(companion_pos, target_pos);
        float distance_to_leader = glm::distance(companion_pos, leader_pos);

        // Check if close enough to stop
        if (distance_to_target < 0.5f) {
            // At destination, stop moving
            if (auto* agent = world.try_get<navigation::NavAgentComponent>(entity)) {
                agent->has_target = false;
                agent->state = navigation::NavAgentState::Idle;
            }
            continue;
        }

        // Determine movement speed
        float speed_mult = comp.follow_speed_multiplier;
        if (distance_to_leader > comp.catch_up_distance) {
            speed_mult = comp.catch_up_speed_multiplier;
        }

        // Move toward target using nav agent
        if (auto* agent = world.try_get<navigation::NavAgentComponent>(entity)) {
            agent->target = target_pos;
            agent->has_target = true;
            agent->speed = agent->speed * speed_mult;  // Adjust base speed
        }
    }
}

// ============================================================================
// Combat System
// ============================================================================

void companion_combat_system(scene::World& world, double dt) {
    float delta = static_cast<float>(dt);

    auto view = world.view<CompanionComponent>();

    for (auto entity : view) {
        auto& comp = view.get<CompanionComponent>(entity);

        // Update combat timer
        if (comp.is_in_combat()) {
            comp.time_in_combat += delta;
        } else {
            comp.time_in_combat = 0.0f;
        }

        // Skip passive companions
        if (comp.combat_behavior == CombatBehavior::Passive) {
            continue;
        }

        // Skip dead companions
        if (comp.state == CompanionState::Dead) {
            continue;
        }

        Vec3 companion_pos = get_companion_position(world, entity);

        // If already in combat, check if should disengage
        if (comp.state == CompanionState::Attacking) {
            // Check if target is still valid
            if (comp.combat_target == scene::NullEntity ||
                !world.valid(comp.combat_target)) {
                comp.combat_target = scene::NullEntity;
                comp.set_state(CompanionState::Following);
                continue;
            }

            // Check distance to target
            Vec3 target_pos = get_companion_position(world, comp.combat_target);
            float distance = glm::distance(companion_pos, target_pos);

            if (distance > comp.disengage_range) {
                comp.combat_target = scene::NullEntity;
                comp.set_state(CompanionState::Following);
            }

            continue;
        }

        // Note: Auto-engagement and perception-based targeting would require
        // game-specific perception and faction systems. The companion component
        // provides the combat_target field for games to set directly.
    }
}

// ============================================================================
// Command System
// ============================================================================

void companion_command_system(scene::World& world, double /*dt*/) {
    auto view = world.view<CompanionComponent>();

    for (auto entity : view) {
        auto& comp = view.get<CompanionComponent>(entity);

        // Process move commands
        if (comp.state == CompanionState::Moving) {
            Vec3 companion_pos = get_companion_position(world, entity);
            float distance = glm::distance(companion_pos, comp.command_position);

            if (distance < 1.0f) {
                // Reached destination
                comp.set_state(CompanionState::Waiting);
                comp.wait_position = comp.command_position;
            } else {
                // Keep moving toward target
                if (auto* agent = world.try_get<navigation::NavAgentComponent>(entity)) {
                    agent->target = comp.command_position;
                    agent->has_target = true;
                }
            }
        }

        // Process interact commands
        if (comp.state == CompanionState::Interacting) {
            // Check if interaction target is still valid
            if (comp.command_target == scene::NullEntity ||
                !world.valid(comp.command_target)) {
                comp.set_state(CompanionState::Following);
            }
            // Note: Actual interaction is handled by game-specific code
        }

        // Process defend commands
        if (comp.state == CompanionState::Defending) {
            // Defend position - stay near and attack threats
            Vec3 defend_pos = (comp.command_target != scene::NullEntity)
                ? get_companion_position(world, comp.command_target)
                : comp.command_position;

            Vec3 companion_pos = get_companion_position(world, entity);
            float distance = glm::distance(companion_pos, defend_pos);

            // Move toward defend position if too far
            if (distance > 5.0f) {
                if (auto* agent = world.try_get<navigation::NavAgentComponent>(entity)) {
                    agent->target = defend_pos;
                    agent->has_target = true;
                }
            }
        }
    }
}

// ============================================================================
// Teleport System
// ============================================================================

void companion_teleport_system(scene::World& world, double dt) {
    float delta = static_cast<float>(dt);

    auto view = world.view<CompanionComponent>();

    for (auto entity : view) {
        auto& comp = view.get<CompanionComponent>(entity);

        // Update teleport cooldown
        comp.time_since_teleport += delta;

        // Skip if teleport not enabled
        if (!comp.teleport_if_too_far) {
            continue;
        }

        // Skip if not following
        if (comp.state != CompanionState::Following) {
            continue;
        }

        // Skip if on cooldown
        if (comp.time_since_teleport < comp.teleport_cooldown) {
            continue;
        }

        // Check distance to leader
        if (comp.owner == scene::NullEntity) {
            continue;
        }

        Vec3 leader_pos, leader_forward;
        if (!get_leader_transform(world, comp.owner, leader_pos, leader_forward)) {
            continue;
        }

        Vec3 companion_pos = get_companion_position(world, entity);
        float distance = glm::distance(companion_pos, leader_pos);

        if (distance > comp.teleport_distance) {
            // Teleport to formation position
            Vec3 target_pos = calculate_target_position(world, comp, leader_pos, leader_forward);

            // Set position directly
            if (auto* transform = world.try_get<scene::LocalTransform>(entity)) {
                transform->position = target_pos;
            }

            // Reset nav agent state
            if (auto* agent = world.try_get<navigation::NavAgentComponent>(entity)) {
                agent->has_target = false;
                agent->state = navigation::NavAgentState::Idle;
                agent->path.clear();
            }

            comp.time_since_teleport = 0.0f;
        }
    }
}

// ============================================================================
// Registration
// ============================================================================

void register_companion_systems(scene::World& /*world*/) {
    // Systems are registered via the scheduler in Application
    // This function provides the system functions for registration

    // Follow system - FixedUpdate, after AI
    // Combat system - FixedUpdate, after follow
    // Command system - Update
    // Teleport system - PostUpdate
}

} // namespace engine::companion
