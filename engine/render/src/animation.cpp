#include <engine/render/animation.hpp>
#include <engine/core/log.hpp>
#include <algorithm>
#include <cmath>

namespace engine::render {

using namespace engine::core;

// ============================================================================
// AnimationChannel implementation
// ============================================================================

void AnimationChannel::set_target(int32_t bone_index, TargetType target_type) {
    m_bone_index = bone_index;
    m_target_type = target_type;
}

void AnimationChannel::add_position_keyframe(float time, const Vec3& position) {
    Keyframe<Vec3> key;
    key.time = time;
    key.value = position;
    key.in_tangent = Vec3(0.0f);
    key.out_tangent = Vec3(0.0f);

    // Insert in sorted order
    auto it = std::lower_bound(m_position_keys.begin(), m_position_keys.end(), key,
        [](const Keyframe<Vec3>& a, const Keyframe<Vec3>& b) { return a.time < b.time; });
    m_position_keys.insert(it, key);
}

void AnimationChannel::add_rotation_keyframe(float time, const Quat& rotation) {
    Keyframe<Quat> key;
    key.time = time;
    key.value = rotation;
    key.in_tangent = Quat(1.0f, 0.0f, 0.0f, 0.0f);
    key.out_tangent = Quat(1.0f, 0.0f, 0.0f, 0.0f);

    auto it = std::lower_bound(m_rotation_keys.begin(), m_rotation_keys.end(), key,
        [](const Keyframe<Quat>& a, const Keyframe<Quat>& b) { return a.time < b.time; });
    m_rotation_keys.insert(it, key);
}

void AnimationChannel::add_scale_keyframe(float time, const Vec3& scale) {
    Keyframe<Vec3> key;
    key.time = time;
    key.value = scale;
    key.in_tangent = Vec3(0.0f);
    key.out_tangent = Vec3(0.0f);

    auto it = std::lower_bound(m_scale_keys.begin(), m_scale_keys.end(), key,
        [](const Keyframe<Vec3>& a, const Keyframe<Vec3>& b) { return a.time < b.time; });
    m_scale_keys.insert(it, key);
}

template<typename T>
T AnimationChannel::sample_channel(const std::vector<Keyframe<T>>& keyframes, float time) const {
    if (keyframes.empty()) {
        return T{};
    }

    // Before first keyframe
    if (time <= keyframes.front().time) {
        return keyframes.front().value;
    }

    // After last keyframe
    if (time >= keyframes.back().time) {
        return keyframes.back().value;
    }

    // Find surrounding keyframes
    size_t next_idx = 0;
    for (size_t i = 0; i < keyframes.size(); ++i) {
        if (keyframes[i].time > time) {
            next_idx = i;
            break;
        }
    }

    size_t prev_idx = next_idx - 1;
    const auto& prev_key = keyframes[prev_idx];
    const auto& next_key = keyframes[next_idx];

    // Calculate interpolation factor
    float dt = next_key.time - prev_key.time;
    float t = (time - prev_key.time) / dt;

    // Apply interpolation based on mode
    switch (m_interpolation) {
        case AnimationInterpolation::Step:
            return prev_key.value;

        case AnimationInterpolation::Linear:
            // Linear interpolation handled differently for quaternions
            if constexpr (std::is_same_v<T, Quat>) {
                return glm::slerp(prev_key.value, next_key.value, t);
            } else {
                return glm::mix(prev_key.value, next_key.value, t);
            }

        case AnimationInterpolation::CubicSpline:
            // Hermite spline interpolation
            if constexpr (std::is_same_v<T, Quat>) {
                // For quaternions, use squad or fall back to slerp
                return glm::slerp(prev_key.value, next_key.value, t);
            } else {
                float t2 = t * t;
                float t3 = t2 * t;

                // Hermite basis functions
                float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
                float h10 = t3 - 2.0f * t2 + t;
                float h01 = -2.0f * t3 + 3.0f * t2;
                float h11 = t3 - t2;

                return prev_key.value * h00 +
                       prev_key.out_tangent * (dt * h10) +
                       next_key.value * h01 +
                       next_key.in_tangent * (dt * h11);
            }

        default:
            return prev_key.value;
    }
}

Vec3 AnimationChannel::sample_position(float time) const {
    return sample_channel(m_position_keys, time);
}

Quat AnimationChannel::sample_rotation(float time) const {
    return sample_channel(m_rotation_keys, time);
}

Vec3 AnimationChannel::sample_scale(float time) const {
    return sample_channel(m_scale_keys, time);
}

float AnimationChannel::get_duration() const {
    float duration = 0.0f;

    if (!m_position_keys.empty()) {
        duration = std::max(duration, m_position_keys.back().time);
    }
    if (!m_rotation_keys.empty()) {
        duration = std::max(duration, m_rotation_keys.back().time);
    }
    if (!m_scale_keys.empty()) {
        duration = std::max(duration, m_scale_keys.back().time);
    }

    return duration;
}

// ============================================================================
// AnimationClip implementation
// ============================================================================

AnimationClip::AnimationClip(const std::string& name)
    : m_name(name)
{
}

AnimationChannel& AnimationClip::add_channel() {
    m_channels.emplace_back();
    return m_channels.back();
}

void AnimationClip::sample(float time, std::vector<BoneTransform>& out_pose) const {
    for (const auto& channel : m_channels) {
        int32_t bone_index = channel.get_bone_index();
        if (bone_index < 0 || bone_index >= static_cast<int32_t>(out_pose.size())) {
            continue;
        }

        BoneTransform& transform = out_pose[bone_index];

        switch (channel.get_target_type()) {
            case AnimationChannel::TargetType::Translation:
                if (channel.get_position_keyframe_count() > 0) {
                    transform.position = channel.sample_position(time);
                }
                break;

            case AnimationChannel::TargetType::Rotation:
                if (channel.get_rotation_keyframe_count() > 0) {
                    transform.rotation = channel.sample_rotation(time);
                }
                break;

            case AnimationChannel::TargetType::Scale:
                if (channel.get_scale_keyframe_count() > 0) {
                    transform.scale = channel.sample_scale(time);
                }
                break;
        }
    }
}

void AnimationClip::sample_looped(float time, std::vector<BoneTransform>& out_pose) const {
    if (m_duration > 0.0f) {
        time = std::fmod(time, m_duration);
        if (time < 0.0f) {
            time += m_duration;
        }
    }
    sample(time, out_pose);
}

// ============================================================================
// Animator implementation
// ============================================================================

Animator::Animator(SkeletonInstance* skeleton)
    : m_skeleton(skeleton)
{
}

void Animator::play(std::shared_ptr<AnimationClip> clip, float blend_time) {
    if (!clip) return;

    if (blend_time > 0.0f && m_current_state.playing) {
        // Set up blend transition
        m_blend_from_state = m_current_state;
        m_blend_time = blend_time;
        m_blend_progress = 0.0f;
    }

    m_current_state.clip = clip;
    m_current_state.time = 0.0f;
    m_current_state.playing = true;
    m_current_state.looping = m_looping;
    m_current_state.speed = m_speed;
}

void Animator::play(const std::string& name, float blend_time) {
    auto it = m_clips.find(name);
    if (it != m_clips.end()) {
        play(it->second, blend_time);
    }
}

void Animator::stop() {
    m_current_state.playing = false;
    m_current_state.time = 0.0f;
    m_blend_time = 0.0f;
}

bool Animator::is_playing() const {
    return m_current_state.playing;
}

bool Animator::is_playing(const std::string& name) const {
    return m_current_state.playing &&
           m_current_state.clip &&
           m_current_state.clip->get_name() == name;
}

std::shared_ptr<AnimationClip> Animator::get_current_clip() const {
    return m_current_state.clip;
}

float Animator::get_current_time() const {
    return m_current_state.time;
}

float Animator::get_normalized_time() const {
    if (m_current_state.clip && m_current_state.clip->get_duration() > 0.0f) {
        return m_current_state.time / m_current_state.clip->get_duration();
    }
    return 0.0f;
}

void Animator::update(float delta_time) {
    if (m_paused || !m_skeleton) {
        return;
    }

    // Update blend progress
    if (m_blend_time > 0.0f) {
        m_blend_progress += delta_time / m_blend_time;
        if (m_blend_progress >= 1.0f) {
            m_blend_progress = 1.0f;
            m_blend_time = 0.0f;
        }
    }

    // Update current animation time
    if (m_current_state.playing && m_current_state.clip) {
        float prev_time = m_current_state.time;
        m_current_state.time += delta_time * m_current_state.speed * m_speed;

        float duration = m_current_state.clip->get_duration();
        if (duration > 0.0f) {
            if (m_current_state.time >= duration) {
                if (m_current_state.looping) {
                    m_current_state.time = std::fmod(m_current_state.time, duration);
                } else {
                    m_current_state.time = duration;
                    m_current_state.playing = false;
                }
            }
        }

        // Check for animation events
        check_events(m_current_state, prev_time, m_current_state.time);
    }

    // Apply animations to skeleton
    std::vector<BoneTransform>& pose = m_skeleton->get_pose();

    // Reset to bind pose before applying animations
    *m_skeleton = SkeletonInstance(m_skeleton->get_skeleton());

    // Apply blend-from animation
    if (m_blend_time > 0.0f && m_blend_from_state.clip) {
        apply_animation(m_blend_from_state, 1.0f - m_blend_progress);
    }

    // Apply current animation
    if (m_current_state.clip) {
        float weight = (m_blend_time > 0.0f) ? m_blend_progress : 1.0f;
        apply_animation(m_current_state, weight);
    }

    // Apply animation layers
    for (int i = 1; i < MAX_LAYERS; ++i) {
        if (m_layers[i].playing && m_layers[i].clip && m_layer_weights[i] > 0.0f) {
            apply_animation(m_layers[i], m_layer_weights[i]);
        }
    }

    // Mark matrices as dirty
    m_skeleton->calculate_matrices();
}

void Animator::apply_animation(const AnimationState& state, float blend_weight) {
    if (!state.clip || blend_weight <= 0.0f) {
        return;
    }

    std::vector<BoneTransform>& pose = m_skeleton->get_pose();

    if (blend_weight >= 1.0f) {
        // Full weight - just sample directly
        if (state.looping) {
            state.clip->sample_looped(state.time, pose);
        } else {
            state.clip->sample(state.time, pose);
        }
    } else {
        // Partial weight - need to blend
        std::vector<BoneTransform> anim_pose = pose;
        if (state.looping) {
            state.clip->sample_looped(state.time, anim_pose);
        } else {
            state.clip->sample(state.time, anim_pose);
        }

        // Blend each bone transform
        for (size_t i = 0; i < pose.size() && i < anim_pose.size(); ++i) {
            pose[i] = BoneTransform::lerp(pose[i], anim_pose[i], blend_weight);
        }
    }
}

void Animator::check_events(const AnimationState& state, float prev_time, float curr_time) {
    if (!m_event_callback || !state.clip) {
        return;
    }

    auto it = m_events.find(state.clip->get_name());
    if (it == m_events.end()) {
        return;
    }

    for (const auto& event : it->second) {
        // Check if event time is between prev_time and curr_time
        bool triggered = false;

        if (curr_time >= prev_time) {
            // Normal playback
            triggered = (event.time > prev_time && event.time <= curr_time);
        } else {
            // Looped - check both sides
            triggered = (event.time > prev_time) || (event.time <= curr_time);
        }

        if (triggered) {
            m_event_callback(event.name);
        }
    }
}

void Animator::add_clip(const std::string& name, std::shared_ptr<AnimationClip> clip) {
    m_clips[name] = clip;
}

std::shared_ptr<AnimationClip> Animator::get_clip(const std::string& name) const {
    auto it = m_clips.find(name);
    if (it != m_clips.end()) {
        return it->second;
    }
    return nullptr;
}

void Animator::remove_clip(const std::string& name) {
    m_clips.erase(name);
}

void Animator::set_layer_weight(int layer, float weight) {
    if (layer >= 0 && layer < MAX_LAYERS) {
        m_layer_weights[layer] = std::clamp(weight, 0.0f, 1.0f);
    }
}

void Animator::play_on_layer(int layer, std::shared_ptr<AnimationClip> clip, float /*blend_time*/) {
    if (layer >= 0 && layer < MAX_LAYERS && clip) {
        m_layers[layer].clip = clip;
        m_layers[layer].time = 0.0f;
        m_layers[layer].playing = true;
        m_layers[layer].looping = true;
    }
}

void Animator::add_event(const std::string& clip_name, float time, const std::string& event_name) {
    m_events[clip_name].push_back({time, event_name});
}

} // namespace engine::render
