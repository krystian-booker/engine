#include <engine/quest/waypoint.hpp>
#include <engine/quest/quest_manager.hpp>
#include <engine/scene/transform.hpp>
#include <engine/core/log.hpp>
#include <algorithm>
#include <cmath>

namespace engine::quest {

namespace {

Vec3 get_entity_position(scene::World& world, scene::Entity entity) {
    auto* world_transform = world.try_get<scene::WorldTransform>(entity);
    if (world_transform) {
        return world_transform->get_position();
    }
    auto* local_transform = world.try_get<scene::LocalTransform>(entity);
    if (local_transform) {
        return local_transform->position;
    }
    return Vec3(0.0f);
}

} // anonymous namespace

// ============================================================================
// Singleton Instance
// ============================================================================

WaypointSystem& WaypointSystem::instance() {
    static WaypointSystem s_instance;
    return s_instance;
}

// ============================================================================
// Update
// ============================================================================

void WaypointSystem::update(scene::World& world, float dt) {
    m_animation_time += dt;

    auto view = world.view<WaypointComponent>();

    for (auto entity : view) {
        auto& waypoint = view.get<WaypointComponent>(entity);

        if (!waypoint.enabled) continue;

        // Update visibility based on quest state
        if (!waypoint.quest_id.empty()) {
            const Quest* quest = QuestManager::instance().get_quest(waypoint.quest_id);
            if (!quest || quest->state != QuestState::Active) {
                waypoint.visible = false;
                continue;
            }

            if (!waypoint.objective_id.empty()) {
                const Objective* obj = quest->find_objective(waypoint.objective_id);
                if (!obj || obj->state != ObjectiveState::Active) {
                    waypoint.visible = false;
                    continue;
                }
            }

            waypoint.visible = true;
        }

        // Check distance visibility
        if (waypoint.max_distance > 0.0f && m_player != scene::NullEntity) {
            Vec3 waypoint_pos = waypoint.position_override.value_or(
                get_entity_position(world, entity));
            Vec3 player_pos = get_entity_position(world, m_player);
            float distance = glm::length(waypoint_pos - player_pos);

            if (distance > waypoint.max_distance) {
                waypoint.visible = false;
            } else if (distance < waypoint.min_distance) {
                waypoint.visible = false;
            }
        }
    }
}

// ============================================================================
// Create Waypoints
// ============================================================================

scene::Entity WaypointSystem::create_waypoint(scene::World& world, const Vec3& position,
                                               WaypointType type, const std::string& label) {
    scene::Entity entity = world.create();

    auto& transform = world.emplace<scene::LocalTransform>(entity);
    transform.position = position;

    auto& waypoint = world.emplace<WaypointComponent>(entity);
    waypoint.type = type;
    waypoint.label = label;
    waypoint.position_override = position;

    // Set default colors based on type
    switch (type) {
        case WaypointType::Objective:
            waypoint.color = Vec4(1.0f, 0.84f, 0.0f, 1.0f);  // Gold
            break;
        case WaypointType::QuestGiver:
            waypoint.color = Vec4(1.0f, 1.0f, 0.0f, 1.0f);   // Yellow
            break;
        case WaypointType::QuestTurnIn:
            waypoint.color = Vec4(0.0f, 1.0f, 0.0f, 1.0f);   // Green
            break;
        case WaypointType::PointOfInterest:
            waypoint.color = Vec4(0.5f, 0.5f, 1.0f, 1.0f);   // Light blue
            break;
        default:
            waypoint.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);   // White
            break;
    }

    return entity;
}

scene::Entity WaypointSystem::create_objective_waypoint(scene::World& world,
                                                         const std::string& quest_id,
                                                         const std::string& objective_id,
                                                         const Vec3& position) {
    scene::Entity entity = create_waypoint(world, position, WaypointType::Objective);

    auto& waypoint = world.get<WaypointComponent>(entity);
    waypoint.quest_id = quest_id;
    waypoint.objective_id = objective_id;

    // Start invisible, will be enabled when objective is active
    waypoint.visible = false;

    return entity;
}

