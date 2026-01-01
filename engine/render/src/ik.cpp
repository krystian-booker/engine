#include <engine/render/ik.hpp>
#include <algorithm>
#include <cmath>

namespace engine::render {

// BoneConstraint implementation

Quat BoneConstraint::constrain(const Quat& rotation) const {
    // Convert to euler angles
    Vec3 euler = glm::degrees(glm::eulerAngles(rotation));

    // Clamp to limits
    euler.x = std::clamp(euler.x, min_angles.x, max_angles.x);
    euler.y = std::clamp(euler.y, min_angles.y, max_angles.y);
    euler.z = std::clamp(euler.z, min_angles.z, max_angles.z);

    // Apply stiffness (blend toward identity)
    if (stiffness > 0.0f) {
        euler = euler * (1.0f - stiffness);
    }

    return Quat(glm::radians(euler));
}

// IKChain implementation

void IKChain::calculate_lengths(const Skeleton& skeleton) {
    bone_lengths.clear();

    if (bone_indices.size() < 2) return;

    auto bind_pose = skeleton.get_bind_pose();

    for (size_t i = 0; i < bone_indices.size() - 1; ++i) {
        int32_t current = bone_indices[i];
        int32_t next = bone_indices[i + 1];

        if (current >= 0 && next >= 0 &&
            current < static_cast<int32_t>(bind_pose.size()) &&
            next < static_cast<int32_t>(bind_pose.size())) {

            Vec3 pos_current = bind_pose[current].position;
            Vec3 pos_next = bind_pose[next].position;
            float length = glm::length(pos_next - pos_current);
            bone_lengths.push_back(length);
        }
    }
}

float IKChain::get_total_length() const {
    float total = 0.0f;
    for (float len : bone_lengths) {
        total += len;
    }
    return total;
}

// IKSolver implementation

void IKSolver::solve_fabrik(
    IKChain& chain,
    std::vector<BoneTransform>& pose,
    const Skeleton& skeleton
) {
    if (chain.bone_indices.size() < 2) return;

    // Get world positions of all bones in chain
    std::vector<Vec3> positions(chain.bone_indices.size());
    for (size_t i = 0; i < chain.bone_indices.size(); ++i) {
        int32_t bone_idx = chain.bone_indices[i];
        if (bone_idx >= 0 && bone_idx < static_cast<int32_t>(pose.size())) {
            positions[i] = pose[bone_idx].position;
        }
    }

    Vec3 root_pos = positions[0];
    float total_length = chain.get_total_length();

    // Check if target is reachable
    Vec3 to_target = chain.target_position - root_pos;
    float dist_to_target = glm::length(to_target);

    if (dist_to_target > total_length) {
        // Target is out of reach - stretch toward it
        Vec3 direction = glm::normalize(to_target);
        for (size_t i = 1; i < positions.size(); ++i) {
            float len = (i - 1 < chain.bone_lengths.size()) ? chain.bone_lengths[i - 1] : 1.0f;
            positions[i] = positions[i - 1] + direction * len;
        }
    } else {
        // FABRIK iterations
        for (int iter = 0; iter < chain.max_iterations; ++iter) {
            // Check if we're close enough
            Vec3 end_to_target = chain.target_position - positions.back();
            if (glm::length(end_to_target) < chain.tolerance) {
                break;
            }

            // Forward reaching (from end to root)
            positions.back() = chain.target_position;
            for (int i = static_cast<int>(positions.size()) - 2; i >= 0; --i) {
                Vec3 dir = glm::normalize(positions[i] - positions[i + 1]);
                float len = (i < static_cast<int>(chain.bone_lengths.size())) ? chain.bone_lengths[i] : 1.0f;
                positions[i] = positions[i + 1] + dir * len;
            }

            // Backward reaching (from root to end)
            positions[0] = root_pos;
            for (size_t i = 1; i < positions.size(); ++i) {
                Vec3 dir = glm::normalize(positions[i] - positions[i - 1]);
                float len = (i - 1 < chain.bone_lengths.size()) ? chain.bone_lengths[i - 1] : 1.0f;
                positions[i] = positions[i - 1] + dir * len;
            }
        }
    }

    // Apply weights and update pose
    for (size_t i = 0; i < chain.bone_indices.size(); ++i) {
        int32_t bone_idx = chain.bone_indices[i];
        if (bone_idx >= 0 && bone_idx < static_cast<int32_t>(pose.size())) {
            Vec3 original = pose[bone_idx].position;
            pose[bone_idx].position = glm::mix(original, positions[i], chain.weight);

            // Apply constraints if available
            if (i < chain.constraints.size()) {
                pose[bone_idx].rotation = chain.constraints[i].constrain(pose[bone_idx].rotation);
            }
        }
    }

    // Apply target rotation to end effector if requested
    if (chain.use_target_rotation && !chain.bone_indices.empty()) {
        int32_t end_bone = chain.bone_indices.back();
        if (end_bone >= 0 && end_bone < static_cast<int32_t>(pose.size())) {
            pose[end_bone].rotation = glm::slerp(
                pose[end_bone].rotation,
                chain.target_rotation,
                chain.weight
            );
        }
    }
}

void IKSolver::solve_ccd(
    IKChain& chain,
    std::vector<BoneTransform>& pose,
    const Skeleton& skeleton
) {
    if (chain.bone_indices.size() < 2) return;

    for (int iter = 0; iter < chain.max_iterations; ++iter) {
        // Get end effector position
        int32_t end_bone = chain.bone_indices.back();
        if (end_bone < 0 || end_bone >= static_cast<int32_t>(pose.size())) return;

        Vec3 end_pos = pose[end_bone].position;

        // Check if we're close enough
        if (glm::length(chain.target_position - end_pos) < chain.tolerance) {
            break;
        }

        // Iterate from end to root (excluding end effector)
        for (int i = static_cast<int>(chain.bone_indices.size()) - 2; i >= 0; --i) {
            int32_t bone_idx = chain.bone_indices[i];
            if (bone_idx < 0 || bone_idx >= static_cast<int32_t>(pose.size())) continue;

            Vec3 bone_pos = pose[bone_idx].position;

            // Recalculate end effector position
            end_pos = pose[end_bone].position;

            // Calculate rotation to align end effector with target
            Vec3 to_end = glm::normalize(end_pos - bone_pos);
            Vec3 to_target = glm::normalize(chain.target_position - bone_pos);

            float dot = glm::dot(to_end, to_target);
            if (dot < 0.9999f) {
                Vec3 axis = glm::cross(to_end, to_target);
                float axis_len = glm::length(axis);
                if (axis_len > 0.0001f) {
                    axis /= axis_len;
                    float angle = std::acos(std::clamp(dot, -1.0f, 1.0f));

                    Quat rotation = glm::angleAxis(angle, axis);
                    pose[bone_idx].rotation = rotation * pose[bone_idx].rotation;

                    // Apply constraints
                    if (i < static_cast<int>(chain.constraints.size())) {
                        pose[bone_idx].rotation = chain.constraints[i].constrain(pose[bone_idx].rotation);
                    }
                }
            }
        }
    }

    // Apply weight
    if (chain.weight < 1.0f) {
        // Would need to store original pose and blend - simplified here
    }
}

void IKSolver::solve_two_bone(
    const TwoBoneIKSettings& settings,
    std::vector<BoneTransform>& pose,
    const Skeleton& skeleton,
    const Mat4& world_transform
) {
    if (settings.root_bone < 0 || settings.mid_bone < 0 || settings.end_bone < 0) return;
    if (settings.root_bone >= static_cast<int32_t>(pose.size()) ||
        settings.mid_bone >= static_cast<int32_t>(pose.size()) ||
        settings.end_bone >= static_cast<int32_t>(pose.size())) return;

    // Get bone positions
    Vec3 root_pos = get_bone_world_position(settings.root_bone, pose, skeleton, world_transform);
    Vec3 mid_pos = get_bone_world_position(settings.mid_bone, pose, skeleton, world_transform);
    Vec3 end_pos = get_bone_world_position(settings.end_bone, pose, skeleton, world_transform);

    // Calculate bone lengths
    float upper_len = glm::length(mid_pos - root_pos);
    float lower_len = glm::length(end_pos - mid_pos);
    float total_len = upper_len + lower_len;

    // Target position (transform to world space if needed)
    Vec3 target = settings.target_position;

    // Calculate distance to target
    Vec3 to_target = target - root_pos;
    float target_dist = glm::length(to_target);

    // Clamp target distance to reachable range
    if (target_dist > total_len - 0.001f) {
        target_dist = total_len - 0.001f;
    }
    if (target_dist < std::abs(upper_len - lower_len) + 0.001f) {
        target_dist = std::abs(upper_len - lower_len) + 0.001f;
    }

    // Calculate mid joint angle using law of cosines
    float cos_angle = (upper_len * upper_len + lower_len * lower_len - target_dist * target_dist)
                    / (2.0f * upper_len * lower_len);
    cos_angle = std::clamp(cos_angle, -1.0f, 1.0f);
    float mid_angle = std::acos(cos_angle);

    // Calculate root angle
    float cos_root = (upper_len * upper_len + target_dist * target_dist - lower_len * lower_len)
                   / (2.0f * upper_len * target_dist);
    cos_root = std::clamp(cos_root, -1.0f, 1.0f);
    float root_angle = std::acos(cos_root);

    // Calculate the plane of the IK solve using pole vector
    Vec3 target_dir = glm::normalize(to_target);
    Vec3 pole_dir;
    if (settings.use_pole_target) {
        Vec3 to_pole = settings.pole_target - root_pos;
        pole_dir = glm::normalize(to_pole - target_dir * glm::dot(to_pole, target_dir));
    } else {
        pole_dir = settings.pole_vector;
        // Make sure pole vector is perpendicular to target direction
        pole_dir = glm::normalize(pole_dir - target_dir * glm::dot(pole_dir, target_dir));
    }

    // Calculate new mid position
    Vec3 plane_normal = glm::cross(target_dir, pole_dir);
    if (glm::length(plane_normal) < 0.001f) {
        plane_normal = Vec3{0.0f, 0.0f, 1.0f};
    }
    plane_normal = glm::normalize(plane_normal);

    // Rotate target direction by root angle around plane normal
    Quat root_rotation = glm::angleAxis(root_angle, plane_normal);
    Vec3 upper_dir = root_rotation * target_dir;
    Vec3 new_mid_pos = root_pos + upper_dir * upper_len;

    // Calculate new end position
    Vec3 mid_to_target = glm::normalize(target - new_mid_pos);
    Vec3 new_end_pos = new_mid_pos + mid_to_target * lower_len;

    // Apply weight
    if (settings.weight < 1.0f) {
        new_mid_pos = glm::mix(mid_pos, new_mid_pos, settings.weight);
        new_end_pos = glm::mix(end_pos, new_end_pos, settings.weight);
    }

    // Update bone rotations based on new positions
    // This is simplified - in a full implementation you'd update the local transforms

    // Calculate rotations needed
    Vec3 original_upper_dir = glm::normalize(mid_pos - root_pos);
    Vec3 new_upper_dir = glm::normalize(new_mid_pos - root_pos);
    if (glm::length(glm::cross(original_upper_dir, new_upper_dir)) > 0.001f) {
        Quat upper_rotation = glm::rotation(original_upper_dir, new_upper_dir);
        pose[settings.root_bone].rotation = upper_rotation * pose[settings.root_bone].rotation;
    }

    Vec3 original_lower_dir = glm::normalize(end_pos - mid_pos);
    Vec3 new_lower_dir = glm::normalize(new_end_pos - new_mid_pos);
    if (glm::length(glm::cross(original_lower_dir, new_lower_dir)) > 0.001f) {
        Quat lower_rotation = glm::rotation(original_lower_dir, new_lower_dir);
        pose[settings.mid_bone].rotation = lower_rotation * pose[settings.mid_bone].rotation;
    }

    // Apply target rotation to end effector
    if (settings.use_target_rotation) {
        pose[settings.end_bone].rotation = glm::slerp(
            pose[settings.end_bone].rotation,
            settings.target_rotation,
            settings.weight
        );
    }
}

void IKSolver::solve_look_at(
    const LookAtIKSettings& settings,
    std::vector<BoneTransform>& pose,
    const Skeleton& skeleton,
    const Mat4& world_transform
) {
    if (settings.bone_index < 0 || settings.bone_index >= static_cast<int32_t>(pose.size())) {
        return;
    }

    // Get bone world position
    Vec3 bone_pos = get_bone_world_position(settings.bone_index, pose, skeleton, world_transform);

    // Calculate direction to target
    Vec3 to_target = settings.target - bone_pos;
    float dist = glm::length(to_target);
    if (dist < 0.001f) return;
    to_target /= dist;

    // Get current forward direction in world space
    Mat4 bone_world = world_transform;
    // Simplified - would need full bone chain calculation
    Vec3 current_forward = glm::normalize(Vec3(bone_world * Vec4(settings.forward_axis, 0.0f)));
    Vec3 current_up = glm::normalize(Vec3(bone_world * Vec4(settings.up_axis, 0.0f)));

    // Calculate angle to target
    float dot = glm::dot(current_forward, to_target);

    // Check if target is behind (reduce weight)
    float effective_weight = settings.weight;
    if (dot < 0.0f) {
        effective_weight *= settings.clamp_weight;
    }

    // Calculate horizontal and vertical angles
    Vec3 horizontal_target = to_target;
    horizontal_target.y = 0.0f;
    if (glm::length(horizontal_target) > 0.001f) {
        horizontal_target = glm::normalize(horizontal_target);
    }

    float horizontal_angle = std::atan2(
        glm::dot(glm::cross(current_forward, horizontal_target), current_up),
        glm::dot(current_forward, horizontal_target)
    );

    float vertical_angle = std::asin(std::clamp(to_target.y, -1.0f, 1.0f));

    // Clamp angles
    horizontal_angle = std::clamp(
        horizontal_angle,
        -glm::radians(settings.max_horizontal_angle),
        glm::radians(settings.max_horizontal_angle)
    );
    vertical_angle = std::clamp(
        vertical_angle,
        -glm::radians(settings.max_vertical_angle),
        glm::radians(settings.max_vertical_angle)
    );

    // Create look-at rotation
    Quat horizontal_rot = glm::angleAxis(horizontal_angle * effective_weight, current_up);
    Vec3 right = glm::cross(current_forward, current_up);
    Quat vertical_rot = glm::angleAxis(vertical_angle * effective_weight, right);

    Quat look_rotation = horizontal_rot * vertical_rot;

    // Apply to main bone
    pose[settings.bone_index].rotation = look_rotation * pose[settings.bone_index].rotation;

    // Apply to affected bones (neck, spine, etc.)
    for (const auto& affected : settings.affected_bones) {
        if (affected.bone_index >= 0 && affected.bone_index < static_cast<int32_t>(pose.size())) {
            Quat partial_rot = glm::slerp(Quat{1, 0, 0, 0}, look_rotation, affected.weight);
            pose[affected.bone_index].rotation = partial_rot * pose[affected.bone_index].rotation;
        }
    }
}

Vec3 IKSolver::get_bone_world_position(
    int32_t bone_index,
    const std::vector<BoneTransform>& pose,
    const Skeleton& skeleton,
    const Mat4& world_transform
) const {
    if (bone_index < 0 || bone_index >= static_cast<int32_t>(pose.size())) {
        return Vec3{0.0f};
    }

    // Build chain from root to this bone
    std::vector<int32_t> chain;
    int32_t current = bone_index;
    while (current >= 0) {
        chain.push_back(current);
        current = skeleton.get_bone(current).parent_index;
    }

    // Calculate world transform by multiplying up the chain
    Mat4 result = world_transform;
    for (int i = static_cast<int>(chain.size()) - 1; i >= 0; --i) {
        result = result * pose[chain[i]].to_matrix();
    }

    return Vec3(result[3]);
}

void IKSolver::set_bone_world_position(
    int32_t bone_index,
    const Vec3& world_position,
    std::vector<BoneTransform>& pose,
    const Skeleton& skeleton,
    const Mat4& world_transform
) {
    // Simplified - would need inverse transform calculation
    if (bone_index >= 0 && bone_index < static_cast<int32_t>(pose.size())) {
        pose[bone_index].position = world_position;
    }
}

void IKSolver::apply_constraint(BoneTransform& transform, const BoneConstraint& constraint) {
    transform.rotation = constraint.constrain(transform.rotation);
}

// FootIKProcessor implementation

void FootIKProcessor::init(const FootIKSettings& settings) {
    m_left_foot_offset = 0.0f;
    m_right_foot_offset = 0.0f;
    m_pelvis_offset = 0.0f;
}

void FootIKProcessor::process(
    const FootIKSettings& settings,
    std::vector<BoneTransform>& pose,
    const Skeleton& skeleton,
    const Mat4& world_transform,
    float dt
) {
    if (!m_raycast) return;

    // Get current foot positions
    Vec3 left_foot_pos = m_solver.get_bone_world_position(
        settings.left_leg.end_bone, pose, skeleton, world_transform);
    Vec3 right_foot_pos = m_solver.get_bone_world_position(
        settings.right_leg.end_bone, pose, skeleton, world_transform);

    // Raycast for each foot
    FootIKRaycastResult left_hit = raycast_foot(left_foot_pos, settings, world_transform);
    FootIKRaycastResult right_hit = raycast_foot(right_foot_pos, settings, world_transform);

    // Calculate target offsets
    if (left_hit.hit) {
        m_left_foot_target = left_hit.position.y - left_foot_pos.y + settings.foot_height;
    } else {
        m_left_foot_target = 0.0f;
    }

    if (right_hit.hit) {
        m_right_foot_target = right_hit.position.y - right_foot_pos.y + settings.foot_height;
    } else {
        m_right_foot_target = 0.0f;
    }

    // Calculate pelvis offset (move down to keep lowest foot on ground)
    m_pelvis_target = std::min(0.0f, std::min(m_left_foot_target, m_right_foot_target));
    m_pelvis_target = std::clamp(m_pelvis_target, -settings.pelvis_offset_limit, 0.0f);

    // Smooth the offsets
    float pos_lerp = 1.0f - std::exp(-settings.position_speed * dt);
    m_left_foot_offset = glm::mix(m_left_foot_offset, m_left_foot_target - m_pelvis_target, pos_lerp);
    m_right_foot_offset = glm::mix(m_right_foot_offset, m_right_foot_target - m_pelvis_target, pos_lerp);
    m_pelvis_offset = glm::mix(m_pelvis_offset, m_pelvis_target, pos_lerp);

    // Apply pelvis offset
    if (settings.pelvis_bone >= 0 && settings.pelvis_bone < static_cast<int32_t>(pose.size())) {
        pose[settings.pelvis_bone].position.y += m_pelvis_offset;
    }

    // Apply foot IK
    TwoBoneIKSettings left_ik = settings.left_leg;
    left_ik.target_position = left_foot_pos + Vec3{0.0f, m_left_foot_offset, 0.0f};
    m_solver.solve_two_bone(left_ik, pose, skeleton, world_transform);

    TwoBoneIKSettings right_ik = settings.right_leg;
    right_ik.target_position = right_foot_pos + Vec3{0.0f, m_right_foot_offset, 0.0f};
    m_solver.solve_two_bone(right_ik, pose, skeleton, world_transform);

    // Rotate feet to match ground normal
    float rot_lerp = 1.0f - std::exp(-settings.rotation_speed * dt);

    if (left_hit.hit && settings.left_foot_bone >= 0 &&
        settings.left_foot_bone < static_cast<int32_t>(pose.size())) {
        Quat target_rot = glm::rotation(Vec3{0.0f, 1.0f, 0.0f}, left_hit.normal);
        m_left_foot_rotation = glm::slerp(m_left_foot_rotation, target_rot, rot_lerp);
        pose[settings.left_foot_bone].rotation = m_left_foot_rotation * pose[settings.left_foot_bone].rotation;
    }

    if (right_hit.hit && settings.right_foot_bone >= 0 &&
        settings.right_foot_bone < static_cast<int32_t>(pose.size())) {
        Quat target_rot = glm::rotation(Vec3{0.0f, 1.0f, 0.0f}, right_hit.normal);
        m_right_foot_rotation = glm::slerp(m_right_foot_rotation, target_rot, rot_lerp);
        pose[settings.right_foot_bone].rotation = m_right_foot_rotation * pose[settings.right_foot_bone].rotation;
    }
}

FootIKRaycastResult FootIKProcessor::raycast_foot(
    const Vec3& foot_position,
    const FootIKSettings& settings,
    const Mat4& world_transform
) {
    if (!m_raycast) {
        return FootIKRaycastResult{};
    }

    Vec3 ray_origin = foot_position + Vec3{0.0f, settings.ray_start_offset, 0.0f};
    Vec3 ray_dir{0.0f, -1.0f, 0.0f};

    return m_raycast(ray_origin, ray_dir, settings.ray_length, settings.ground_layer_mask);
}

// IKComponent implementation

void IKComponent::process(
    std::vector<BoneTransform>& pose,
    const Skeleton& skeleton,
    const Mat4& world_transform,
    float dt
) {
    // Process foot IK
    if (foot_ik_enabled) {
        foot_ik.process(foot_ik_settings, pose, skeleton, world_transform, dt);
    }

    // Process look-at IK
    if (look_at_enabled) {
        for (const auto& look_at : look_at_targets) {
            solver.solve_look_at(look_at, pose, skeleton, world_transform);
        }
    }

    // Process two-bone IK (arms, etc.)
    for (const auto& two_bone : two_bone_targets) {
        solver.solve_two_bone(two_bone, pose, skeleton, world_transform);
    }

    // Process hand IK
    if (hand_ik_enabled) {
        for (const auto& hand : hand_targets) {
            solver.solve_two_bone(hand.arm, pose, skeleton, world_transform);
            // Additional hand rotation could be applied here
        }
    }
}

// Helper functions

namespace IKHelpers {

void setup_foot_ik_humanoid(
    FootIKSettings& settings,
    const Skeleton& skeleton,
    const std::string& left_hip,
    const std::string& left_knee,
    const std::string& left_ankle,
    const std::string& right_hip,
    const std::string& right_knee,
    const std::string& right_ankle,
    const std::string& pelvis
) {
    settings.left_leg.root_bone = skeleton.find_bone(left_hip);
    settings.left_leg.mid_bone = skeleton.find_bone(left_knee);
    settings.left_leg.end_bone = skeleton.find_bone(left_ankle);
    settings.left_foot_bone = skeleton.find_bone(left_ankle);

    settings.right_leg.root_bone = skeleton.find_bone(right_hip);
    settings.right_leg.mid_bone = skeleton.find_bone(right_knee);
    settings.right_leg.end_bone = skeleton.find_bone(right_ankle);
    settings.right_foot_bone = skeleton.find_bone(right_ankle);

    settings.pelvis_bone = skeleton.find_bone(pelvis);

    // Default pole vectors (knees bend forward)
    settings.left_leg.pole_vector = Vec3{0.0f, 0.0f, 1.0f};
    settings.right_leg.pole_vector = Vec3{0.0f, 0.0f, 1.0f};
}

void setup_look_at_humanoid(
    LookAtIKSettings& settings,
    const Skeleton& skeleton,
    const std::string& head,
    const std::string& neck,
    float neck_weight
) {
    settings.bone_index = skeleton.find_bone(head);

    int32_t neck_bone = skeleton.find_bone(neck);
    if (neck_bone >= 0) {
        settings.affected_bones.push_back({neck_bone, neck_weight});
    }

    settings.forward_axis = Vec3{0.0f, 0.0f, 1.0f};
    settings.up_axis = Vec3{0.0f, 1.0f, 0.0f};
}

void setup_arm_ik(
    TwoBoneIKSettings& settings,
    const Skeleton& skeleton,
    bool is_left,
    const std::string& shoulder,
    const std::string& elbow,
    const std::string& wrist
) {
    std::string prefix = is_left ? "Left" : "Right";

    settings.root_bone = skeleton.find_bone(prefix + shoulder);
    settings.mid_bone = skeleton.find_bone(prefix + elbow);
    settings.end_bone = skeleton.find_bone(prefix + wrist);

    // Default pole vector (elbows bend back)
    settings.pole_vector = Vec3{0.0f, 0.0f, -1.0f};
}

} // namespace IKHelpers

} // namespace engine::render
