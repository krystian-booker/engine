#pragma once

#include <engine/render/skeleton.hpp>
#include <engine/core/math.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace engine::render {

using namespace engine::core;

// Forward declarations
namespace physics { class PhysicsWorld; }

// IK solver types
enum class IKSolverType {
    TwoBone,        // Simple 2-bone IK (limbs)
    FABRIK,         // Forward and Backward Reaching IK (chains)
    CCD,            // Cyclic Coordinate Descent (chains)
    LookAt          // Single bone rotation toward target
};

// Bone rotation constraint
struct BoneConstraint {
    // Euler angle limits (degrees)
    Vec3 min_angles{-180.0f};
    Vec3 max_angles{180.0f};

    // Twist constraint (rotation around bone axis)
    Vec3 twist_axis{0.0f, 1.0f, 0.0f};
    float twist_min = -180.0f;
    float twist_max = 180.0f;

    // Stiffness (0 = fully flexible, 1 = completely stiff)
    float stiffness = 0.0f;

    // Apply constraints to a rotation
    Quat constrain(const Quat& rotation) const;
};

// IK chain definition
struct IKChain {
    std::vector<int32_t> bone_indices;  // From root to tip
    std::vector<float> bone_lengths;    // Cached lengths
    Vec3 target_position;
    Quat target_rotation{1, 0, 0, 0};
    bool use_target_rotation = false;
    float weight = 1.0f;
    int max_iterations = 10;
    float tolerance = 0.001f;

    // Per-bone constraints
    std::vector<BoneConstraint> constraints;

    // Cache bone lengths from skeleton
    void calculate_lengths(const Skeleton& skeleton);
    float get_total_length() const;
};

// Two-bone IK settings (for limbs like arms and legs)
struct TwoBoneIKSettings {
    int32_t root_bone = -1;     // Hip/shoulder
    int32_t mid_bone = -1;      // Knee/elbow
    int32_t end_bone = -1;      // Ankle/wrist

    Vec3 target_position;
    Quat target_rotation{1, 0, 0, 0};
    bool use_target_rotation = false;

    // Pole vector (hint for knee/elbow bend direction)
    Vec3 pole_vector{0.0f, 0.0f, 1.0f};
    bool use_pole_target = false;
    Vec3 pole_target;  // World position for pole

    // Softness at full extension (prevents snapping)
    float soft_limit = 0.0f;

    // Overall weight (0-1)
    float weight = 1.0f;
};

// Look-at IK settings
struct LookAtIKSettings {
    int32_t bone_index = -1;  // Usually head or spine bone
    Vec3 target;              // World position to look at

    // Axis definitions
    Vec3 forward_axis{0.0f, 0.0f, 1.0f};  // Local forward
    Vec3 up_axis{0.0f, 1.0f, 0.0f};       // Local up

    float weight = 1.0f;
    float clamp_weight = 0.5f;  // Reduce weight when target is behind

    // Angular limits (degrees)
    float max_horizontal_angle = 70.0f;
    float max_vertical_angle = 60.0f;

    // Additional bones to rotate (e.g., neck, spine)
    struct AffectedBone {
        int32_t bone_index;
        float weight;  // How much of the rotation this bone takes
    };
    std::vector<AffectedBone> affected_bones;
};

// Foot IK settings
struct FootIKSettings {
    // Leg bones
    TwoBoneIKSettings left_leg;
    TwoBoneIKSettings right_leg;

    // Foot bones for rotation
    int32_t left_foot_bone = -1;
    int32_t right_foot_bone = -1;

    // Raycast settings
    float ray_length = 1.5f;
    float ray_start_offset = 0.5f;  // Start ray above character
    float foot_height = 0.1f;       // Height of foot above ground
    float pelvis_offset_limit = 0.5f;  // Max hip adjustment

    // Smoothing
    float position_speed = 10.0f;
    float rotation_speed = 10.0f;

    // Ground detection
    uint16_t ground_layer_mask = 0xFFFF;

    // Pelvis bone for vertical adjustment
    int32_t pelvis_bone = -1;
};

// IK solver class
class IKSolver {
public:
    IKSolver() = default;

    // Solve a generic IK chain using FABRIK algorithm
    void solve_fabrik(
        IKChain& chain,
        std::vector<BoneTransform>& pose,
        const Skeleton& skeleton
    );

    // Solve using CCD algorithm
    void solve_ccd(
        IKChain& chain,
        std::vector<BoneTransform>& pose,
        const Skeleton& skeleton
    );

    // Solve two-bone IK (limbs)
    void solve_two_bone(
        const TwoBoneIKSettings& settings,
        std::vector<BoneTransform>& pose,
        const Skeleton& skeleton,
        const Mat4& world_transform = Mat4{1.0f}
    );

