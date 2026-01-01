#include <engine/render/animation_graph.hpp>
#include <algorithm>
#include <cmath>

namespace engine::render {

// ClipNode implementation

ClipNode::ClipNode(std::shared_ptr<AnimationClip> clip)
    : m_clip(clip)
{
}

void ClipNode::evaluate(float dt, AnimationContext& ctx, std::vector<BoneTransform>& out_pose) {
    if (!m_clip) {
        return;
    }

    // Advance time
    m_time += dt * m_speed;

    float duration = get_duration();
    if (duration > 0.0f) {
        if (m_looping) {
            // Wrap time for looping
            while (m_time >= duration) {
                m_time -= duration;
            }
            while (m_time < 0.0f) {
                m_time += duration;
            }
        } else {
            // Clamp time for non-looping
            m_time = std::clamp(m_time, 0.0f, duration);
        }
    }

    // Sample the clip
    if (m_looping) {
        m_clip->sample_looped(m_time, out_pose);
    } else {
        m_clip->sample(m_time, out_pose);
    }
}

float ClipNode::get_duration() const {
    return m_clip ? m_clip->get_duration() : 0.0f;
}

void ClipNode::reset() {
    m_time = 0.0f;
}

std::unique_ptr<IAnimGraphNode> ClipNode::clone() const {
    auto cloned = std::make_unique<ClipNode>(m_clip);
    cloned->m_speed = m_speed;
    cloned->m_looping = m_looping;
    cloned->m_time = m_time;
    return cloned;
}

// Pose blending functions

void blend_poses(
    const std::vector<BoneTransform>& pose_a,
    const std::vector<BoneTransform>& pose_b,
    float blend_factor,
    std::vector<BoneTransform>& out_pose
) {
    if (pose_a.empty() && pose_b.empty()) {
        out_pose.clear();
        return;
    }

    // Handle edge cases
    if (blend_factor <= 0.0f) {
        out_pose = pose_a;
        return;
    }
    if (blend_factor >= 1.0f) {
        out_pose = pose_b;
        return;
    }

    // Resize output to match input size
    size_t bone_count = std::max(pose_a.size(), pose_b.size());
    out_pose.resize(bone_count);

    for (size_t i = 0; i < bone_count; ++i) {
        // Handle cases where one pose has fewer bones
        if (i >= pose_a.size()) {
            out_pose[i] = pose_b[i];
        } else if (i >= pose_b.size()) {
            out_pose[i] = pose_a[i];
        } else {
            // Interpolate between transforms
            out_pose[i] = BoneTransform::lerp(pose_a[i], pose_b[i], blend_factor);
        }
    }
}

void add_pose(
    const std::vector<BoneTransform>& base_pose,
    const std::vector<BoneTransform>& additive_pose,
    float weight,
    std::vector<BoneTransform>& out_pose
) {
    if (base_pose.empty()) {
        out_pose.clear();
        return;
    }

    // If no additive or zero weight, just copy base
    if (additive_pose.empty() || weight <= 0.0f) {
        out_pose = base_pose;
        return;
    }

    out_pose.resize(base_pose.size());

    for (size_t i = 0; i < base_pose.size(); ++i) {
        const BoneTransform& base = base_pose[i];

        if (i < additive_pose.size()) {
            const BoneTransform& additive = additive_pose[i];

            // Add position delta
            out_pose[i].position = base.position + additive.position * weight;

            // Multiply rotation (additive rotation is relative to identity)
            // Scale the rotation angle by weight using slerp from identity
            Quat scaled_rotation = glm::slerp(
                Quat{1.0f, 0.0f, 0.0f, 0.0f},
                additive.rotation,
                weight
            );
            out_pose[i].rotation = base.rotation * scaled_rotation;

            // Multiply scale (additive scale is relative to 1.0)
            Vec3 scale_delta = (additive.scale - Vec3{1.0f}) * weight;
            out_pose[i].scale = base.scale * (Vec3{1.0f} + scale_delta);
        } else {
            out_pose[i] = base;
        }
    }
}

void blend_poses_masked(
    const std::vector<BoneTransform>& pose_a,
    const std::vector<BoneTransform>& pose_b,
    float blend_factor,
    const std::vector<float>& bone_mask,
    std::vector<BoneTransform>& out_pose
) {
    if (pose_a.empty() && pose_b.empty()) {
        out_pose.clear();
        return;
    }

    size_t bone_count = std::max(pose_a.size(), pose_b.size());
    out_pose.resize(bone_count);

    for (size_t i = 0; i < bone_count; ++i) {
        // Get per-bone mask value (default to global blend_factor if no mask)
        float mask = (i < bone_mask.size()) ? bone_mask[i] : 1.0f;
        float effective_blend = blend_factor * mask;

        if (i >= pose_a.size()) {
            out_pose[i] = pose_b[i];
        } else if (i >= pose_b.size()) {
            out_pose[i] = pose_a[i];
        } else if (effective_blend <= 0.0f) {
            out_pose[i] = pose_a[i];
        } else if (effective_blend >= 1.0f) {
            out_pose[i] = pose_b[i];
        } else {
            out_pose[i] = BoneTransform::lerp(pose_a[i], pose_b[i], effective_blend);
        }
    }
}

} // namespace engine::render
