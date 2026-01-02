#include <engine/cinematic/light_track.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/render_components.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>

namespace engine::cinematic {

LightTrack::LightTrack(const std::string& name)
    : Track(name, TrackType::Light) {
}

void LightTrack::add_keyframe(const LightKeyframe& keyframe) {
    m_keyframes.push_back(keyframe);
    sort_keyframes();
}

void LightTrack::remove_keyframe(size_t index) {
    if (index < m_keyframes.size()) {
        m_keyframes.erase(m_keyframes.begin() + index);
    }
}

void LightTrack::clear_keyframes() {
    m_keyframes.clear();
}

float LightTrack::get_duration() const {
    if (m_keyframes.empty()) {
        return 0.0f;
    }
    return m_keyframes.back().time;
}

void LightTrack::evaluate(float time, scene::World& world) {
    if (!m_enabled || m_keyframes.empty() || m_target_entity == scene::NullEntity) {
        return;
    }

    // Store world reference for reset
    m_world = &world;

    // Capture initial state on first evaluation
    if (!m_has_initial_state && world.has<scene::Light>(m_target_entity)) {
        const auto& light = world.get<scene::Light>(m_target_entity);
        m_initial_state.color = light.color;
        m_initial_state.intensity = light.intensity;
        m_initial_state.range = light.range;
        m_initial_state.spot_inner_angle = light.spot_inner_angle;
        m_initial_state.spot_outer_angle = light.spot_outer_angle;
        m_has_initial_state = true;
    }

    // Sample light values at time
    LightKeyframe sampled = sample(time);

    // Apply to target light entity
    if (world.has<scene::Light>(m_target_entity)) {
        auto& light = world.get<scene::Light>(m_target_entity);
        light.color = sampled.color;
        light.intensity = sampled.intensity;
        light.range = sampled.range;
        light.spot_inner_angle = sampled.spot_inner_angle;
        light.spot_outer_angle = sampled.spot_outer_angle;
    }
}

void LightTrack::reset() {
    if (m_has_initial_state && m_target_entity != scene::NullEntity && m_world) {
        if (m_world->has<scene::Light>(m_target_entity)) {
            auto& light = m_world->get<scene::Light>(m_target_entity);
            light.color = m_initial_state.color;
            light.intensity = m_initial_state.intensity;
            light.range = m_initial_state.range;
            light.spot_inner_angle = m_initial_state.spot_inner_angle;
            light.spot_outer_angle = m_initial_state.spot_outer_angle;
        }
    }
    m_has_initial_state = false;
}

LightKeyframe LightTrack::sample(float time) const {
    if (m_keyframes.empty()) {
        return LightKeyframe{};
    }

    // Before first keyframe
    if (time <= m_keyframes.front().time) {
        return m_keyframes.front();
    }

    // After last keyframe
    if (time >= m_keyframes.back().time) {
        return m_keyframes.back();
    }

    // Find surrounding keyframes
    size_t index = find_keyframe_index(time);
    const LightKeyframe& k0 = m_keyframes[index];
    const LightKeyframe& k1 = m_keyframes[index + 1];

    // Calculate interpolation factor
    float segment_duration = k1.time - k0.time;
    float t = (time - k0.time) / segment_duration;

    // Apply easing
    t = apply_easing(t, k0.easing);

    // Interpolate
    LightKeyframe result;
    result.time = time;

    switch (k0.interpolation) {
        case InterpolationMode::Step:
            result = k0;
            result.time = time;
            break;

        case InterpolationMode::Linear:
        case InterpolationMode::Bezier:
        case InterpolationMode::CatmullRom:
        default:
            result.color = interpolate_linear(k0.color, k1.color, t);
            result.intensity = interpolate_linear(k0.intensity, k1.intensity, t);
            result.range = interpolate_linear(k0.range, k1.range, t);
            result.spot_inner_angle = interpolate_linear(k0.spot_inner_angle, k1.spot_inner_angle, t);
            result.spot_outer_angle = interpolate_linear(k0.spot_outer_angle, k1.spot_outer_angle, t);
            break;
    }

    return result;
}

void LightTrack::sort_keyframes() {
    std::sort(m_keyframes.begin(), m_keyframes.end(),
        [](const LightKeyframe& a, const LightKeyframe& b) {
            return a.time < b.time;
        });
}

size_t LightTrack::find_keyframe_index(float time) const {
    if (m_keyframes.size() < 2) {
        return 0;
    }
    for (size_t i = 0; i < m_keyframes.size() - 1; ++i) {
        if (time >= m_keyframes[i].time && time < m_keyframes[i + 1].time) {
            return i;
        }
    }
    return m_keyframes.size() - 2;
}

void LightTrack::serialize(nlohmann::json& j) const {
    j["keyframes"] = nlohmann::json::array();
    for (const auto& kf : m_keyframes) {
        j["keyframes"].push_back({
            {"time", kf.time},
            {"color", {kf.color.x, kf.color.y, kf.color.z}},
            {"intensity", kf.intensity},
            {"range", kf.range},
            {"spot_inner_angle", kf.spot_inner_angle},
            {"spot_outer_angle", kf.spot_outer_angle},
            {"interpolation", static_cast<int>(kf.interpolation)},
            {"easing", static_cast<int>(kf.easing)}
        });
    }
}

void LightTrack::deserialize(const nlohmann::json& j) {
    m_keyframes.clear();

    if (j.contains("keyframes")) {
        for (const auto& kf_json : j["keyframes"]) {
            LightKeyframe kf;
            kf.time = kf_json.value("time", 0.0f);
            if (kf_json.contains("color")) {
                auto& col = kf_json["color"];
                kf.color = Vec3{col[0], col[1], col[2]};
            }
            kf.intensity = kf_json.value("intensity", 1.0f);
            kf.range = kf_json.value("range", 10.0f);
            kf.spot_inner_angle = kf_json.value("spot_inner_angle", 30.0f);
            kf.spot_outer_angle = kf_json.value("spot_outer_angle", 45.0f);
            kf.interpolation = static_cast<InterpolationMode>(kf_json.value("interpolation", 0));
            kf.easing = static_cast<EaseType>(kf_json.value("easing", 0));
            m_keyframes.push_back(kf);
        }
    }
}

} // namespace engine::cinematic
