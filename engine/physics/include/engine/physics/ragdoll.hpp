#pragma once

#include <engine/physics/physics_world.hpp>
#include <engine/core/math.hpp>
#include <engine/render/skeleton.hpp>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace engine::physics {

using namespace engine::core;

// Shape types for ragdoll bodies
enum class RagdollShapeType : uint8_t {
    Capsule,
    Box,
    Sphere
};

// Definition of a single ragdoll body (maps to a bone)
struct RagdollBodyDef {
    std::string bone_name;
    RagdollShapeType shape = RagdollShapeType::Capsule;
    Vec3 dimensions{0.1f, 0.3f, 0.1f};  // Half-extents or radius/length
    Vec3 offset{0.0f};                   // Offset from bone origin
    Quat rotation_offset{1, 0, 0, 0};    // Rotation offset from bone
    float mass = 5.0f;
    float friction = 0.5f;
    float restitution = 0.0f;
};

// Joint constraint types
enum class RagdollJointType : uint8_t {
    Fixed,      // No rotation allowed
    Hinge,      // Single axis rotation (elbow, knee)
    Cone,       // Cone-shaped limit (shoulder, hip)
    Twist       // Twist around axis (spine)
};

// Joint constraint between ragdoll bodies
struct RagdollJointDef {
    std::string body_a;  // Parent body (bone name)
    std::string body_b;  // Child body (bone name)

    RagdollJointType type = RagdollJointType::Cone;

    // Limits in radians
    float twist_min = -0.5f;
    float twist_max = 0.5f;
    float swing_limit_1 = 0.5f;  // Cone limit around axis 1
    float swing_limit_2 = 0.5f;  // Cone limit around axis 2

    // Hinge axis (for hinge joints)
    Vec3 hinge_axis{1.0f, 0.0f, 0.0f};

    // Joint position/orientation (relative to body_a)
    Vec3 local_anchor_a{0.0f};
    Vec3 local_anchor_b{0.0f};
};

// Complete ragdoll definition
struct RagdollDefinition {
    std::string name;
    std::vector<RagdollBodyDef> bodies;
    std::vector<RagdollJointDef> joints;

    // The root body (typically pelvis/hips)
    std::string root_body;

    // Load from file
    static RagdollDefinition load(const std::string& path);

    // Save to file
    bool save(const std::string& path) const;

    // Auto-generate from skeleton
    static RagdollDefinition generate_from_skeleton(const render::Skeleton& skeleton);

    // Validation
    bool is_valid() const;
};

// Ragdoll state
enum class RagdollState : uint8_t {
    Disabled,   // Not simulating
    Active,     // Full ragdoll physics
    Blending,   // Blending from ragdoll back to animation
    Powered     // Motor-driven ragdoll (active ragdoll)
};

// Ragdoll controller
class Ragdoll {
public:
    Ragdoll();
    ~Ragdoll();

    // Non-copyable
    Ragdoll(const Ragdoll&) = delete;
    Ragdoll& operator=(const Ragdoll&) = delete;

    // Movable
    Ragdoll(Ragdoll&&) noexcept;
    Ragdoll& operator=(Ragdoll&&) noexcept;

    // Initialization
    void init(PhysicsWorld& world, const RagdollDefinition& def,
              const render::Skeleton& skeleton);
    void shutdown();
    bool is_initialized() const;

    // State control
    void set_state(RagdollState state);
    RagdollState get_state() const;

    // Activate ragdoll physics
    void activate(const std::vector<render::BoneTransform>& current_pose,
                  const Vec3& initial_velocity = Vec3{0.0f});

    // Deactivate (return to animation control)
    void deactivate();

    // Blend from ragdoll back to animation over time
    void blend_to_animation(float duration = 0.5f);

    // Apply impulse to a specific body
    void apply_impulse(const std::string& bone_name, const Vec3& impulse, const Vec3& point);

    // Apply force to all bodies
    void apply_force(const Vec3& force);

    // Get current pose from ragdoll bodies (for rendering)
    void get_pose(std::vector<render::BoneTransform>& out_pose) const;

    // Update (call each frame when active or blending)
    void update(float dt, const std::vector<render::BoneTransform>* anim_pose = nullptr);

    // Partial ragdoll control
    void set_bone_kinematic(const std::string& bone_name, bool kinematic);
    void set_bones_kinematic_below(const std::string& bone_name, bool kinematic);

    // Motor-driven ragdoll (for powered ragdoll / active characters)
    void set_motor_targets(const std::vector<render::BoneTransform>& target_pose);
    void set_motor_strength(float strength);  // 0 = pure ragdoll, 1 = follow animation
    float get_motor_strength() const;

    // Position in world
    void set_position(const Vec3& pos);
    Vec3 get_position() const;

    void set_rotation(const Quat& rot);
    Quat get_rotation() const;

    // Get body for a specific bone
    PhysicsBodyId get_body(const std::string& bone_name) const;

    // Get all body IDs
    std::vector<PhysicsBodyId> get_all_bodies() const;

    // Layer control
    void set_collision_layer(uint16_t layer);
    uint16_t get_collision_layer() const;

    // Get definition
    const RagdollDefinition& get_definition() const { return m_definition; }

private:
    void create_bodies(PhysicsWorld& world, const render::Skeleton& skeleton);
    void create_joints(PhysicsWorld& world);
    void destroy_bodies();
    void destroy_joints();

    void update_blend(float dt, const std::vector<render::BoneTransform>& anim_pose);

    PhysicsWorld* m_world = nullptr;
    RagdollDefinition m_definition;
    const render::Skeleton* m_skeleton = nullptr;

    RagdollState m_state = RagdollState::Disabled;
    float m_motor_strength = 0.0f;

    // Blending
    float m_blend_time = 0.0f;
    float m_blend_duration = 0.5f;
    std::vector<render::BoneTransform> m_blend_start_pose;

    // Bone name to body ID mapping
    std::unordered_map<std::string, PhysicsBodyId> m_bone_to_body;
    std::unordered_map<std::string, int> m_bone_to_index;

    // Joint IDs (implementation specific)
    std::vector<uint32_t> m_joint_ids;

    uint16_t m_collision_layer = 0x0004;  // RAGDOLL layer
    bool m_initialized = false;
};

// ECS Component
struct RagdollComponent {
    std::shared_ptr<Ragdoll> ragdoll;
    RagdollDefinition definition;
    bool auto_activate_on_death = true;
    float activation_impulse_threshold = 100.0f;  // Impulse needed to activate
};

} // namespace engine::physics
