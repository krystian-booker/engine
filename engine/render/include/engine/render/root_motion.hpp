#pragma once

#include <engine/render/animation.hpp>
#include <engine/render/skeleton.hpp>
#include <engine/core/math.hpp>
#include <string>
#include <memory>

namespace engine::render {

using namespace engine::core;

// Root motion extraction settings
struct RootMotionSettings {
    // Index of the root bone to extract motion from (typically pelvis/hips)
    int32_t root_bone_index = 0;

    // Which axes to extract translation from
    bool extract_translation_x = true;
    bool extract_translation_y = false;  // Usually false to prevent floating
    bool extract_translation_z = true;

    // Which rotations to extract
    bool extract_rotation_y = true;  // Yaw rotation around up axis
    bool extract_rotation_xz = false; // Pitch and roll (usually false)

    // How to apply the root motion
    enum class ApplicationMode {
        ApplyToTransform,    // Apply directly to entity's LocalTransform
        ApplyToPhysics,      // Apply as velocity to physics body
        ApplyToNavAgent,     // Apply via navigation agent system
        ExtractOnly          // Only extract, let user handle application
    };
    ApplicationMode application_mode = ApplicationMode::ApplyToTransform;

    // Blending with external movement (0 = full root motion, 1 = full external)
    float blend_with_external = 0.0f;

    // Scale factors for motion
    float translation_scale = 1.0f;
    float rotation_scale = 1.0f;
};

// Root motion data extracted from animation
struct RootMotionDelta {
    Vec3 translation{0.0f};       // World space translation delta
    Quat rotation{1, 0, 0, 0};    // World space rotation delta
    Vec3 velocity{0.0f};          // Instantaneous velocity (translation / dt)
    Vec3 angular_velocity{0.0f};  // Instantaneous angular velocity

    void reset() {
        translation = Vec3{0.0f};
        rotation = Quat{1, 0, 0, 0};
        velocity = Vec3{0.0f};
        angular_velocity = Vec3{0.0f};
    }

    // Accumulate another delta
    RootMotionDelta& operator+=(const RootMotionDelta& other) {
        translation += other.translation;
        rotation = other.rotation * rotation;
        velocity += other.velocity;
        angular_velocity += other.angular_velocity;
        return *this;
    }
};

// Root motion extractor - extracts motion from animation clips
class RootMotionExtractor {
public:
    RootMotionExtractor() = default;
    explicit RootMotionExtractor(const RootMotionSettings& settings);

    // Set extraction settings
    void set_settings(const RootMotionSettings& settings) { m_settings = settings; }
    const RootMotionSettings& get_settings() const { return m_settings; }

    // Extract root motion between two times in an animation
    void extract(
        const AnimationClip& clip,
        const Skeleton& skeleton,
        float prev_time,
        float curr_time,
        float delta_time,
        RootMotionDelta& out_delta
    );

    // Extract for looped animation (handles wrap-around)
    void extract_looped(
        const AnimationClip& clip,
        const Skeleton& skeleton,
        float prev_time,
        float curr_time,
        float delta_time,
        RootMotionDelta& out_delta
    );

    // Get the root transform at a specific time
    BoneTransform get_root_transform_at_time(
        const AnimationClip& clip,
        const Skeleton& skeleton,
        float time
    ) const;

    // Remove root motion from a pose (keeps only the offset from origin)
    void remove_root_motion_from_pose(
        std::vector<BoneTransform>& pose,
        const RootMotionDelta& motion_to_remove
    );

private:
    // Apply axis filtering based on settings
    Vec3 filter_translation(const Vec3& translation) const;
    Quat filter_rotation(const Quat& rotation) const;

    RootMotionSettings m_settings;
};

// Root motion applicator - applies extracted motion to different targets
class RootMotionApplicator {
public:
    RootMotionApplicator() = default;

    // Apply motion to a transform
    void apply_to_transform(
        const RootMotionDelta& delta,
        const RootMotionSettings& settings,
        Vec3& position,
        Quat& rotation
    );

    // Get velocity for physics application
    Vec3 get_linear_velocity(
        const RootMotionDelta& delta,
        const RootMotionSettings& settings
    ) const;

    // Get angular velocity for physics application
    Vec3 get_angular_velocity(
        const RootMotionDelta& delta,
        const RootMotionSettings& settings
    ) const;

    // Blend root motion with external velocity
    Vec3 blend_velocity(
        const Vec3& root_motion_velocity,
        const Vec3& external_velocity,
        float blend_factor  // 0 = root motion, 1 = external
    ) const;
};

// ECS Component for root motion on entities
struct RootMotionComponent {
    RootMotionSettings settings;
    RootMotionExtractor extractor;
    RootMotionApplicator applicator;

    // Current frame data
    RootMotionDelta current_delta;

    // Accumulated motion (for physics bodies that need cumulative deltas)
    RootMotionDelta accumulated_delta;

    // Optional external velocity to blend with
    Vec3 external_velocity{0.0f};

    // Previous animation time (for delta calculation)
    float prev_animation_time = 0.0f;

    // Whether root motion is enabled
    bool enabled = true;

    void reset() {
        current_delta.reset();
        accumulated_delta.reset();
        prev_animation_time = 0.0f;
    }
};

// Helper function to create root motion settings for common scenarios
namespace RootMotionPresets {

// Standard character locomotion (XZ translation, Y rotation)
inline RootMotionSettings locomotion() {
    RootMotionSettings settings;
    settings.extract_translation_x = true;
    settings.extract_translation_y = false;
    settings.extract_translation_z = true;
    settings.extract_rotation_y = true;
    settings.extract_rotation_xz = false;
    settings.application_mode = RootMotionSettings::ApplicationMode::ApplyToTransform;
    return settings;
}

// Full root motion (all axes)
inline RootMotionSettings full() {
    RootMotionSettings settings;
    settings.extract_translation_x = true;
    settings.extract_translation_y = true;
    settings.extract_translation_z = true;
    settings.extract_rotation_y = true;
    settings.extract_rotation_xz = true;
    settings.application_mode = RootMotionSettings::ApplicationMode::ApplyToTransform;
    return settings;
}

// Root motion for physics-based characters
inline RootMotionSettings physics_based() {
    RootMotionSettings settings;
    settings.extract_translation_x = true;
    settings.extract_translation_y = false;
    settings.extract_translation_z = true;
    settings.extract_rotation_y = true;
    settings.extract_rotation_xz = false;
    settings.application_mode = RootMotionSettings::ApplicationMode::ApplyToPhysics;
    return settings;
}

// In-place animations (no translation extracted)
inline RootMotionSettings in_place() {
    RootMotionSettings settings;
    settings.extract_translation_x = false;
    settings.extract_translation_y = false;
    settings.extract_translation_z = false;
    settings.extract_rotation_y = true;
    settings.extract_rotation_xz = false;
    settings.application_mode = RootMotionSettings::ApplicationMode::ExtractOnly;
    return settings;
}

} // namespace RootMotionPresets

} // namespace engine::render
