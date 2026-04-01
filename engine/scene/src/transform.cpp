#include <engine/scene/transform.hpp>
#include <engine/scene/world.hpp>
#include <engine/core/job_system.hpp>
#include <algorithm>
#include <vector>

namespace engine::scene {

namespace {
constexpr float kScaleEpsilon = 1e-6f;
constexpr size_t kMaxTransformHierarchyDepth = 1024;

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

Mat4 compute_world_matrix_recursive(const World& world, Entity entity, const LocalTransform& local, size_t depth = 0) {
    const Mat4 local_matrix = local.matrix();

    if (depth >= kMaxTransformHierarchyDepth) {
        return local_matrix;
    }

    const auto* hierarchy = world.try_get<Hierarchy>(entity);
    if (!hierarchy || hierarchy->parent == NullEntity) {
        return local_matrix;
    }

    const Entity parent = hierarchy->parent;
    if (const auto* parent_local = world.try_get<LocalTransform>(parent)) {
        return compute_world_matrix_recursive(world, parent, *parent_local, depth + 1) * local_matrix;
    }

    if (const auto* parent_world = world.try_get<WorldTransform>(parent)) {
        return parent_world->matrix * local_matrix;
    }

    return local_matrix;
}

void write_runtime_transform_state(entt::registry& registry, Entity entity, const Mat4& world_matrix, bool snap_previous) {
    registry.emplace_or_replace<WorldTransform>(entity, world_matrix);
    registry.emplace_or_replace<InterpolatedTransform>(entity, world_matrix);

    if (!snap_previous) {
        return;
    }

    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f};
    decompose_matrix_trs(world_matrix, position, rotation, scale);
    registry.emplace_or_replace<PreviousTransform>(entity, position, rotation, scale);
}

bool transform_cache_has_stale_entities(const entt::registry& registry, const std::vector<TransformEntity>& entities) {
    for (const auto& te : entities) {
        if (!registry.valid(te.entity) || !registry.all_of<LocalTransform, WorldTransform>(te.entity)) {
            return true;
        }
    }
    return false;
}

} // namespace

Mat4 compose_matrix_trs(const Vec3& position, const Quat& rotation, const Vec3& scale) {
    Mat4 result{1.0f};
    result = glm::translate(result, position);
    result = result * glm::mat4_cast(rotation);
    result = glm::scale(result, scale);
    return result;
}

void decompose_matrix_trs(const Mat4& matrix, Vec3& position, Quat& rotation, Vec3& scale) {
    position = Vec3{matrix[3]};

    const Vec3 axis_x{matrix[0]};
    const Vec3 axis_y{matrix[1]};
    const Vec3 axis_z{matrix[2]};

    scale = Vec3{
        glm::length(axis_x),
        glm::length(axis_y),
        glm::length(axis_z)
    };

    if (scale.x <= kScaleEpsilon || scale.y <= kScaleEpsilon || scale.z <= kScaleEpsilon) {
        rotation = Quat{1.0f, 0.0f, 0.0f, 0.0f};
        return;
    }

    Mat3 rotation_matrix{
        axis_x / scale.x,
        axis_y / scale.y,
        axis_z / scale.z
    };
    rotation = glm::normalize(glm::quat_cast(rotation_matrix));
}

void get_entity_world_pose(const World& world, Entity entity, const LocalTransform& local, Vec3& position, Quat& rotation) {
    const Mat4 world_matrix = compute_world_matrix_recursive(world, entity, local);
    Vec3 scale{1.0f};
    decompose_matrix_trs(world_matrix, position, rotation, scale);
}

