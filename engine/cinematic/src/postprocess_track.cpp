#include <engine/cinematic/postprocess_track.hpp>
#include <engine/scene/world.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>

namespace engine::cinematic {

PostProcessTrack::PostProcessTrack(const std::string& name)
    : Track(name, TrackType::PostProcess) {
}

void PostProcessTrack::add_keyframe(const PostProcessKeyframe& keyframe) {
    m_keyframes.push_back(keyframe);
    sort_keyframes();
}

void PostProcessTrack::remove_keyframe(size_t index) {
    if (index < m_keyframes.size()) {
        m_keyframes.erase(m_keyframes.begin() + index);
    }
}

void PostProcessTrack::clear_keyframes() {
    m_keyframes.clear();
}

float PostProcessTrack::get_duration() const {
    if (m_keyframes.empty()) {
        return 0.0f;
    }
    return m_keyframes.back().time;
}

void PostProcessTrack::evaluate(float time, scene::World& /*world*/) {
    if (!m_enabled || m_keyframes.empty() || !m_post_process) {
        return;
    }

    // Capture initial state on first evaluation
    if (!m_has_initial_state) {
        m_initial_config = m_post_process->get_config();
        m_has_initial_state = true;
    }

    // Sample post-process values at time
    PostProcessKeyframe sampled = sample(time);

    // Get current config and modify animated values
    render::PostProcessConfig config = m_post_process->get_config();

    // Apply animated values
    config.tonemapping.exposure = sampled.exposure;
    config.bloom.threshold = sampled.bloom_threshold;
    config.bloom.intensity = sampled.bloom_intensity;
    config.vignette_intensity = sampled.vignette_intensity;
    config.vignette_smoothness = sampled.vignette_smoothness;
    config.ca_intensity = sampled.ca_intensity;

    // Enable/disable features based on intensity
    config.vignette_enabled = sampled.vignette_intensity > 0.0f;
    config.chromatic_aberration = sampled.ca_intensity > 0.0f;

    m_post_process->set_config(config);
}

void PostProcessTrack::reset() {
    if (m_has_initial_state && m_post_process) {
        m_post_process->set_config(m_initial_config);
    }
    m_has_initial_state = false;
}

PostProcessKeyframe PostProcessTrack::sample(float time) const {
    if (m_keyframes.empty()) {
        return PostProcessKeyframe{};
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
    const PostProcessKeyframe& k0 = m_keyframes[index];
    const PostProcessKeyframe& k1 = m_keyframes[index + 1];

    // Calculate interpolation factor
    float segment_duration = k1.time - k0.time;
    float t = (time - k0.time) / segment_duration;

    // Apply easing
    t = apply_easing(t, k0.easing);

    // Interpolate
    PostProcessKeyframe result;
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
            result.exposure = interpolate_linear(k0.exposure, k1.exposure, t);
            result.bloom_threshold = interpolate_linear(k0.bloom_threshold, k1.bloom_threshold, t);
            result.bloom_intensity = interpolate_linear(k0.bloom_intensity, k1.bloom_intensity, t);
            result.vignette_intensity = interpolate_linear(k0.vignette_intensity, k1.vignette_intensity, t);
            result.vignette_smoothness = interpolate_linear(k0.vignette_smoothness, k1.vignette_smoothness, t);
            result.ca_intensity = interpolate_linear(k0.ca_intensity, k1.ca_intensity, t);
            break;
    }

    return result;
}

void PostProcessTrack::sort_keyframes() {
    std::sort(m_keyframes.begin(), m_keyframes.end(),
        [](const PostProcessKeyframe& a, const PostProcessKeyframe& b) {
            return a.time < b.time;
        });
}

size_t PostProcessTrack::find_keyframe_index(float time) const {
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

void PostProcessTrack::serialize(nlohmann::json& j) const {
    j["keyframes"] = nlohmann::json::array();
    for (const auto& kf : m_keyframes) {
        j["keyframes"].push_back({
            {"time", kf.time},
            {"exposure", kf.exposure},
            {"bloom_threshold", kf.bloom_threshold},
            {"bloom_intensity", kf.bloom_intensity},
            {"vignette_intensity", kf.vignette_intensity},
            {"vignette_smoothness", kf.vignette_smoothness},
            {"ca_intensity", kf.ca_intensity},
            {"interpolation", static_cast<int>(kf.interpolation)},
            {"easing", static_cast<int>(kf.easing)}
        });
    }
}

void PostProcessTrack::deserialize(const nlohmann::json& j) {
    m_keyframes.clear();

    if (j.contains("keyframes")) {
        for (const auto& kf_json : j["keyframes"]) {
            PostProcessKeyframe kf;
            kf.time = kf_json.value("time", 0.0f);
            kf.exposure = kf_json.value("exposure", 1.0f);
            kf.bloom_threshold = kf_json.value("bloom_threshold", 1.0f);
            kf.bloom_intensity = kf_json.value("bloom_intensity", 0.5f);
            kf.vignette_intensity = kf_json.value("vignette_intensity", 0.0f);
            kf.vignette_smoothness = kf_json.value("vignette_smoothness", 0.5f);
            kf.ca_intensity = kf_json.value("ca_intensity", 0.0f);
            kf.interpolation = static_cast<InterpolationMode>(kf_json.value("interpolation", 0));
            kf.easing = static_cast<EaseType>(kf_json.value("easing", 0));
            m_keyframes.push_back(kf);
        }
    }
}

} // namespace engine::cinematic
