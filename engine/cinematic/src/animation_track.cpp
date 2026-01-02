#include <engine/cinematic/animation_track.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/render/animation_state_machine.hpp>
#include <nlohmann/json.hpp>
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

void AnimationTrack::evaluate(float time, scene::World& world) {
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

    // Apply to AnimatorComponent
    if (world.has<render::AnimatorComponent>(m_target_entity)) {
        auto& animator_comp = world.get<render::AnimatorComponent>(m_target_entity);
        if (animator_comp.state_machine) {
            auto* state = animator_comp.state_machine->get_state(active->clip_name);
            if (state) {
                // Only switch state if we are not already playing it
                // OR if it's the first time we're evaluating this track
                if (animator_comp.state_machine->get_current_state_name() != active->clip_name) {
                    animator_comp.state_machine->set_state(active->clip_name);
                }
                
                state->speed = active->playback_speed;
                state->loop = active->loop;
                state->weight = blend_weight;
                
                // Force time update if we just switched to this clip in the track logic
                // (though set_state handles reset, we might be scrubbing time)
                // Note: Direct time control of ASM state usually requires manual update mode,
                // here we trust the ASM update loop, but we might want to sync time for accurate scrubbing.
                // For now, we rely on ASM internal update, but the weight is the critical fix.
            }
        }
    }

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

void TransformTrack::evaluate(float time, scene::World& world) {
    if (!m_enabled || m_target_entity == scene::NullEntity) {
        return;
    }

    // Store world reference for reset
    m_world = &world;

    // Store initial state on first evaluation
    if (!m_has_initial_state && world.has<scene::LocalTransform>(m_target_entity)) {
        const auto& t = world.get<scene::LocalTransform>(m_target_entity);
        m_initial_position = t.position;
        m_initial_rotation = t.rotation;
        m_initial_scale = t.scale;
        m_has_initial_state = true;
    }

    Vec3 position = sample_position(time);
    Quat rotation = sample_rotation(time);
    Vec3 scale = sample_scale(time);

    // Apply to LocalTransform component
    if (world.has<scene::LocalTransform>(m_target_entity)) {
        auto& transform = world.get<scene::LocalTransform>(m_target_entity);
        transform.position = position;
        transform.rotation = rotation;
        transform.scale = scale;
    }
}

void TransformTrack::reset() {
    if (m_has_initial_state && m_target_entity != scene::NullEntity && m_world) {
        if (m_world->has<scene::LocalTransform>(m_target_entity)) {
            auto& transform = m_world->get<scene::LocalTransform>(m_target_entity);
            transform.position = m_initial_position;
            transform.rotation = m_initial_rotation;
            transform.scale = m_initial_scale;
        }
    }
    m_has_initial_state = false;
}

template<typename T>
static T sample_keys_interpolated(const std::vector<Keyframe<T>>& keys, float time, const T& default_value) {
    if (keys.empty()) {
        return default_value;
    }

    // Before first key
    if (time <= keys.front().time) {
        return keys.front().value;
    }

    // After last key
    if (time >= keys.back().time) {
        return keys.back().value;
    }

    // Find segment
    size_t index = 0;
    for (size_t i = 0; i < keys.size() - 1; ++i) {
        if (time >= keys[i].time && time < keys[i + 1].time) {
            index = i;
            break;
        }
    }

    const auto& k0 = keys[index];
    const auto& k1 = keys[index + 1];

    float segment_duration = k1.time - k0.time;
    if (segment_duration <= 0.0001f) return k0.value;

    float t = (time - k0.time) / segment_duration;
    
    // Apply easing
    t = apply_easing(t, k0.easing);

    switch (k0.interpolation) {
        case InterpolationMode::Step:
            return k0.value;
            
        case InterpolationMode::Linear:
            return interpolate_linear(k0.value, k1.value, t);
            
        case InterpolationMode::Bezier:
            return interpolate_bezier(k0.value, k1.value, k0.tangent_out, k1.tangent_in, t);
            
        case InterpolationMode::CatmullRom: {
            // Need 4 points
            size_t i0 = index > 0 ? index - 1 : 0;
            size_t i1 = index;
            size_t i2 = index + 1;
            size_t i3 = std::min(index + 2, keys.size() - 1);
            
            return evaluate_catmull_rom(keys[i0].value, keys[i1].value, keys[i2].value, keys[i3].value, t);
        }
        
        default:
            return interpolate_linear(k0.value, k1.value, t);
    }
}

Vec3 TransformTrack::sample_position(float time) const {
    return sample_keys_interpolated(m_position_keys, time, m_initial_position);
}

Quat TransformTrack::sample_rotation(float time) const {
    return sample_keys_interpolated(m_rotation_keys, time, m_initial_rotation);
}

Vec3 TransformTrack::sample_scale(float time) const {
    return sample_keys_interpolated(m_scale_keys, time, m_initial_scale);
}

// ============================================================================
// AnimationTrack Serialization
// ============================================================================

void AnimationTrack::serialize(nlohmann::json& j) const {
    j["clips"] = nlohmann::json::array();
    for (const auto& clip : m_clips) {
        j["clips"].push_back({
            {"clip_name", clip.clip_name},
            {"start_time", clip.start_time},
            {"duration", clip.duration},
            {"clip_start", clip.clip_start},
            {"playback_speed", clip.playback_speed},
            {"blend_in", clip.blend_in},
            {"blend_out", clip.blend_out},
            {"loop", clip.loop}
        });
    }

    j["blends"] = nlohmann::json::array();
    for (const auto& blend : m_blends) {
        j["blends"].push_back({
            {"time", blend.time},
            {"duration", blend.duration},
            {"from_clip", blend.from_clip},
            {"to_clip", blend.to_clip}
        });
    }
}

void AnimationTrack::deserialize(const nlohmann::json& j) {
    m_clips.clear();
    m_blends.clear();

    if (j.contains("clips")) {
        for (const auto& clip_json : j["clips"]) {
            AnimationClipRef clip;
            clip.clip_name = clip_json.value("clip_name", "");
            clip.start_time = clip_json.value("start_time", 0.0f);
            clip.duration = clip_json.value("duration", -1.0f);
            clip.clip_start = clip_json.value("clip_start", 0.0f);
            clip.playback_speed = clip_json.value("playback_speed", 1.0f);
            clip.blend_in = clip_json.value("blend_in", 0.0f);
            clip.blend_out = clip_json.value("blend_out", 0.0f);
            clip.loop = clip_json.value("loop", false);
            m_clips.push_back(clip);
        }
    }

    if (j.contains("blends")) {
        for (const auto& blend_json : j["blends"]) {
            AnimationBlend blend;
            blend.time = blend_json.value("time", 0.0f);
            blend.duration = blend_json.value("duration", 0.3f);
            blend.from_clip = blend_json.value("from_clip", "");
            blend.to_clip = blend_json.value("to_clip", "");
            m_blends.push_back(blend);
        }
    }
}

// ============================================================================
// TransformTrack Serialization
// ============================================================================

void TransformTrack::serialize(nlohmann::json& j) const {
    j["position_keys"] = nlohmann::json::array();
    for (const auto& key : m_position_keys) {
        j["position_keys"].push_back({
            {"time", key.time},
            {"value", {key.value.x, key.value.y, key.value.z}},
            {"easing", static_cast<int>(key.easing)},
            {"interpolation", static_cast<int>(key.interpolation)}
        });
    }

    j["rotation_keys"] = nlohmann::json::array();
    for (const auto& key : m_rotation_keys) {
        j["rotation_keys"].push_back({
            {"time", key.time},
            {"value", {key.value.w, key.value.x, key.value.y, key.value.z}},
            {"easing", static_cast<int>(key.easing)},
            {"interpolation", static_cast<int>(key.interpolation)}
        });
    }

    j["scale_keys"] = nlohmann::json::array();
    for (const auto& key : m_scale_keys) {
        j["scale_keys"].push_back({
            {"time", key.time},
            {"value", {key.value.x, key.value.y, key.value.z}},
            {"easing", static_cast<int>(key.easing)},
            {"interpolation", static_cast<int>(key.interpolation)}
        });
    }
}

void TransformTrack::deserialize(const nlohmann::json& j) {
    m_position_keys.clear();
    m_rotation_keys.clear();
    m_scale_keys.clear();

    if (j.contains("position_keys")) {
        for (const auto& key_json : j["position_keys"]) {
            Keyframe<Vec3> key;
            key.time = key_json.value("time", 0.0f);
            if (key_json.contains("value")) {
                auto& v = key_json["value"];
                key.value = Vec3{v[0], v[1], v[2]};
            }
            key.easing = static_cast<EaseType>(key_json.value("easing", 0));
            key.interpolation = static_cast<InterpolationMode>(key_json.value("interpolation", 0));
            m_position_keys.push_back(key);
        }
    }

    if (j.contains("rotation_keys")) {
        for (const auto& key_json : j["rotation_keys"]) {
            Keyframe<Quat> key;
            key.time = key_json.value("time", 0.0f);
            if (key_json.contains("value")) {
                auto& v = key_json["value"];
                key.value = Quat{v[0], v[1], v[2], v[3]};
            }
            key.easing = static_cast<EaseType>(key_json.value("easing", 0));
            key.interpolation = static_cast<InterpolationMode>(key_json.value("interpolation", 0));
            m_rotation_keys.push_back(key);
        }
    }

    if (j.contains("scale_keys")) {
        for (const auto& key_json : j["scale_keys"]) {
            Keyframe<Vec3> key;
            key.time = key_json.value("time", 0.0f);
            if (key_json.contains("value")) {
                auto& v = key_json["value"];
                key.value = Vec3{v[0], v[1], v[2]};
            }
            key.easing = static_cast<EaseType>(key_json.value("easing", 0));
            key.interpolation = static_cast<InterpolationMode>(key_json.value("interpolation", 0));
            m_scale_keys.push_back(key);
        }
    }
}

} // namespace engine::cinematic
