#include <engine/cinematic/animation_track.hpp>
#include <algorithm>

namespace engine::cinematic {

// ============================================================================
// AnimationTrack
// ============================================================================

AnimationTrack::AnimationTrack(const std::string& name)
    : Track(name, TrackType::Animation) {
}

void AnimationTrack::add_clip(const AnimationClipRef& clip) {
    m_clips.push_back(clip);
    sort_clips();
}

void AnimationTrack::remove_clip(size_t index) {
    if (index < m_clips.size()) {
        m_clips.erase(m_clips.begin() + index);
    }
}

void AnimationTrack::clear_clips() {
    m_clips.clear();
}

void AnimationTrack::add_blend(const AnimationBlend& blend) {
    m_blends.push_back(blend);
    std::sort(m_blends.begin(), m_blends.end(),
        [](const AnimationBlend& a, const AnimationBlend& b) {
            return a.time < b.time;
        });
}

float AnimationTrack::get_duration() const {
    float duration = 0.0f;
    for (const auto& clip : m_clips) {
        float clip_end = clip.start_time;
        if (clip.duration > 0) {
            clip_end += clip.duration;
        }
        duration = std::max(duration, clip_end);
    }
    return duration;
}

void AnimationTrack::evaluate(float time) {
    if (!m_enabled || m_clips.empty() || m_target_entity == scene::NullEntity) {
        return;
    }

    const AnimationClipRef* active = get_active_clip(time);
    if (!active) {
        return;
    }

    // Calculate local time within clip
    float local_time = (time - active->start_time) * active->playback_speed + active->clip_start;

    // Handle looping
    if (active->loop && active->duration > 0) {
        local_time = std::fmod(local_time, active->duration);
    }

    // Calculate blend weight
    float blend_weight = 1.0f;
    float clip_elapsed = time - active->start_time;
    float clip_remaining = (active->start_time + active->duration) - time;

    if (active->blend_in > 0 && clip_elapsed < active->blend_in) {
        blend_weight = clip_elapsed / active->blend_in;
    } else if (active->blend_out > 0 && active->duration > 0 && clip_remaining < active->blend_out) {
        blend_weight = clip_remaining / active->blend_out;
    }

    // Would apply to Animator component
    // auto& animator = world.get<Animator>(m_target_entity);
    // animator.set_animation(active->clip_name, local_time, blend_weight);

    m_current_clip = active->clip_name;
    m_current_clip_time = local_time;
}

void AnimationTrack::reset() {
    m_current_clip.clear();
    m_current_clip_time = 0.0f;
}

const AnimationClipRef* AnimationTrack::get_active_clip(float time) const {
    for (auto it = m_clips.rbegin(); it != m_clips.rend(); ++it) {
        if (time >= it->start_time) {
            if (it->duration < 0 || time < it->start_time + it->duration) {
                return &(*it);
            }
        }
    }
    return nullptr;
}

void AnimationTrack::sort_clips() {
    std::sort(m_clips.begin(), m_clips.end(),
        [](const AnimationClipRef& a, const AnimationClipRef& b) {
            return a.start_time < b.start_time;
        });
}

// ============================================================================
// TransformTrack
// ============================================================================

TransformTrack::TransformTrack(const std::string& name)
    : Track(name, TrackType::Transform) {
}

void TransformTrack::add_position_key(float time, const Vec3& position) {
    m_position_keys.emplace_back(time, position);
    std::sort(m_position_keys.begin(), m_position_keys.end(),
        [](const auto& a, const auto& b) { return a.time < b.time; });
}

void TransformTrack::add_rotation_key(float time, const Quat& rotation) {
    m_rotation_keys.emplace_back(time, rotation);
    std::sort(m_rotation_keys.begin(), m_rotation_keys.end(),
        [](const auto& a, const auto& b) { return a.time < b.time; });
}

void TransformTrack::add_scale_key(float time, const Vec3& scale) {
    m_scale_keys.emplace_back(time, scale);
    std::sort(m_scale_keys.begin(), m_scale_keys.end(),
        [](const auto& a, const auto& b) { return a.time < b.time; });
}

void TransformTrack::add_transform_key(float time, const Vec3& position, const Quat& rotation, const Vec3& scale) {
    add_position_key(time, position);
    add_rotation_key(time, rotation);
    add_scale_key(time, scale);
}

void TransformTrack::clear_keyframes() {
    m_position_keys.clear();
    m_rotation_keys.clear();
    m_scale_keys.clear();
}

float TransformTrack::get_duration() const {
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

void TransformTrack::evaluate(float time) {
    if (!m_enabled || m_target_entity == scene::NullEntity) {
        return;
    }

    // Store initial state on first evaluation
    if (!m_has_initial_state) {
        // Would get from LocalTransform component
        m_has_initial_state = true;
    }

    Vec3 position = sample_position(time);
    Quat rotation = sample_rotation(time);
    Vec3 scale = sample_scale(time);

    // Would apply to LocalTransform component
    // auto& transform = world.get<LocalTransform>(m_target_entity);
    // transform.position = position;
    // transform.rotation = rotation;
    // transform.scale = scale;
}

void TransformTrack::reset() {
    if (m_has_initial_state && m_target_entity != scene::NullEntity) {
        // Restore initial transform
    }
}

template<typename T>
static T sample_keys(const std::vector<Keyframe<T>>& keys, float time, const T& default_value) {
    if (keys.empty()) {
        return default_value;
    }

    if (time <= keys.front().time) {
        return keys.front().value;
    }

    if (time >= keys.back().time) {
        return keys.back().value;
    }

    // Find surrounding keys
    for (size_t i = 0; i < keys.size() - 1; ++i) {
        if (time >= keys[i].time && time < keys[i + 1].time) {
            float segment_duration = keys[i + 1].time - keys[i].time;
            float t = (time - keys[i].time) / segment_duration;
            t = apply_easing(t, keys[i].easing);
            return interpolate_linear(keys[i].value, keys[i + 1].value, t);
        }
    }

    return keys.back().value;
}

Vec3 TransformTrack::sample_position(float time) const {
    return sample_keys(m_position_keys, time, m_initial_position);
}

Quat TransformTrack::sample_rotation(float time) const {
    return sample_keys(m_rotation_keys, time, m_initial_rotation);
}

Vec3 TransformTrack::sample_scale(float time) const {
    return sample_keys(m_scale_keys, time, m_initial_scale);
}

} // namespace engine::cinematic
