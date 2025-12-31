#pragma once

#include <engine/core/math.hpp>
#include <engine/scene/entity.hpp>
#include <vector>

namespace engine::scene {

using namespace engine::core;

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
        Mat4 result{1.0f};
        result = glm::translate(result, position);
        result = result * glm::mat4_cast(rotation);
        result = glm::scale(result, scale);
        return result;
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
        return Vec3{
            glm::length(Vec3{matrix[0]}),
            glm::length(Vec3{matrix[1]}),
            glm::length(Vec3{matrix[2]})
        };
    }

    // Extract world rotation
    Quat rotation() const {
        Vec3 s = scale();
        Mat3 rot_mat{
            Vec3{matrix[0]} / s.x,
            Vec3{matrix[1]} / s.y,
            Vec3{matrix[2]} / s.z
        };
        return glm::quat_cast(rot_mat);
    }
};

// Previous frame transform for interpolation
struct PreviousTransform {
    Mat4 matrix{1.0f};

    PreviousTransform() = default;
    explicit PreviousTransform(const Mat4& m) : matrix(m) {}
};

// Hierarchy links using intrusive linked list for efficient modification
struct Hierarchy {
    Entity parent = NullEntity;
    Entity first_child = NullEntity;
    Entity next_sibling = NullEntity;
    Entity prev_sibling = NullEntity;
    uint32_t depth = 0;  // Used for sorting and update order

    // Cached children list for iteration-heavy use cases
    mutable std::vector<Entity> cached_children;
    mutable bool children_dirty = true;

    Hierarchy() = default;
};

// Forward declarations of hierarchy functions
class World;

// Set parent of an entity, handles all linked list updates
void set_parent(World& world, Entity child, Entity parent);

// Remove parent relationship
void remove_parent(World& world, Entity child);

// Get children of an entity (uses cache if valid)
const std::vector<Entity>& get_children(World& world, Entity parent);

// Iterate children without caching (direct traversal)
void iterate_children(World& world, Entity parent, std::function<void(Entity)> fn);

// Get root entities (entities with no parent)
std::vector<Entity> get_root_entities(World& world);

// Check if entity is ancestor of another
bool is_ancestor_of(World& world, Entity ancestor, Entity descendant);

// Systems for transform updates
void transform_system(World& world, double dt);
void interpolate_transforms(World& world, double alpha);

} // namespace engine::scene
