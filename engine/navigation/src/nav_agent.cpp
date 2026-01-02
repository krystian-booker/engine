#include <engine/navigation/nav_agent.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/components.hpp>
#include <engine/core/log.hpp>

#include <algorithm>

namespace engine::navigation {

NavAgentSystem::NavAgentSystem() = default;
NavAgentSystem::~NavAgentSystem() = default;

void NavAgentSystem::init(Pathfinder* pathfinder) {
    m_pathfinder = pathfinder;
    core::log(core::LogLevel::Info, "NavAgentSystem initialized");
}

void NavAgentSystem::shutdown() {
    m_pathfinder = nullptr;
}

void NavAgentSystem::set_destination(scene::World& world, uint32_t entity_id, const Vec3& target) {
    auto entity = static_cast<entt::entity>(entity_id);
    if (!world.registry().valid(entity)) {
        return;
    }

    auto* agent = world.registry().try_get<NavAgentComponent>(entity);
    if (!agent) {
        return;
    }

    // Get current position
    auto* transform = world.registry().try_get<scene::LocalTransform>(entity);
    if (!transform) {
        return;
    }

    Vec3 position = transform->position;

    agent->target = target;
    agent->has_target = true;
    agent->state = NavAgentState::Moving;

    // Calculate initial path
    calculate_path(*agent, position);
}

void NavAgentSystem::stop(scene::World& world, uint32_t entity_id) {
    auto entity = static_cast<entt::entity>(entity_id);
    if (!world.registry().valid(entity)) {
        return;
    }

    auto* agent = world.registry().try_get<NavAgentComponent>(entity);
    if (!agent) {
        return;
    }

    agent->has_target = false;
    agent->path.clear();
    agent->path_index = 0;
    agent->state = NavAgentState::Idle;
    agent->velocity = Vec3(0.0f);
}

void NavAgentSystem::warp(scene::World& world, uint32_t entity_id, const Vec3& position) {
    auto entity = static_cast<entt::entity>(entity_id);
    if (!world.registry().valid(entity)) {
        return;
    }

    auto* transform = world.registry().try_get<scene::LocalTransform>(entity);
    if (transform) {
        transform->position = position;
    }

    auto* agent = world.registry().try_get<NavAgentComponent>(entity);
    if (agent) {
        // Clear current path since we teleported
        agent->path.clear();
        agent->path_index = 0;
        agent->velocity = Vec3(0.0f);

        // Recalculate path if we have a target
        if (agent->has_target) {
            calculate_path(*agent, position);
        }
    }
}

void NavAgentSystem::update(scene::World& world, float dt) {
    if (!m_pathfinder || !m_pathfinder->is_initialized()) {
        return;
    }

    auto view = world.registry().view<NavAgentComponent, scene::LocalTransform>();

    for (auto entity : view) {
        auto& agent = view.get<NavAgentComponent>(entity);
        auto& transform = view.get<scene::LocalTransform>(entity);

        Vec3 position = transform.position;
        update_agent(agent, position, dt);
        transform.position = position;
    }
}

void NavAgentSystem::update_agent(NavAgentComponent& agent, Vec3& position, float dt) {
    if (!agent.has_target) {
        agent.state = NavAgentState::Idle;
        return;
    }

    // Update repath timer
    agent.time_since_repath += dt;

    // Check if we need to recalculate path
    if (agent.path.empty() && agent.auto_repath) {
        if (agent.time_since_repath >= agent.repath_interval) {
            calculate_path(agent, position);
        }
    }

    if (agent.path.empty()) {
        agent.state = NavAgentState::Failed;
        return;
    }

    // Follow path
    follow_path(agent, position, dt);
}

void NavAgentSystem::calculate_path(NavAgentComponent& agent, const Vec3& position) {
    if (!m_pathfinder) {
        agent.state = NavAgentState::Failed;
        return;
    }

    agent.time_since_repath = 0.0f;

    // Set custom area costs for this agent
    m_pathfinder->set_area_costs(agent.area_costs);

    PathResult result = m_pathfinder->find_path(position, agent.target);

    if (!result.success) {
        agent.path.clear();
        agent.path_index = 0;
        agent.state = NavAgentState::Failed;
        return;
    }

    agent.path = std::move(result.path);
    agent.path_index = 0;
    agent.path_distance = result.total_distance();

    // Smooth the path for better looking movement
    smooth_path(agent);

    agent.state = NavAgentState::Moving;
}

void NavAgentSystem::follow_path(NavAgentComponent& agent, Vec3& position, float dt) {
    if (agent.path_index >= agent.path.size()) {
        agent.state = NavAgentState::Arrived;
        agent.velocity = Vec3(0.0f);
        agent.current_speed = 0.0f;
        return;
    }

    // Get current waypoint
    Vec3 waypoint = agent.path[agent.path_index];

    // Calculate direction to waypoint
    Vec3 to_waypoint = waypoint - position;
    to_waypoint.y = 0.0f;  // Ignore vertical for now
    float distance = glm::length(to_waypoint);

    // Check if we've reached the waypoint
    if (distance < agent.corner_threshold) {
        agent.path_index++;

        // Check if we've reached the final waypoint
        if (agent.path_index >= agent.path.size()) {
            // Check if close enough to target
            float dist_to_target = glm::length(position - agent.target);
            if (dist_to_target < agent.stopping_distance) {
                agent.state = NavAgentState::Arrived;
                agent.velocity = Vec3(0.0f);
                agent.current_speed = 0.0f;
                agent.has_target = false;
                return;
            }
        }

        // Continue to next waypoint
        if (agent.path_index < agent.path.size()) {
            waypoint = agent.path[agent.path_index];
            to_waypoint = waypoint - position;
            to_waypoint.y = 0.0f;
            distance = glm::length(to_waypoint);
        } else {
            return;
        }
    }

    // Calculate desired velocity
    Vec3 desired_direction = glm::normalize(to_waypoint);

    // Calculate remaining distance to end
    float remaining_distance = distance;
    for (size_t i = agent.path_index + 1; i < agent.path.size(); ++i) {
        remaining_distance += glm::length(agent.path[i] - agent.path[i - 1]);
    }
    agent.path_distance = remaining_distance;

    // Calculate desired speed (slow down when approaching target)
    float desired_speed = agent.speed;
    if (agent.deceleration > 0.0f) {
        float stopping_dist = (agent.speed * agent.speed) / (2.0f * agent.deceleration);

        if (remaining_distance < stopping_dist) {
            desired_speed = std::sqrt(2.0f * agent.deceleration * remaining_distance);
            desired_speed = std::max(desired_speed, 0.5f);  // Minimum speed
        }
    }

    // Accelerate/decelerate towards desired speed
    float speed_diff = desired_speed - agent.current_speed;
    if (speed_diff > 0) {
        agent.current_speed += agent.acceleration * dt;
        agent.current_speed = std::min(agent.current_speed, desired_speed);
    } else {
        agent.current_speed -= agent.deceleration * dt;
        agent.current_speed = std::max(agent.current_speed, desired_speed);
    }

    // Apply velocity
    agent.velocity = desired_direction * agent.current_speed;

    // Move position
    Vec3 movement = agent.velocity * dt;
    position += movement;

    // Project to navmesh to stay on surface
    if (m_pathfinder) {
        NavPointResult projected = m_pathfinder->project_point(position);
        if (projected.valid) {
            position.y = projected.point.y;
        }
    }

    agent.state = NavAgentState::Moving;
}

void NavAgentSystem::smooth_path(NavAgentComponent& agent) {
    if (agent.path.size() < 3) {
        return;
    }

    // Simple path smoothing: remove intermediate waypoints if direct path is clear
    if (!m_pathfinder) return;

    std::vector<Vec3> smoothed;
    smoothed.push_back(agent.path.front());

    size_t current = 0;
    while (current < agent.path.size() - 1) {
        // Try to skip ahead as far as possible
        size_t farthest = current + 1;

        for (size_t i = agent.path.size() - 1; i > current + 1; --i) {
            if (m_pathfinder->is_path_clear(agent.path[current], agent.path[i])) {
                farthest = i;
                break;
            }
        }

        smoothed.push_back(agent.path[farthest]);
        current = farthest;
    }

    agent.path = std::move(smoothed);
}

bool NavAgentSystem::has_arrived(scene::World& world, uint32_t entity_id) const {
    auto entity = static_cast<entt::entity>(entity_id);
    if (!world.registry().valid(entity)) {
        return false;
    }

    auto* agent = world.registry().try_get<NavAgentComponent>(entity);
    if (!agent) {
        return false;
    }

    return agent->state == NavAgentState::Arrived;
}

float NavAgentSystem::get_remaining_distance(scene::World& world, uint32_t entity_id) const {
    auto entity = static_cast<entt::entity>(entity_id);
    if (!world.registry().valid(entity)) {
        return -1.0f;
    }

    auto* agent = world.registry().try_get<NavAgentComponent>(entity);
    if (!agent) {
        return -1.0f;
    }

    if (!agent->has_target) {
        return 0.0f;
    }

    return agent->path_distance;
}

void NavAgentSystem::set_max_agents(int max_agents) {
    m_max_agents = max_agents;
}

} // namespace engine::navigation