void set_entity_world_pose(World& world, Entity entity, LocalTransform& local, const Vec3& position, const Quat& rotation) {
    if (const auto* hierarchy = world.try_get<Hierarchy>(entity); hierarchy && hierarchy->parent != NullEntity) {
        if (const auto* parent_local = world.try_get<LocalTransform>(hierarchy->parent)) {
            const Mat4 parent_world_matrix = compute_world_matrix_recursive(world, hierarchy->parent, *parent_local);
            const Mat4 desired_world = compose_matrix_trs(position, rotation, local.scale);
            const float determinant = glm::determinant(parent_world_matrix);
            if (std::abs(determinant) > kScaleEpsilon) {
                const Mat4 local_matrix = glm::inverse(parent_world_matrix) * desired_world;
                decompose_matrix_trs(local_matrix, local.position, local.rotation, local.scale);
                return;
            }
        } else if (const auto* parent_world = world.try_get<WorldTransform>(hierarchy->parent)) {
            const Mat4 desired_world = compose_matrix_trs(position, rotation, local.scale);
            const float determinant = glm::determinant(parent_world->matrix);
            if (std::abs(determinant) > kScaleEpsilon) {
                const Mat4 local_matrix = glm::inverse(parent_world->matrix) * desired_world;
                decompose_matrix_trs(local_matrix, local.position, local.rotation, local.scale);
                return;
            }
        }
    }

    local.position = position;
    local.rotation = rotation;
}

void sync_world_transform(World& world, Entity entity, bool snap_previous) {
    auto* local = world.try_get<LocalTransform>(entity);
    if (!local) {
        return;
    }

    write_runtime_transform_state(world.registry(), entity, compute_world_matrix_recursive(world, entity, *local), snap_previous);
}

void sync_world_transform_tree(World& world, Entity entity, bool snap_previous) {
    sync_world_transform(world, entity, snap_previous);

    auto* hierarchy = world.try_get<Hierarchy>(entity);
    if (!hierarchy) {
        return;
    }

    Entity child = hierarchy->first_child;
    size_t iterations = 0;
    while (child != NullEntity && iterations++ < kMaxTransformHierarchyDepth) {
        auto* child_h = world.try_get<Hierarchy>(child);
        const Entity next = child_h ? child_h->next_sibling : NullEntity;
        sync_world_transform_tree(world, child, snap_previous);
        child = next;
    }
}

// Transform system - computes world matrices from local transforms and hierarchy
void transform_system(World& world, double /*dt*/) {
    auto& registry = world.registry();
    auto& cache = get_transform_cache(registry);
    auto& entities = cache.entities;

    bool hierarchy_changed = is_hierarchy_dirty(world);

    // Only rebuild the sorted list when hierarchy has changed or entities were added/removed
    auto view = registry.view<LocalTransform, WorldTransform>();
    size_t current_count = view.size_hint();

    if (hierarchy_changed || current_count != cache.last_count ||
        entities.size() != current_count || transform_cache_has_stale_entities(registry, entities)) {
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
            decompose_matrix_trs(world_tf.matrix, prev->position, prev->rotation, prev->scale);
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

// Interpolate transforms for smooth rendering between fixed updates.
// Writes to InterpolatedTransform so WorldTransform retains the true simulation state.
void interpolate_transforms(World& world, double alpha) {
    auto& registry = world.registry();
    float a = static_cast<float>(alpha);

    auto view = registry.view<WorldTransform, PreviousTransform>();
    for (auto [entity, world_tf, prev] : view.each()) {
        Vec3 cur_pos{0.0f};
        Quat cur_rot{1.0f, 0.0f, 0.0f, 0.0f};
        Vec3 cur_scale{1.0f};
        decompose_matrix_trs(world_tf.matrix, cur_pos, cur_rot, cur_scale);

        // Previous transform is already TRS — no decomposition needed
        Vec3 pos = glm::mix(prev.position, cur_pos, a);
        Quat rot = glm::slerp(prev.rotation, cur_rot, a);
        Vec3 scl = glm::mix(prev.scale, cur_scale, a);

        // Recompose into InterpolatedTransform (renderer reads this)
        Mat4 result = compose_matrix_trs(pos, rot, scl);
        registry.emplace_or_replace<InterpolatedTransform>(entity, result);
    }
}

} // namespace engine::scene