scene::Entity WaypointSystem::create_objective_waypoint(scene::World& world,
                                                         const std::string& quest_id,
                                                         const std::string& objective_id,
                                                         scene::Entity target) {
    scene::Entity entity = world.create();

    auto& waypoint = world.emplace<WaypointComponent>(entity);
    waypoint.type = WaypointType::Objective;
    waypoint.quest_id = quest_id;
    waypoint.objective_id = objective_id;
    waypoint.color = Vec4(1.0f, 0.84f, 0.0f, 1.0f);
    waypoint.visible = false;

    // Link to target entity's position
    // The rendering system should follow the target entity
    // For now, store it and let the render system handle it

    return entity;
}

// ============================================================================
// Waypoint Queries
// ============================================================================

std::vector<scene::Entity> WaypointSystem::get_visible_waypoints(scene::World& world,
                                                                   const Vec3& camera_pos) const {
    std::vector<scene::Entity> result;

    auto view = world.view<WaypointComponent>();
    for (auto entity : view) {
        const auto& waypoint = view.get<WaypointComponent>(entity);
        if (waypoint.enabled && waypoint.visible) {
            result.push_back(entity);
        }
    }

    // Sort by priority (higher first), then by distance (closer first)
    std::sort(result.begin(), result.end(),
        [&world, &camera_pos](scene::Entity a, scene::Entity b) {
            const auto& wa = world.get<WaypointComponent>(a);
            const auto& wb = world.get<WaypointComponent>(b);

            if (wa.priority != wb.priority) {
                return static_cast<int>(wa.priority) > static_cast<int>(wb.priority);
            }

            Vec3 pos_a = wa.position_override.value_or(get_entity_position(world, a));
            Vec3 pos_b = wb.position_override.value_or(get_entity_position(world, b));

            float dist_a = glm::length(pos_a - camera_pos);
            float dist_b = glm::length(pos_b - camera_pos);

            return dist_a < dist_b;
        });

    return result;
}

scene::Entity WaypointSystem::get_closest_waypoint(scene::World& world,
                                                    const Vec3& position,
                                                    WaypointType type) const {
    scene::Entity closest = scene::NullEntity;
    float closest_dist = std::numeric_limits<float>::max();

    auto view = world.view<WaypointComponent>();
    for (auto entity : view) {
        const auto& waypoint = view.get<WaypointComponent>(entity);

        if (!waypoint.enabled || !waypoint.visible) continue;
        if (waypoint.type != type) continue;

        Vec3 waypoint_pos = waypoint.position_override.value_or(
            get_entity_position(world, entity));
        float dist = glm::length(waypoint_pos - position);

        if (dist < closest_dist) {
            closest_dist = dist;
            closest = entity;
        }
    }

    return closest;
}

// ============================================================================
// Quest Integration
// ============================================================================

void WaypointSystem::update_quest_waypoints(scene::World& world, const std::string& quest_id) {
    auto view = world.view<WaypointComponent>();

    for (auto entity : view) {
        auto& waypoint = view.get<WaypointComponent>(entity);

        if (waypoint.quest_id != quest_id) continue;

        const Quest* quest = QuestManager::instance().get_quest(quest_id);
        if (!quest || quest->state != QuestState::Active) {
            waypoint.visible = false;
            continue;
        }

        if (waypoint.objective_id.empty()) {
            waypoint.visible = true;
        } else {
            const Objective* obj = quest->find_objective(waypoint.objective_id);
            waypoint.visible = obj && obj->state == ObjectiveState::Active;
        }
    }
}

void WaypointSystem::remove_quest_waypoints(scene::World& world, const std::string& quest_id) {
    auto view = world.view<WaypointComponent>();
    std::vector<scene::Entity> to_remove;

    for (auto entity : view) {
        const auto& waypoint = view.get<WaypointComponent>(entity);
        if (waypoint.quest_id == quest_id) {
            to_remove.push_back(entity);
        }
    }

    for (auto entity : to_remove) {
        world.destroy(entity);
    }
}

} // namespace engine::quest
