#pragma once

#include <engine/render/skeleton.hpp>
#include <engine/core/math.hpp>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>

namespace engine::render {

using namespace engine::core;

// Interpolation mode for animation keyframes
enum class AnimationInterpolation {
    Step,       // No interpolation, snap to keyframe
    Linear,     // Linear interpolation
    CubicSpline // Cubic spline interpolation (glTF)
};

// A single keyframe in an animation channel
template<typename T>
struct Keyframe {
    float time;
    T value;
    T in_tangent;   // For cubic spline
    T out_tangent;  // For cubic spline
};

// Animation channel - animates a single property of a single bone
class AnimationChannel {
public:
    enum class TargetType {
        Translation,
        Rotation,
        Scale
    };

    AnimationChannel() = default;

    // Set target bone and property
    void set_target(int32_t bone_index, TargetType target_type);
    int32_t get_bone_index() const { return m_bone_index; }
    TargetType get_target_type() const { return m_target_type; }

    // Set interpolation mode
    void set_interpolation(AnimationInterpolation interp) { m_interpolation = interp; }
    AnimationInterpolation get_interpolation() const { return m_interpolation; }

    // Add keyframes
    void add_position_keyframe(float time, const Vec3& position);
    void add_rotation_keyframe(float time, const Quat& rotation);
    void add_scale_keyframe(float time, const Vec3& scale);

    // Sample the channel at a given time
    Vec3 sample_position(float time) const;
    Quat sample_rotation(float time) const;
    Vec3 sample_scale(float time) const;

    // Get duration
    float get_duration() const;

    // Get keyframe counts
    size_t get_position_keyframe_count() const { return m_position_keys.size(); }
    size_t get_rotation_keyframe_count() const { return m_rotation_keys.size(); }
    size_t get_scale_keyframe_count() const { return m_scale_keys.size(); }

private:
    template<typename T>
    T sample_channel(const std::vector<Keyframe<T>>& keyframes, float time) const;

    int32_t m_bone_index = -1;
    TargetType m_target_type = TargetType::Translation;
    AnimationInterpolation m_interpolation = AnimationInterpolation::Linear;

    std::vector<Keyframe<Vec3>> m_position_keys;
    std::vector<Keyframe<Quat>> m_rotation_keys;
    std::vector<Keyframe<Vec3>> m_scale_keys;
};

// Animation clip - a complete animation (walk, run, idle, etc.)
class AnimationClip {
public:
    AnimationClip() = default;
    explicit AnimationClip(const std::string& name);

    // Name
    const std::string& get_name() const { return m_name; }
    void set_name(const std::string& name) { m_name = name; }

    // Duration
    float get_duration() const { return m_duration; }
    void set_duration(float duration) { m_duration = duration; }

    // Ticks per second (for time scaling)
    float get_ticks_per_second() const { return m_ticks_per_second; }
    void set_ticks_per_second(float tps) { m_ticks_per_second = tps; }

    // Channels
    AnimationChannel& add_channel();
    const std::vector<AnimationChannel>& get_channels() const { return m_channels; }
    std::vector<AnimationChannel>& get_channels() { return m_channels; }

    // Sample the entire animation at a time, writing to pose
    void sample(float time, std::vector<BoneTransform>& out_pose) const;

    // Sample with looping
    void sample_looped(float time, std::vector<BoneTransform>& out_pose) const;

private:
    std::string m_name;
    float m_duration = 0.0f;
    float m_ticks_per_second = 25.0f;
    std::vector<AnimationChannel> m_channels;
};

// Animation blend mode
enum class AnimationBlendMode {
    Override,   // New animation completely replaces old
    Additive,   // Add on top of base pose
    Blend       // Blend between animations
};

// Animation playback state
struct AnimationState {
    std::shared_ptr<AnimationClip> clip;
    float time = 0.0f;
    float speed = 1.0f;
    float weight = 1.0f;
    bool looping = true;
    bool playing = false;
    AnimationBlendMode blend_mode = AnimationBlendMode::Override;
};

// Animation event callback
using AnimationEventCallback = std::function<void(const std::string& event_name)>;

// Animation event (triggered at specific times)
struct AnimationEvent {
    float time;
    std::string name;
};

// Animator - manages animation playback for a skeleton instance
class Animator {
public:
    Animator() = default;
    explicit Animator(SkeletonInstance* skeleton);

    // Set skeleton instance
    void set_skeleton(SkeletonInstance* skeleton) { m_skeleton = skeleton; }
    SkeletonInstance* get_skeleton() { return m_skeleton; }

    // Play an animation
    void play(std::shared_ptr<AnimationClip> clip, float blend_time = 0.0f);
    void play(const std::string& name, float blend_time = 0.0f);

    // Stop current animation
    void stop();

    // Pause/resume
    void pause() { m_paused = true; }
    void resume() { m_paused = false; }
    bool is_paused() const { return m_paused; }

    // Check if playing
    bool is_playing() const;
    bool is_playing(const std::string& name) const;

    // Get current animation
    std::shared_ptr<AnimationClip> get_current_clip() const;
    float get_current_time() const;
    float get_normalized_time() const;  // 0-1 range

    // Speed control
    void set_speed(float speed) { m_speed = speed; }
    float get_speed() const { return m_speed; }

    // Looping
    void set_looping(bool loop) { m_looping = loop; }
    bool is_looping() const { return m_looping; }

    // Update animation (call once per frame)
    void update(float delta_time);

    // Animation library management
    void add_clip(const std::string& name, std::shared_ptr<AnimationClip> clip);
    std::shared_ptr<AnimationClip> get_clip(const std::string& name) const;
    void remove_clip(const std::string& name);

    // Blending layers
    void set_layer_weight(int layer, float weight);
    void play_on_layer(int layer, std::shared_ptr<AnimationClip> clip, float blend_time = 0.0f);

    // Events
    void set_event_callback(AnimationEventCallback callback) { m_event_callback = callback; }
    void add_event(const std::string& clip_name, float time, const std::string& event_name);

private:
    void apply_animation(const AnimationState& state, float blend_weight);
    void check_events(const AnimationState& state, float prev_time, float curr_time);

    SkeletonInstance* m_skeleton = nullptr;

    // Current animation state
    AnimationState m_current_state;
    AnimationState m_blend_from_state;
    float m_blend_time = 0.0f;
    float m_blend_progress = 0.0f;

    // Playback control
    float m_speed = 1.0f;
    bool m_looping = true;
    bool m_paused = false;

    // Animation library
    std::unordered_map<std::string, std::shared_ptr<AnimationClip>> m_clips;

    // Animation layers (for blending upper/lower body, additive, etc.)
    static const int MAX_LAYERS = 4;
    std::array<AnimationState, MAX_LAYERS> m_layers;
    std::array<float, MAX_LAYERS> m_layer_weights{1.0f, 0.0f, 0.0f, 0.0f};

    // Events
    AnimationEventCallback m_event_callback;
    std::unordered_map<std::string, std::vector<AnimationEvent>> m_events;
};

} // namespace engine::render
