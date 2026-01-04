#include <engine/navigation/nav_behaviors.hpp>
#include <engine/navigation/nav_agent.hpp>
#include <engine/navigation/navigation_systems.hpp>
#include <engine/scene/transform.hpp>
#include <engine/core/log.hpp>

#include <random>

namespace engine::navigation {

namespace {

// Thread-local random number generator
thread_local std::mt19937 g_rng{std::random_device{}()};

float random_range(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(g_rng);
}

void update_wander(scene::World& world, scene::Entity entity,
                   NavBehaviorComponent& behavior, NavAgentComponent& agent, float dt) {
    auto* agent_system = get_agent_system();
    auto* pathfinder = get_pathfinder();
    if (!agent_system || !pathfinder) return;

    uint32_t entity_id = static_cast<uint32_t>(entity);

    // Initialize wander origin to current position if not set
    if (!behavior.behavior_started) {
        if (auto* transform = world.try_get<scene::LocalTransform>(entity)) {
            if (behavior.wander_origin == Vec3{0.0f}) {
                behavior.wander_origin = transform->position;
            }
        }
        behavior.behavior_started = true;
        behavior.wait_timer = 0.0f;  // Start immediately
    }

    // If idle or arrived, wait then pick new random point
    if (agent.state == NavAgentState::Idle || agent.state == NavAgentState::Arrived) {
        behavior.wait_timer -= dt;
        if (behavior.wait_timer <= 0) {
            // Pick random point within wander radius
            auto result = pathfinder->find_random_point_around(
                behavior.wander_origin, behavior.wander_radius);

            if (result.valid) {
                agent_system->set_destination(world, entity_id, result.point);
                behavior.wait_timer = random_range(behavior.wander_wait_min,
                                                   behavior.wander_wait_max);
            } else {
                // Failed to find point, try again next frame
                behavior.wait_timer = 0.1f;
            }
        }
    } else if (agent.state == NavAgentState::Failed) {
        // Path failed, pick new destination
        behavior.wait_timer = 0.0f;
    }
}

void update_patrol(scene::World& world, scene::Entity entity,
                   NavBehaviorComponent& behavior, NavAgentComponent& agent, float dt) {
    if (behavior.patrol_points.empty()) return;

    auto* agent_system = get_agent_system();
    if (!agent_system) return;

    uint32_t entity_id = static_cast<uint32_t>(entity);

    // Start patrol if not started
    if (!behavior.behavior_started) {
        behavior.behavior_started = true;
        behavior.patrol_index = 0;
        behavior.patrol_forward = true;
        agent_system->set_destination(world, entity_id,
            behavior.patrol_points[behavior.patrol_index]);
        return;
    }

    if (agent.state == NavAgentState::Arrived) {
        behavior.wait_timer -= dt;
        if (behavior.wait_timer <= 0) {
            // Move to next patrol point
            if (behavior.patrol_loop) {
                // Loop mode: wrap around
                behavior.patrol_index = (behavior.patrol_index + 1) %
                                        behavior.patrol_points.size();
            } else {
                // Ping-pong mode: reverse at ends
                if (behavior.patrol_forward) {
                    behavior.patrol_index++;
                    if (behavior.patrol_index >= behavior.patrol_points.size()) {
                        behavior.patrol_index = behavior.patrol_points.size() > 1 ?
                                               behavior.patrol_points.size() - 2 : 0;
                        behavior.patrol_forward = false;
                    }
                } else {
                    if (behavior.patrol_index == 0) {
                        behavior.patrol_forward = true;
                        if (behavior.patrol_points.size() > 1) {
                            behavior.patrol_index = 1;
                        }
                    } else {
                        behavior.patrol_index--;
                    }
                }
            }

            agent_system->set_destination(world, entity_id,
                behavior.patrol_points[behavior.patrol_index]);
            behavior.wait_timer = behavior.patrol_wait_time;
        }
    } else if (agent.state == NavAgentState::Idle) {
        // Restart patrol
        agent_system->set_destination(world, entity_id,
            behavior.patrol_points[behavior.patrol_index]);
    } else if (agent.state == NavAgentState::Failed) {
        // Skip to next point if current is unreachable
        if (behavior.patrol_loop) {
            behavior.patrol_index = (behavior.patrol_index + 1) %
                                    behavior.patrol_points.size();
        } else {
            behavior.patrol_forward = !behavior.patrol_forward;
        }
        behavior.wait_timer = 0.5f;  // Brief delay before retry
    }
}

void update_follow(scene::World& world, scene::Entity entity,
                   NavBehaviorComponent& behavior, NavAgentComponent& agent, float dt) {
    auto* agent_system = get_agent_system();
    if (!agent_system) return;

    // Check if target exists
    auto target_entity = static_cast<entt::entity>(behavior.follow_target);
    if (!world.valid(target_entity)) {
        return;
    }

    auto* target_transform = world.try_get<scene::LocalTransform>(target_entity);
    if (!target_transform) {
        return;
    }

    uint32_t entity_id = static_cast<uint32_t>(entity);
    Vec3 target_pos = target_transform->position;

    // Get current position
    auto* my_transform = world.try_get<scene::LocalTransform>(entity);
    if (!my_transform) return;

    Vec3 my_pos = my_transform->position;
    float distance_to_target = glm::length(target_pos - my_pos);

    // Update follow timer
    behavior.follow_timer += dt;

    // Update path if enough time has passed or we're idle and far from target
    bool should_update = behavior.follow_timer >= behavior.follow_update_rate;
    bool too_far = distance_to_target > behavior.follow_distance * 2.0f;
    bool is_idle = agent.state == NavAgentState::Idle || agent.state == NavAgentState::Arrived;

    if (should_update || (is_idle && too_far)) {
        behavior.follow_timer = 0.0f;

        // Only move if we're too far from target
        if (distance_to_target > behavior.follow_distance) {
            // Move towards target, stopping at follow_distance
            Vec3 direction = glm::normalize(target_pos - my_pos);
            Vec3 destination = target_pos - direction * behavior.follow_distance;
            agent_system->set_destination(world, entity_id, destination);
        } else if (agent.state == NavAgentState::Moving) {
            // Close enough, stop following
            agent_system->stop(world, entity_id);
        }
    }
}

void update_flee(scene::World& world, scene::Entity entity,
                 NavBehaviorComponent& behavior, NavAgentComponent& agent, float dt) {
    auto* agent_system = get_agent_system();
    auto* pathfinder = get_pathfinder();
    if (!agent_system || !pathfinder) return;

    uint32_t entity_id = static_cast<uint32_t>(entity);

    // Get current position
    auto* transform = world.try_get<scene::LocalTransform>(entity);
    if (!transform) return;

    Vec3 my_pos = transform->position;
    float distance_from_threat = glm::length(my_pos - behavior.flee_from);

    // If we're far enough, stop fleeing
    if (distance_from_threat >= behavior.flee_distance) {
        if (agent.state == NavAgentState::Moving) {
            agent_system->stop(world, entity_id);
        }
        return;
    }

    // If not moving or failed, find new flee position
    if (agent.state == NavAgentState::Idle || agent.state == NavAgentState::Arrived ||
        agent.state == NavAgentState::Failed) {

        // Calculate direction away from threat
        Vec3 flee_direction = my_pos - behavior.flee_from;
        if (glm::length(flee_direction) < 0.01f) {
            // We're at the threat position, pick random direction
            flee_direction = Vec3{random_range(-1.0f, 1.0f), 0.0f, random_range(-1.0f, 1.0f)};
        }
        flee_direction = glm::normalize(flee_direction);

        // Try to find a point in the flee direction
        Vec3 flee_target = my_pos + flee_direction * behavior.flee_distance;
        auto result = pathfinder->find_nearest_point(flee_target, behavior.flee_distance);

        if (result.valid) {
            agent_system->set_destination(world, entity_id, result.point);
        } else {
            // Try random point around us
            result = pathfinder->find_random_point_around(my_pos, behavior.flee_distance * 0.5f);
            if (result.valid) {
                // Check if it's actually away from threat
                if (glm::length(result.point - behavior.flee_from) > distance_from_threat) {
                    agent_system->set_destination(world, entity_id, result.point);
                }
            }
        }
    }
}

} // anonymous namespace

void navigation_behavior_system(scene::World& world, double dt) {
    if (!navigation_is_initialized()) {
        return;
    }

    float fdt = static_cast<float>(dt);

    auto view = world.view<NavBehaviorComponent, NavAgentComponent>();

    for (auto entity : view) {
        auto& behavior = view.get<NavBehaviorComponent>(entity);
        auto& agent = view.get<NavAgentComponent>(entity);

        if (!behavior.enabled || behavior.type == NavBehaviorType::None) {
            continue;
        }

        switch (behavior.type) {
            case NavBehaviorType::Wander:
                update_wander(world, entity, behavior, agent, fdt);
                break;
            case NavBehaviorType::Patrol:
                update_patrol(world, entity, behavior, agent, fdt);
                break;
            case NavBehaviorType::Follow:
                update_follow(world, entity, behavior, agent, fdt);
                break;
            case NavBehaviorType::Flee:
                update_flee(world, entity, behavior, agent, fdt);
                break;
            default:
                break;
        }
    }
}

} // namespace engine::navigation
