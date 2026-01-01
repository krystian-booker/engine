#include <engine/render/root_motion.hpp>
#include <cmath>
#include <algorithm>

namespace engine::render {

RootMotionExtractor::RootMotionExtractor(const RootMotionSettings& settings)
    : m_settings(settings)
{
}

void RootMotionExtractor::extract(
    const AnimationClip& clip,
    const Skeleton& skeleton,
    float prev_time,
    float curr_time,
    float delta_time,
    RootMotionDelta& out_delta
) {
    out_delta.reset();

    if (m_settings.root_bone_index < 0 || m_settings.root_bone_index >= skeleton.get_bone_count()) {
        return;
    }

    // Get root transforms at both times
    BoneTransform prev_transform = get_root_transform_at_time(clip, skeleton, prev_time);
    BoneTransform curr_transform = get_root_transform_at_time(clip, skeleton, curr_time);

    // Calculate translation delta
    Vec3 translation_delta = curr_transform.position - prev_transform.position;
    out_delta.translation = filter_translation(translation_delta);

    // Scale translation
    out_delta.translation *= m_settings.translation_scale;

    // Calculate rotation delta
    // The delta rotation is: curr * inverse(prev)
    Quat rotation_delta = curr_transform.rotation * glm::inverse(prev_transform.rotation);
    out_delta.rotation = filter_rotation(rotation_delta);

    // Apply rotation scale (by interpolating from identity)
    if (m_settings.rotation_scale != 1.0f) {
        out_delta.rotation = glm::slerp(Quat{1, 0, 0, 0}, out_delta.rotation, m_settings.rotation_scale);
    }

    // Calculate velocities
    if (delta_time > 0.0001f) {
        out_delta.velocity = out_delta.translation / delta_time;

        // Calculate angular velocity from quaternion delta
        // Using quaternion logarithm approximation
        Vec3 axis;
        float angle = glm::angle(out_delta.rotation);
        if (angle > 0.0001f) {
            axis = glm::axis(out_delta.rotation);
            out_delta.angular_velocity = axis * (angle / delta_time);
        }
    }
}

void RootMotionExtractor::extract_looped(
    const AnimationClip& clip,
    const Skeleton& skeleton,
    float prev_time,
    float curr_time,
    float delta_time,
    RootMotionDelta& out_delta
) {
    out_delta.reset();

    float duration = clip.get_duration();
    if (duration <= 0.0f) {
        return;
    }

    // Normalize times to be within animation duration
    while (prev_time < 0.0f) prev_time += duration;
    while (prev_time >= duration) prev_time -= duration;
    while (curr_time < 0.0f) curr_time += duration;
    while (curr_time >= duration) curr_time -= duration;

    if (curr_time >= prev_time) {
        // No wrap-around, extract normally
        extract(clip, skeleton, prev_time, curr_time, delta_time, out_delta);
    } else {
        // Animation wrapped around - split into two segments
        RootMotionDelta delta1, delta2;

        // First segment: prev_time to end
        float dt1 = duration - prev_time;
        extract(clip, skeleton, prev_time, duration, dt1, delta1);

        // Second segment: start to curr_time
        float dt2 = curr_time;
        extract(clip, skeleton, 0.0f, curr_time, dt2, delta2);

        // Combine deltas
        out_delta = delta1;
        out_delta += delta2;

        // Recalculate velocities based on total delta time
        if (delta_time > 0.0001f) {
            out_delta.velocity = out_delta.translation / delta_time;

            Vec3 axis;
            float angle = glm::angle(out_delta.rotation);
            if (angle > 0.0001f) {
                axis = glm::axis(out_delta.rotation);
                out_delta.angular_velocity = axis * (angle / delta_time);
            }
        }
    }
}

BoneTransform RootMotionExtractor::get_root_transform_at_time(
    const AnimationClip& clip,
    const Skeleton& skeleton,
    float time
) const {
    // Sample the animation at the given time
    std::vector<BoneTransform> pose = skeleton.get_bind_pose();
    clip.sample(time, pose);

    // Return the root bone's transform
    if (m_settings.root_bone_index >= 0 &&
        m_settings.root_bone_index < static_cast<int32_t>(pose.size())) {
        return pose[m_settings.root_bone_index];
    }

    return BoneTransform{};
}

void RootMotionExtractor::remove_root_motion_from_pose(
    std::vector<BoneTransform>& pose,
    const RootMotionDelta& motion_to_remove
) {
    if (m_settings.root_bone_index < 0 ||
        m_settings.root_bone_index >= static_cast<int32_t>(pose.size())) {
        return;
    }

    BoneTransform& root = pose[m_settings.root_bone_index];

    // Remove translation
    root.position -= motion_to_remove.translation;

    // Remove rotation
    root.rotation = glm::inverse(motion_to_remove.rotation) * root.rotation;
}

Vec3 RootMotionExtractor::filter_translation(const Vec3& translation) const {
    Vec3 result{0.0f};

    if (m_settings.extract_translation_x) result.x = translation.x;
    if (m_settings.extract_translation_y) result.y = translation.y;
    if (m_settings.extract_translation_z) result.z = translation.z;

    return result;
}

Quat RootMotionExtractor::filter_rotation(const Quat& rotation) const {
    if (!m_settings.extract_rotation_y && !m_settings.extract_rotation_xz) {
        return Quat{1, 0, 0, 0};
    }

    // Decompose rotation into yaw (Y axis) and pitch/roll (XZ axes)
    // Using swing-twist decomposition around Y axis

    Vec3 up{0.0f, 1.0f, 0.0f};

    // Extract the twist (rotation around Y axis)
    Vec3 rotated_up = rotation * up;
    Vec3 projected = glm::normalize(Vec3{rotated_up.x, 0.0f, rotated_up.z});

    Quat yaw_rotation{1, 0, 0, 0};
    if (glm::length(projected) > 0.001f) {
        // Calculate yaw from the projection
        float yaw_angle = std::atan2(projected.x, projected.z);
        yaw_rotation = glm::angleAxis(yaw_angle, up);
    }

    // The pitch/roll is: rotation * inverse(yaw)
    Quat pitch_roll = rotation * glm::inverse(yaw_rotation);

    Quat result{1, 0, 0, 0};

    if (m_settings.extract_rotation_y) {
        result = yaw_rotation;
    }

    if (m_settings.extract_rotation_xz) {
        result = pitch_roll * result;
    }

    return result;
}

// RootMotionApplicator implementation

void RootMotionApplicator::apply_to_transform(
    const RootMotionDelta& delta,
    const RootMotionSettings& settings,
    Vec3& position,
    Quat& rotation
) {
    // Apply rotation first (so translation is in the new orientation)
    rotation = delta.rotation * rotation;

    // Transform the translation by the current rotation and add to position
    Vec3 world_translation = rotation * delta.translation;
    position += world_translation;
}

Vec3 RootMotionApplicator::get_linear_velocity(
    const RootMotionDelta& delta,
    const RootMotionSettings& settings
) const {
    return delta.velocity * settings.translation_scale;
}

Vec3 RootMotionApplicator::get_angular_velocity(
    const RootMotionDelta& delta,
    const RootMotionSettings& settings
) const {
    return delta.angular_velocity * settings.rotation_scale;
}

Vec3 RootMotionApplicator::blend_velocity(
    const Vec3& root_motion_velocity,
    const Vec3& external_velocity,
    float blend_factor
) const {
    return glm::mix(root_motion_velocity, external_velocity, std::clamp(blend_factor, 0.0f, 1.0f));
}

} // namespace engine::render
