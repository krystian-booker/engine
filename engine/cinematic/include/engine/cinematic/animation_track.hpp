#pragma once

#include <engine/cinematic/track.hpp>
#include <engine/scene/entity.hpp>
#include <string>

namespace engine::cinematic {

// Animation clip reference
struct AnimationClipRef {
    std::string clip_name;
    float start_time = 0.0f;      // When in sequence this clip starts
    float duration = -1.0f;        // Duration (-1 = use clip length)
    float clip_start = 0.0f;       // Start time within the clip
    float playback_speed = 1.0f;
    float blend_in = 0.0f;
    float blend_out = 0.0f;
    bool loop = false;
};

// Blend between animations
struct AnimationBlend {
    float time = 0.0f;
    float duration = 0.3f;
    std::string from_clip;
    std::string to_clip;
};

// Animation track for controlling skeletal animations
class AnimationTrack : public Track {
public:
    explicit AnimationTrack(const std::string& name);
    ~AnimationTrack() override = default;

    // Set target entity (must have Animator component)
    void set_target_entity(scene::Entity entity) { m_target_entity = entity; }
    scene::Entity get_target_entity() const { return m_target_entity; }

    // Add animation clips to timeline
    void add_clip(const AnimationClipRef& clip);
    void remove_clip(size_t index);
    void clear_clips();

    size_t clip_count() const { return m_clips.size(); }
    AnimationClipRef& get_clip(size_t index) { return m_clips[index]; }
    const AnimationClipRef& get_clip(size_t index) const { return m_clips[index]; }

    // Add explicit blends
    void add_blend(const AnimationBlend& blend);

    // Track interface
    float get_duration() const override;
    void evaluate(float time) override;
    void reset() override;

    // Query active clip at time
    const AnimationClipRef* get_active_clip(float time) const;

private:
    void sort_clips();

    std::vector<AnimationClipRef> m_clips;
    std::vector<AnimationBlend> m_blends;
    scene::Entity m_target_entity = scene::NullEntity;

    // Playback state
    std::string m_current_clip;
    float m_current_clip_time = 0.0f;
};

// Transform track for animating entity transforms directly
class TransformTrack : public Track {
public:
    explicit TransformTrack(const std::string& name);
    ~TransformTrack() override = default;

    // Set target entity
    void set_target_entity(scene::Entity entity) { m_target_entity = entity; }
    scene::Entity get_target_entity() const { return m_target_entity; }

    // Add keyframes
    void add_position_key(float time, const Vec3& position);
    void add_rotation_key(float time, const Quat& rotation);
    void add_scale_key(float time, const Vec3& scale);

    void add_transform_key(float time, const Vec3& position, const Quat& rotation, const Vec3& scale);

    void clear_keyframes();

    // Track interface
    float get_duration() const override;
    void evaluate(float time) override;
    void reset() override;

    // Sample transform at time
    Vec3 sample_position(float time) const;
    Quat sample_rotation(float time) const;
    Vec3 sample_scale(float time) const;

private:
    std::vector<Keyframe<Vec3>> m_position_keys;
    std::vector<Keyframe<Quat>> m_rotation_keys;
    std::vector<Keyframe<Vec3>> m_scale_keys;

    scene::Entity m_target_entity = scene::NullEntity;

    // Initial state
    Vec3 m_initial_position{0.0f};
    Quat m_initial_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 m_initial_scale{1.0f};
    bool m_has_initial_state = false;
};

} // namespace engine::cinematic