    // Solve look-at IK
    void solve_look_at(
        const LookAtIKSettings& settings,
        std::vector<BoneTransform>& pose,
        const Skeleton& skeleton,
        const Mat4& world_transform = Mat4{1.0f}
    );

private:
    // Helper to get world position of a bone
    Vec3 get_bone_world_position(
        int32_t bone_index,
        const std::vector<BoneTransform>& pose,
        const Skeleton& skeleton,
        const Mat4& world_transform
    ) const;

    // Helper to set bone world position (updates local transform)
    void set_bone_world_position(
        int32_t bone_index,
        const Vec3& world_position,
        std::vector<BoneTransform>& pose,
        const Skeleton& skeleton,
        const Mat4& world_transform
    );

    // Apply bone constraint
    void apply_constraint(BoneTransform& transform, const BoneConstraint& constraint);
};

// Raycast result for foot IK
struct FootIKRaycastResult {
    bool hit = false;
    Vec3 position;
    Vec3 normal;
    float distance = 0.0f;
};

// Foot IK processor
class FootIKProcessor {
public:
    FootIKProcessor() = default;

    void init(const FootIKSettings& settings);

    // Raycast callback type
    using RaycastCallback = std::function<FootIKRaycastResult(
        const Vec3& origin,
        const Vec3& direction,
        float max_distance,
        uint16_t layer_mask
    )>;

    void set_raycast_callback(RaycastCallback callback) { m_raycast = callback; }

    // Process foot IK for a character
    void process(
        const FootIKSettings& settings,
        std::vector<BoneTransform>& pose,
        const Skeleton& skeleton,
        const Mat4& world_transform,
        float dt
    );

    // Get current foot offsets (for debugging)
    float get_left_foot_offset() const { return m_left_foot_offset; }
    float get_right_foot_offset() const { return m_right_foot_offset; }
    float get_pelvis_offset() const { return m_pelvis_offset; }

private:
    FootIKRaycastResult raycast_foot(
        const Vec3& foot_position,
        const FootIKSettings& settings,
        const Mat4& world_transform
    );

    IKSolver m_solver;
    RaycastCallback m_raycast;

    // Smoothed values
    float m_left_foot_offset = 0.0f;
    float m_right_foot_offset = 0.0f;
    Quat m_left_foot_rotation{1, 0, 0, 0};
    Quat m_right_foot_rotation{1, 0, 0, 0};
    float m_pelvis_offset = 0.0f;

    // Target values (before smoothing)
    float m_left_foot_target = 0.0f;
    float m_right_foot_target = 0.0f;
    float m_pelvis_target = 0.0f;
};

// Hand IK settings (for grabbing, holding weapons, etc.)
struct HandIKSettings {
    TwoBoneIKSettings arm;
    int32_t hand_bone = -1;

    // Target for hand position and rotation
    Vec3 target_position;
    Quat target_rotation{1, 0, 0, 0};

    float weight = 1.0f;

    // Finger bones (optional, for grip adjustment)
    std::vector<int32_t> finger_bones;
    float grip_amount = 0.0f;  // 0 = open, 1 = closed
};

// ECS Component for IK
struct IKComponent {
    IKSolver solver;
    FootIKProcessor foot_ik;
    FootIKSettings foot_ik_settings;

    std::vector<LookAtIKSettings> look_at_targets;
    std::vector<TwoBoneIKSettings> two_bone_targets;
    std::vector<HandIKSettings> hand_targets;

    bool foot_ik_enabled = true;
    bool look_at_enabled = true;
    bool hand_ik_enabled = true;

    // Process all IK after animation
    void process(
        std::vector<BoneTransform>& pose,
        const Skeleton& skeleton,
        const Mat4& world_transform,
        float dt
    );
};

// Helper functions
namespace IKHelpers {

// Setup foot IK bones from common humanoid skeleton
void setup_foot_ik_humanoid(
    FootIKSettings& settings,
    const Skeleton& skeleton,
    const std::string& left_hip = "LeftUpLeg",
    const std::string& left_knee = "LeftLeg",
    const std::string& left_ankle = "LeftFoot",
    const std::string& right_hip = "RightUpLeg",
    const std::string& right_knee = "RightLeg",
    const std::string& right_ankle = "RightFoot",
    const std::string& pelvis = "Hips"
);

// Setup look-at IK for head with neck influence
void setup_look_at_humanoid(
    LookAtIKSettings& settings,
    const Skeleton& skeleton,
    const std::string& head = "Head",
    const std::string& neck = "Neck",
    float neck_weight = 0.3f
);

// Setup arm IK
void setup_arm_ik(
    TwoBoneIKSettings& settings,
    const Skeleton& skeleton,
    bool is_left,
    const std::string& shoulder = "Shoulder",
    const std::string& elbow = "Arm",
    const std::string& wrist = "Hand"
);

} // namespace IKHelpers

} // namespace engine::render
