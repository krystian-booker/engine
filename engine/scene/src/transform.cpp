#include <engine/scene/transform.hpp>
#include <engine/scene/world.hpp>
#include <engine/core/job_system.hpp>
#include <algorithm>
#include <vector>

namespace engine::scene {

namespace {

struct TransformEntity {
    Entity entity;
    uint32_t depth;
};

// Per-world persistent storage to avoid per-frame heap allocations
struct TransformCache {
    std::vector<TransformEntity> entities;
    size_t last_count = 0;
};

TransformCache& get_transform_cache(entt::registry& registry) {
    if (!registry.ctx().contains<TransformCache>()) {
        registry.ctx().emplace<TransformCache>();
    }
    return registry.ctx().get<TransformCache>();
}

} // namespace

// Transform system - computes world matrices from local transforms and hierarchy
void transform_system(World& world, double /*dt*/) {
    auto& registry = world.registry();
    auto& cache = get_transform_cache(registry);
    auto& entities = cache.entities;

    bool hierarchy_changed = is_hierarchy_dirty(world);

    // Only rebuild the sorted list when hierarchy has changed or entities were added/removed
    auto view = registry.view<LocalTransform, WorldTransform>();
    size_t current_count = view.size_hint();

    if (hierarchy_changed || current_count != cache.last_count) {
        entities.clear();

        for (auto entity : view) {
            uint32_t depth = 0;
            if (auto* h = registry.try_get<Hierarchy>(entity)) {
                depth = h->depth;
            }
            entities.push_back({entity, depth});
        }

        std::sort(entities.begin(), entities.end(),
            [](const TransformEntity& a, const TransformEntity& b) {
                return a.depth < b.depth;
            });

        cache.last_count = entities.size();
        clear_hierarchy_dirty(world);
    }

    // Update transforms in order (parents before children)
    for (const auto& te : entities) {
        auto& local = registry.get<LocalTransform>(te.entity);
        auto& world_tf = registry.get<WorldTransform>(te.entity);

        // Store previous transform for interpolation (as TRS from current world matrix)
        if (auto* prev = registry.try_get<PreviousTransform>(te.entity)) {
            prev->position = Vec3{world_tf.matrix[3]};
            prev->scale = Vec3{
                glm::length(Vec3{world_tf.matrix[0]}),
                glm::length(Vec3{world_tf.matrix[1]}),
                glm::length(Vec3{world_tf.matrix[2]})
            };
            Mat3 rot_mat{
                Vec3{world_tf.matrix[0]} / prev->scale.x,
                Vec3{world_tf.matrix[1]} / prev->scale.y,
                Vec3{world_tf.matrix[2]} / prev->scale.z
            };
            prev->rotation = glm::quat_cast(rot_mat);
        }

        // Compute local matrix
        Mat4 local_matrix = local.matrix();

        // If has parent, multiply by parent's world matrix
        if (auto* h = registry.try_get<Hierarchy>(te.entity)) {
            if (h->parent != NullEntity) {
                if (auto* parent_world = registry.try_get<WorldTransform>(h->parent)) {
                    world_tf.matrix = parent_world->matrix * local_matrix;
                    continue;
                }
            }
        }

        // No parent, world = local
        world_tf.matrix = local_matrix;
    }
}

// Interpolate transforms for smooth rendering between fixed updates
void interpolate_transforms(World& world, double alpha) {
    auto& registry = world.registry();
    float a = static_cast<float>(alpha);

    auto view = registry.view<WorldTransform, PreviousTransform>();
    for (auto [entity, world_tf, prev] : view.each()) {
        // Decompose current world transform
        Vec3 cur_pos{world_tf.matrix[3]};
        Vec3 cur_scale{
            glm::length(Vec3{world_tf.matrix[0]}),
            glm::length(Vec3{world_tf.matrix[1]}),
            glm::length(Vec3{world_tf.matrix[2]})
        };
        Mat3 cur_rot_mat{
            Vec3{world_tf.matrix[0]} / cur_scale.x,
            Vec3{world_tf.matrix[1]} / cur_scale.y,
            Vec3{world_tf.matrix[2]} / cur_scale.z
        };
        Quat cur_rot = glm::quat_cast(cur_rot_mat);

        // Previous transform is already TRS — no decomposition needed
        Vec3 pos = glm::mix(prev.position, cur_pos, a);
        Quat rot = glm::slerp(prev.rotation, cur_rot, a);
        Vec3 scl = glm::mix(prev.scale, cur_scale, a);

        // Recompose matrix
        Mat4 result{1.0f};
        result = glm::translate(result, pos);
        result = result * glm::mat4_cast(rot);
        result = glm::scale(result, scl);
        world_tf.matrix = result;
    }
}

} // namespace engine::scene
