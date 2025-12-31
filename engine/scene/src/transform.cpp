#include <engine/scene/transform.hpp>
#include <engine/scene/world.hpp>
#include <engine/core/job_system.hpp>
#include <algorithm>
#include <vector>

namespace engine::scene {

// Transform system - computes world matrices from local transforms and hierarchy
void transform_system(World& world, double /*dt*/) {
    auto& registry = world.registry();

    // Collect all entities with transforms and sort by hierarchy depth
    struct TransformEntity {
        Entity entity;
        uint32_t depth;
    };

    std::vector<TransformEntity> entities;
    entities.reserve(world.size());

    auto view = registry.view<LocalTransform, WorldTransform>();
    for (auto entity : view) {
        uint32_t depth = 0;
        if (auto* h = registry.try_get<Hierarchy>(entity)) {
            depth = h->depth;
        }
        entities.push_back({entity, depth});
    }

    // Sort by depth (parents before children)
    std::sort(entities.begin(), entities.end(),
        [](const TransformEntity& a, const TransformEntity& b) {
            return a.depth < b.depth;
        });

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

    auto view = registry.view<WorldTransform, PreviousTransform>();
    for (auto [entity, world_tf, prev] : view.each()) {
        // Linear interpolation of matrices
        // Note: This is a simple approach; for better results, decompose and
        // interpolate position/rotation/scale separately
        float a = static_cast<float>(alpha);
        for (int i = 0; i < 4; ++i) {
            world_tf.matrix[i] = glm::mix(prev.matrix[i], world_tf.matrix[i], a);
        }
    }
}

} // namespace engine::scene
