#include <engine/script/bindings.hpp>
#include <engine/script/script_context.hpp>
#include <engine/navigation/navigation_systems.hpp>
#include <engine/navigation/nav_agent.hpp>
#include <engine/navigation/pathfinder.hpp>
#include <engine/scene/world.hpp>
#include <engine/core/log.hpp>

namespace engine::script {

void register_navigation_bindings(sol::state& lua) {
    using namespace engine::navigation;
    using namespace engine::core;

    // PathResult type
    lua.new_usertype<PathResult>("PathResult",
        sol::constructors<>(),
        "success", sol::readonly(&PathResult::success),
        "partial", sol::readonly(&PathResult::partial),
        "path", sol::readonly(&PathResult::path),
        "empty", &PathResult::empty,
        "size", &PathResult::size,
        "total_distance", &PathResult::total_distance
    );

    // NavPointResult type
    lua.new_usertype<NavPointResult>("NavPointResult",
        sol::constructors<>(),
        "point", sol::readonly(&NavPointResult::point),
        "valid", sol::readonly(&NavPointResult::valid)
    );

    // NavAgentState enum values
    lua["NavAgentState"] = lua.create_table_with(
        "Idle", static_cast<int>(NavAgentState::Idle),
        "Moving", static_cast<int>(NavAgentState::Moving),
        "Waiting", static_cast<int>(NavAgentState::Waiting),
        "Arrived", static_cast<int>(NavAgentState::Arrived),
        "Failed", static_cast<int>(NavAgentState::Failed)
    );

    // Create Nav table
    auto nav = lua.create_named_table("Nav");

    // --- Agent Control ---

    // Set destination for an entity with NavAgentComponent
    nav.set_function("set_destination", [](uint32_t entity_id, const Vec3& target) {
        auto* scene_world = get_current_script_world();
        auto* agent_system = get_agent_system();
        if (!scene_world || !agent_system) {
            core::log(core::LogLevel::Warn, "Nav.set_destination called without navigation context");
            return;
        }

        agent_system->set_destination(*scene_world, entity_id, target);
    });

    // Stop agent movement
    nav.set_function("stop", [](uint32_t entity_id) {
        auto* scene_world = get_current_script_world();
        auto* agent_system = get_agent_system();
        if (!scene_world || !agent_system) {
            return;
        }

        agent_system->stop(*scene_world, entity_id);
    });

    // Warp agent to position (no pathfinding)
    nav.set_function("warp", [](uint32_t entity_id, const Vec3& position) {
        auto* scene_world = get_current_script_world();
        auto* agent_system = get_agent_system();
        if (!scene_world || !agent_system) {
            return;
        }

        agent_system->warp(*scene_world, entity_id, position);
    });

    // Check if agent has arrived at destination
    nav.set_function("has_arrived", [](uint32_t entity_id) -> bool {
        auto* scene_world = get_current_script_world();
        auto* agent_system = get_agent_system();
        if (!scene_world || !agent_system) {
            return false;
        }

        return agent_system->has_arrived(*scene_world, entity_id);
    });

    // Get remaining distance to target
    nav.set_function("get_remaining_distance", [](uint32_t entity_id) -> float {
        auto* scene_world = get_current_script_world();
        auto* agent_system = get_agent_system();
        if (!scene_world || !agent_system) {
            return 0.0f;
        }

        return agent_system->get_remaining_distance(*scene_world, entity_id);
    });

    // Get agent state
    nav.set_function("get_state", [](uint32_t entity_id) -> int {
        auto* scene_world = get_current_script_world();
        if (!scene_world) {
            return static_cast<int>(NavAgentState::Idle);
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (!scene_world->registry().valid(entity)) {
            return static_cast<int>(NavAgentState::Idle);
        }

        auto* agent = scene_world->try_get<NavAgentComponent>(entity);
        if (!agent) {
            return static_cast<int>(NavAgentState::Idle);
        }

        return static_cast<int>(agent->state);
    });

    // Get agent velocity
    nav.set_function("get_velocity", [](uint32_t entity_id) -> Vec3 {
        auto* scene_world = get_current_script_world();
        if (!scene_world) {
            return Vec3{0.0f};
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (!scene_world->registry().valid(entity)) {
            return Vec3{0.0f};
        }

        auto* agent = scene_world->try_get<NavAgentComponent>(entity);
        if (!agent) {
            return Vec3{0.0f};
        }

        return agent->velocity;
    });

    // Set agent speed
    nav.set_function("set_speed", [](uint32_t entity_id, float speed) {
        auto* scene_world = get_current_script_world();
        if (!scene_world) {
            return;
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (!scene_world->registry().valid(entity)) {
            return;
        }

        auto* agent = scene_world->try_get<NavAgentComponent>(entity);
        if (agent) {
            agent->speed = speed;
        }
    });

    // --- Pathfinding Queries ---

    // Find path between two points
    nav.set_function("find_path", [](const Vec3& start, const Vec3& end) -> PathResult {
        auto* pathfinder = get_pathfinder();
        if (!pathfinder) {
            core::log(core::LogLevel::Warn, "Nav.find_path called without navigation initialized");
            return PathResult{};
        }

        return pathfinder->find_path(start, end);
    });

    // Check if point is on navmesh
    nav.set_function("is_on_navmesh", [](const Vec3& point, sol::optional<float> tolerance) -> bool {
        auto* pathfinder = get_pathfinder();
        if (!pathfinder) {
            return false;
        }

        return pathfinder->is_point_on_navmesh(point, tolerance.value_or(0.5f));
    });

    // Find nearest point on navmesh
    nav.set_function("find_nearest_point", [](const Vec3& point,
                                              sol::optional<float> search_radius) -> NavPointResult {
        auto* pathfinder = get_pathfinder();
        if (!pathfinder) {
            return NavPointResult{};
        }

        return pathfinder->find_nearest_point(point, search_radius.value_or(5.0f));
    });

    // Find random point on navmesh
    nav.set_function("find_random_point", []() -> NavPointResult {
        auto* pathfinder = get_pathfinder();
        if (!pathfinder) {
            return NavPointResult{};
        }

        return pathfinder->find_random_point();
    });

    // Find random point within radius
    nav.set_function("find_random_point_around", [](const Vec3& center, float radius) -> NavPointResult {
        auto* pathfinder = get_pathfinder();
        if (!pathfinder) {
            return NavPointResult{};
        }

        return pathfinder->find_random_point_around(center, radius);
    });

    // Check if path between two points is clear
    nav.set_function("is_path_clear", [](const Vec3& start, const Vec3& end) -> bool {
        auto* pathfinder = get_pathfinder();
        if (!pathfinder) {
            return false;
        }

        return pathfinder->is_path_clear(start, end);
    });

    // Check if point is reachable from another
    nav.set_function("is_reachable", [](const Vec3& from, const Vec3& to) -> bool {
        auto* pathfinder = get_pathfinder();
        if (!pathfinder) {
            return false;
        }

        return pathfinder->is_reachable(from, to);
    });

    // Get path distance between two points
    nav.set_function("get_path_distance", [](const Vec3& start, const Vec3& end) -> float {
        auto* pathfinder = get_pathfinder();
        if (!pathfinder) {
            return -1.0f;
        }

        return pathfinder->get_path_distance(start, end);
    });

    // Project point to navmesh surface
    nav.set_function("project_point", [](const Vec3& point) -> NavPointResult {
        auto* pathfinder = get_pathfinder();
        if (!pathfinder) {
            return NavPointResult{};
        }

        return pathfinder->project_point(point);
    });

    // Check if navigation is initialized
    nav.set_function("is_initialized", &navigation_is_initialized);
}

} // namespace engine::script
