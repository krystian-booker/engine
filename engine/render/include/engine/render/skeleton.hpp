#pragma once

#include <engine/core/math.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace engine::render {

using namespace engine::core;

// Maximum bones supported for GPU skinning
constexpr uint32_t MAX_BONES = 128;
constexpr uint32_t MAX_BONE_INFLUENCES = 4;  // Per vertex

// A single bone in the skeleton hierarchy
struct Bone {
    std::string name;
    int32_t parent_index = -1;           // -1 for root bones
    Mat4 inverse_bind_pose{1.0f};        // Inverse of the bind pose transform
    Mat4 local_transform{1.0f};          // Local transform relative to parent
    std::vector<int32_t> children;       // Child bone indices
};

// Transform for a bone (used for animation)
struct BoneTransform {
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};  // Identity quaternion
    Vec3 scale{1.0f};

    // Convert to matrix
    Mat4 to_matrix() const {
        Mat4 result = glm::translate(Mat4(1.0f), position);
        result = result * glm::mat4_cast(rotation);
        result = glm::scale(result, scale);
        return result;
    }

    // Interpolate between two transforms
    static BoneTransform lerp(const BoneTransform& a, const BoneTransform& b, float t) {
        BoneTransform result;
        result.position = glm::mix(a.position, b.position, t);
        result.rotation = glm::slerp(a.rotation, b.rotation, t);
        result.scale = glm::mix(a.scale, b.scale, t);
        return result;
    }
};

// Skeleton definition - shared by all instances using this skeleton
class Skeleton {
public:
    Skeleton() = default;
    ~Skeleton() = default;

    // Add a bone to the skeleton
    int32_t add_bone(const std::string& name, int32_t parent_index = -1);

    // Set bone transforms
    void set_bone_local_transform(int32_t index, const Mat4& transform);
    void set_bone_inverse_bind_pose(int32_t index, const Mat4& inverse_bind);

    // Get bone information
    int32_t get_bone_count() const { return static_cast<int32_t>(m_bones.size()); }
    int32_t find_bone(const std::string& name) const;
    const Bone& get_bone(int32_t index) const { return m_bones[index]; }
    Bone& get_bone(int32_t index) { return m_bones[index]; }

    // Get all bones
    const std::vector<Bone>& get_bones() const { return m_bones; }

    // Calculate final bone matrices from current pose
    void calculate_bone_matrices(
        const std::vector<BoneTransform>& pose,
        std::vector<Mat4>& out_matrices
    ) const;

    // Calculate bone matrices with global transform applied
    void calculate_bone_matrices(
        const std::vector<BoneTransform>& pose,
        const Mat4& global_transform,
        std::vector<Mat4>& out_matrices
    ) const;

    // Get bind pose (rest pose)
    std::vector<BoneTransform> get_bind_pose() const;

private:
    std::vector<Bone> m_bones;
    std::unordered_map<std::string, int32_t> m_bone_name_map;
};

// Skeleton instance - runtime state for an animated character
class SkeletonInstance {
public:
    SkeletonInstance() = default;
    explicit SkeletonInstance(const Skeleton* skeleton);

    // Set the skeleton this instance uses
    void set_skeleton(const Skeleton* skeleton);
    const Skeleton* get_skeleton() const { return m_skeleton; }

    // Get/set current pose
    const std::vector<BoneTransform>& get_pose() const { return m_current_pose; }
    std::vector<BoneTransform>& get_pose() { return m_current_pose; }

    // Set a specific bone's transform
    void set_bone_transform(int32_t bone_index, const BoneTransform& transform);
    void set_bone_transform(const std::string& bone_name, const BoneTransform& transform);

    // Reset to bind pose
    void reset_to_bind_pose();

    // Calculate final matrices for GPU upload
    const std::vector<Mat4>& calculate_matrices();
    const std::vector<Mat4>& calculate_matrices(const Mat4& global_transform);

    // Get the calculated matrices (must call calculate_matrices first)
    const std::vector<Mat4>& get_bone_matrices() const { return m_bone_matrices; }

private:
    const Skeleton* m_skeleton = nullptr;
    std::vector<BoneTransform> m_current_pose;
    std::vector<Mat4> m_bone_matrices;
    bool m_matrices_dirty = true;
};

// Vertex skinning data (per-vertex)
struct SkinningData {
    IVec4 bone_indices{0};   // Up to 4 bone influences
    Vec4 bone_weights{0.0f}; // Corresponding weights (should sum to 1.0)
};

} // namespace engine::render
