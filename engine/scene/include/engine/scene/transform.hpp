#pragma once

#include <engine/core/math.hpp>
#include <engine/scene/entity.hpp>
#include <functional>
#include <vector>

namespace engine::scene {

using core::Vec3;
using core::Vec4;
using core::Quat;
using core::Mat3;
using core::Mat4;

// Shared TRS helpers used by scene, physics, and rendering systems.
Mat4 compose_matrix_trs(const Vec3& position, const Quat& rotation, const Vec3& scale = Vec3{1.0f});
void decompose_matrix_trs(const Mat4& matrix, Vec3& position, Quat& rotation, Vec3& scale);

// Local space transform (relative to parent)
struct LocalTransform {
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};  // Identity quaternion
    Vec3 scale{1.0f};

    LocalTransform() = default;
    LocalTransform(const Vec3& pos) : position(pos) {}
    LocalTransform(const Vec3& pos, const Quat& rot) : position(pos), rotation(rot) {}
    LocalTransform(const Vec3& pos, const Quat& rot, const Vec3& scl)
        : position(pos), rotation(rot), scale(scl) {}

    // Compute the local transformation matrix
    Mat4 matrix() const {
        return compose_matrix_trs(position, rotation, scale);
    }

    // Get forward, right, up vectors
    Vec3 forward() const { return rotation * Vec3{0.0f, 0.0f, -1.0f}; }
    Vec3 right() const { return rotation * Vec3{1.0f, 0.0f, 0.0f}; }
    Vec3 up() const { return rotation * Vec3{0.0f, 1.0f, 0.0f}; }

    // Set rotation from euler angles (in radians)
    void set_euler(const Vec3& euler) {
        rotation = glm::quat(euler);
    }

    // Get euler angles (in radians)
    Vec3 euler() const {
        return glm::eulerAngles(rotation);
    }

    // Look at a target position
    void look_at(const Vec3& target, const Vec3& up_vec = Vec3{0.0f, 1.0f, 0.0f}) {
        Vec3 dir = glm::normalize(target - position);
        rotation = glm::quatLookAt(dir, up_vec);
    }
};

// World space transform (computed from hierarchy)
struct WorldTransform {
    Mat4 matrix{1.0f};

    WorldTransform() = default;
    explicit WorldTransform(const Mat4& m) : matrix(m) {}

    // Extract world position
    Vec3 position() const { return Vec3{matrix[3]}; }

    // Extract world scale (approximate, doesn't handle skew)
    Vec3 scale() const {
        Vec3 position_out{0.0f};
        Quat rotation_out{1.0f, 0.0f, 0.0f, 0.0f};
        Vec3 scale_out{1.0f};
        decompose_matrix_trs(matrix, position_out, rotation_out, scale_out);
        return scale_out;
    }

    // Extract world rotation
    Quat rotation() const {
        Vec3 position_out{0.0f};
        Quat rotation_out{1.0f, 0.0f, 0.0f, 0.0f};
        Vec3 scale_out{1.0f};
        decompose_matrix_trs(matrix, position_out, rotation_out, scale_out);
        return rotation_out;
    }
};

// Interpolated transform for smooth rendering between fixed updates.
// Written by interpolate_transforms(), read by the renderer.
// Keeps WorldTransform untouched so gameplay systems always see the true simulation state.
struct InterpolatedTransform {
    Mat4 matrix{1.0f};
};

// Previous frame transform for interpolation (stored as TRS to avoid decomposition)
struct PreviousTransform {
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f};

    PreviousTransform() = default;
    PreviousTransform(const Vec3& pos, const Quat& rot, const Vec3& scl)
        : position(pos), rotation(rot), scale(scl) {}

    // Construct from world matrix (for migration/compatibility)
    static PreviousTransform from_matrix(const Mat4& m) {
        PreviousTransform pt;
        decompose_matrix_trs(m, pt.position, pt.rotation, pt.scale);
        return pt;
    }
};

// Hierarchy links using intrusive linked list for efficient modification
// Kept POD-like for cache efficiency — children cache is stored externally
struct Hierarchy {
    Entity parent = NullEntity;
    Entity first_child = NullEntity;
    Entity next_sibling = NullEntity;
    Entity prev_sibling = NullEntity;
    uint32_t depth = 0;  // Used for sorting and update order
    bool children_dirty = true;

    Hierarchy() = default;
};

// Forward declarations of hierarchy functions
class World;

// World-space helpers for systems that author absolute motion while ECS stores local transforms.
void get_entity_world_pose(const World& world, Entity entity, const LocalTransform& local, Vec3& position, Quat& rotation);
void set_entity_world_pose(World& world, Entity entity, LocalTransform& local, const Vec3& position, const Quat& rotation);
void sync_world_transform(World& world, Entity entity, bool snap_previous = false);
void sync_world_transform_tree(World& world, Entity entity, bool snap_previous = false);

// Set parent of an entity, handles all linked list updates
void set_parent(World& world, Entity child, Entity parent);

// Set parent and place the child before a specific sibling (or append if null)
void set_parent(World& world, Entity child, Entity parent, Entity before_sibling);

// Remove parent relationship
void remove_parent(World& world, Entity child);

// Detach an entity from its current parent/root list without attaching elsewhere
void detach_from_hierarchy(World& world, Entity child);

// Get children of an entity (uses cache if valid)
const std::vector<Entity>& get_children(World& world, Entity parent);

// Iterate children without caching (direct traversal)
void iterate_children(World& world, Entity parent, std::function<void(Entity)> fn);

// Get root entities (entities with no parent)
const std::vector<Entity>& get_root_entities(World& world);

// Check if entity is ancestor of another
bool is_ancestor_of(World& world, Entity ancestor, Entity descendant);

// Reset the root entity list (call when clearing world)
void reset_roots(World& world);

// Check/clear whether hierarchy has changed since last transform update
bool is_hierarchy_dirty(World& world);
void clear_hierarchy_dirty(World& world);

// Systems for transform updates
void transform_system(World& world, double dt);
void interpolate_transforms(World& world, double alpha);

} // namespace engine::scene
