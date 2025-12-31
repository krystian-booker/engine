#include <engine/render/skeleton.hpp>
#include <engine/core/log.hpp>

namespace engine::render {

using namespace engine::core;

// ============================================================================
// Skeleton implementation
// ============================================================================

int32_t Skeleton::add_bone(const std::string& name, int32_t parent_index) {
    int32_t bone_index = static_cast<int32_t>(m_bones.size());

    Bone bone;
    bone.name = name;
    bone.parent_index = parent_index;

    // Add as child to parent
    if (parent_index >= 0 && parent_index < static_cast<int32_t>(m_bones.size())) {
        m_bones[parent_index].children.push_back(bone_index);
    }

    m_bones.push_back(bone);
    m_bone_name_map[name] = bone_index;

    return bone_index;
}

void Skeleton::set_bone_local_transform(int32_t index, const Mat4& transform) {
    if (index >= 0 && index < static_cast<int32_t>(m_bones.size())) {
        m_bones[index].local_transform = transform;
    }
}

void Skeleton::set_bone_inverse_bind_pose(int32_t index, const Mat4& inverse_bind) {
    if (index >= 0 && index < static_cast<int32_t>(m_bones.size())) {
        m_bones[index].inverse_bind_pose = inverse_bind;
    }
}

int32_t Skeleton::find_bone(const std::string& name) const {
    auto it = m_bone_name_map.find(name);
    if (it != m_bone_name_map.end()) {
        return it->second;
    }
    return -1;
}

void Skeleton::calculate_bone_matrices(
    const std::vector<BoneTransform>& pose,
    std::vector<Mat4>& out_matrices
) const {
    calculate_bone_matrices(pose, Mat4(1.0f), out_matrices);
}

void Skeleton::calculate_bone_matrices(
    const std::vector<BoneTransform>& pose,
    const Mat4& global_transform,
    std::vector<Mat4>& out_matrices
) const {
    size_t bone_count = m_bones.size();
    out_matrices.resize(bone_count);

    // Temporary array for world transforms
    std::vector<Mat4> world_transforms(bone_count);

    // Calculate world transforms in hierarchy order
    for (size_t i = 0; i < bone_count; ++i) {
        const Bone& bone = m_bones[i];

        // Get local transform from pose (or use identity if no pose data)
        Mat4 local_transform;
        if (i < pose.size()) {
            local_transform = pose[i].to_matrix();
        } else {
            local_transform = bone.local_transform;
        }

        // Combine with parent transform
        if (bone.parent_index >= 0) {
            world_transforms[i] = world_transforms[bone.parent_index] * local_transform;
        } else {
            world_transforms[i] = global_transform * local_transform;
        }

        // Final skinning matrix = world_transform * inverse_bind_pose
        out_matrices[i] = world_transforms[i] * bone.inverse_bind_pose;
    }
}

std::vector<BoneTransform> Skeleton::get_bind_pose() const {
    std::vector<BoneTransform> pose(m_bones.size());

    for (size_t i = 0; i < m_bones.size(); ++i) {
        const Mat4& local = m_bones[i].local_transform;

        // Decompose local transform into TRS
        Vec3 scale;
        Quat rotation;
        Vec3 translation;
        Vec3 skew;
        Vec4 perspective;

        glm::decompose(local, scale, rotation, translation, skew, perspective);

        pose[i].position = translation;
        pose[i].rotation = rotation;
        pose[i].scale = scale;
    }

    return pose;
}

// ============================================================================
// SkeletonInstance implementation
// ============================================================================

SkeletonInstance::SkeletonInstance(const Skeleton* skeleton)
    : m_skeleton(skeleton)
{
    if (skeleton) {
        m_current_pose = skeleton->get_bind_pose();
        m_bone_matrices.resize(skeleton->get_bone_count(), Mat4(1.0f));
    }
}

void SkeletonInstance::set_skeleton(const Skeleton* skeleton) {
    m_skeleton = skeleton;
    if (skeleton) {
        m_current_pose = skeleton->get_bind_pose();
        m_bone_matrices.resize(skeleton->get_bone_count(), Mat4(1.0f));
    } else {
        m_current_pose.clear();
        m_bone_matrices.clear();
    }
    m_matrices_dirty = true;
}

void SkeletonInstance::set_bone_transform(int32_t bone_index, const BoneTransform& transform) {
    if (bone_index >= 0 && bone_index < static_cast<int32_t>(m_current_pose.size())) {
        m_current_pose[bone_index] = transform;
        m_matrices_dirty = true;
    }
}

void SkeletonInstance::set_bone_transform(const std::string& bone_name, const BoneTransform& transform) {
    if (m_skeleton) {
        int32_t index = m_skeleton->find_bone(bone_name);
        set_bone_transform(index, transform);
    }
}

void SkeletonInstance::reset_to_bind_pose() {
    if (m_skeleton) {
        m_current_pose = m_skeleton->get_bind_pose();
        m_matrices_dirty = true;
    }
}

const std::vector<Mat4>& SkeletonInstance::calculate_matrices() {
    return calculate_matrices(Mat4(1.0f));
}

const std::vector<Mat4>& SkeletonInstance::calculate_matrices(const Mat4& global_transform) {
    if (m_skeleton && m_matrices_dirty) {
        m_skeleton->calculate_bone_matrices(m_current_pose, global_transform, m_bone_matrices);
        m_matrices_dirty = false;
    }
    return m_bone_matrices;
}

} // namespace engine::render
