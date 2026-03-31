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

// Persistent storage to avoid per-frame heap allocations
struct TransformCache {
    std::vector<TransformEntity> entities;
    size_t last_count = 0;
};

TransformCache& get_transform_cache() {
    static TransformCache cache;
    return cache;
}

} // namespace

// Transform system - computes world matrices from local transforms and hierarchy
void transform_system(World& world, double /*dt*/) {
    auto& registry = world.registry();
    auto& cache = get_transform_cache();
    auto& entities = cache.entities;

    // Rebuild the sorted list
    entities.clear();

    auto view = registry.view<LocalTransform, WorldTransform>();
    for (auto entity : view) {
        uint32_t depth = 0;
        if (auto* h = registry.try_get<Hierarchy>(entity)) {
            depth = h->depth;
        }
        entities.push_back({entity, depth});
    }

    // Re-sort when entity count changed (hierarchy depth changes are picked up
    // each frame since we re-read depth above; the sort itself is fast for
    // nearly-sorted data which is the common case)
    std::sort(entities.begin(), entities.end(),
        [](const TransformEntity& a, const TransformEntity& b) {
            return a.depth < b.depth;
        });

    cache.last_count = entities.size();

    // Update transforms in order (parents before children)
    for (const auto& te : entities) {
        auto& local = registry.get<LocalTransform>(te.entity);
        auto& world_tf = registry.get<WorldTransform>(te.entity);

        // Store previous transform for interpolation
        if (auto* prev = registry.try_get<PreviousTransform>(te.entity)) {
            prev->matrix = world_tf.matrix;
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
        // Decompose previous transform
        Vec3 prev_pos{prev.matrix[3]};
        Vec3 prev_scale{
            glm::length(Vec3{prev.matrix[0]}),
            glm::length(Vec3{prev.matrix[1]}),
            glm::length(Vec3{prev.matrix[2]})
        };
        Mat3 prev_rot_mat{
            Vec3{prev.matrix[0]} / prev_scale.x,
            Vec3{prev.matrix[1]} / prev_scale.y,
            Vec3{prev.matrix[2]} / prev_scale.z
        };
        Quat prev_rot = glm::quat_cast(prev_rot_mat);

        // Decompose current transform
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

        // Interpolate components separately
        Vec3 pos = glm::mix(prev_pos, cur_pos, a);
        Quat rot = glm::slerp(prev_rot, cur_rot, a);
        Vec3 scl = glm::mix(prev_scale, cur_scale, a);

        // Recompose matrix
        Mat4 result{1.0f};
        result = glm::translate(result, pos);
        result = result * glm::mat4_cast(rot);
        result = glm::scale(result, scl);
        world_tf.matrix = result;
    }
}

} // namespace engine::scene
