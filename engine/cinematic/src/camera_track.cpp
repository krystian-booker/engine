#include <engine/cinematic/camera_track.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/scene/render_components.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>

namespace engine::cinematic {

CameraTrack::CameraTrack(const std::string& name)
    : Track(name, TrackType::Camera) {
}

void CameraTrack::add_keyframe(const CameraKeyframe& keyframe) {
    m_keyframes.push_back(keyframe);
    sort_keyframes();
}

void CameraTrack::remove_keyframe(size_t index) {
    if (index < m_keyframes.size()) {
        m_keyframes.erase(m_keyframes.begin() + index);
    }
}

void CameraTrack::clear_keyframes() {
    m_keyframes.clear();
}

void CameraTrack::add_shake(float start_time, const CameraShake& shake) {
    m_shakes.emplace_back(start_time, shake);
    std::sort(m_shakes.begin(), m_shakes.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
}

float CameraTrack::get_duration() const {
    if (m_keyframes.empty()) {
        return 0.0f;
    }
    float duration = m_keyframes.back().time;

    // Include shakes
    for (const auto& [time, shake] : m_shakes) {
        float shake_end = time + shake.duration;
        duration = std::max(duration, shake_end);
    }

    return duration;
}

void CameraTrack::evaluate(float time, scene::World& world) {
    if (!m_enabled || m_keyframes.empty()) {
        return;
    }

    // Store world reference for reset
    m_world = &world;

    // Capture initial state on first evaluation
    if (!m_has_initial_state && m_target_camera != scene::NullEntity) {
        if (world.has<scene::LocalTransform>(m_target_camera)) {
            const auto& t = world.get<scene::LocalTransform>(m_target_camera);
            m_initial_state.position = t.position;
            m_initial_state.rotation = t.rotation;
        }
        if (world.has<scene::Camera>(m_target_camera)) {
            const auto& c = world.get<scene::Camera>(m_target_camera);
            m_initial_state.fov = c.fov;
            m_initial_state.near_plane = c.near_plane;
            m_initial_state.far_plane = c.far_plane;
        }
        m_has_initial_state = true;
    }

    // Sample camera values at time
    CameraKeyframe sampled = sample(time);

    // Apply shake
    sampled.position = apply_shake(sampled.position, time);

    // Apply to target camera entity
    if (m_target_camera != scene::NullEntity) {
        // Apply position/rotation to LocalTransform
        if (world.has<scene::LocalTransform>(m_target_camera)) {
            auto& transform = world.get<scene::LocalTransform>(m_target_camera);
            transform.position = sampled.position;
            transform.rotation = sampled.rotation;
        }
        // Apply camera-specific values to Camera component
        if (world.has<scene::Camera>(m_target_camera)) {
            auto& camera = world.get<scene::Camera>(m_target_camera);
            camera.fov = sampled.fov;
            camera.near_plane = sampled.near_plane;
            camera.far_plane = sampled.far_plane;
        }
    }
}

void CameraTrack::reset() {
    if (m_has_initial_state && m_target_camera != scene::NullEntity && m_world) {
        if (m_world->has<scene::LocalTransform>(m_target_camera)) {
            auto& transform = m_world->get<scene::LocalTransform>(m_target_camera);
            transform.position = m_initial_state.position;
            transform.rotation = m_initial_state.rotation;
        }
        if (m_world->has<scene::Camera>(m_target_camera)) {
            auto& camera = m_world->get<scene::Camera>(m_target_camera);
            camera.fov = m_initial_state.fov;
            camera.near_plane = m_initial_state.near_plane;
            camera.far_plane = m_initial_state.far_plane;
        }
    }
    m_has_initial_state = false;
}

CameraKeyframe CameraTrack::sample(float time) const {
    if (m_keyframes.empty()) {
        return CameraKeyframe{};
    }

    // Before first keyframe
    if (time <= m_keyframes.front().time) {
        CameraKeyframe result = m_keyframes.front();
        // Apply rail transform if needed
        if (m_rail_type == CameraRailType::Orbit) {
            result.position = compute_orbit_position(result.position);
        }
        return result;
    }

    // After last keyframe
    if (time >= m_keyframes.back().time) {
        CameraKeyframe result = m_keyframes.back();
        if (m_rail_type == CameraRailType::Orbit) {
            result.position = compute_orbit_position(result.position);
        }
        return result;
    }

    // Find surrounding keyframes
    size_t index = find_keyframe_index(time);
    const CameraKeyframe& k0 = m_keyframes[index];
    const CameraKeyframe& k1 = m_keyframes[index + 1];

    // Calculate interpolation factor
    float segment_duration = k1.time - k0.time;
    float t = (time - k0.time) / segment_duration;

    // Apply easing
    t = apply_easing(t, k0.easing);

    // Interpolate based on mode
    CameraKeyframe result;
    result.time = time;

    // Handle Spline rail type - use Catmull-Rom across all keyframes
    if (m_rail_type == CameraRailType::Spline && m_keyframes.size() >= 2) {
        // Calculate global t based on overall duration
        float duration = m_keyframes.back().time - m_keyframes.front().time;
        float global_t = (time - m_keyframes.front().time) / duration;

        // Find the segment we're in based on global_t
        float segment_t = global_t * (m_keyframes.size() - 1);
        size_t seg_index = static_cast<size_t>(segment_t);
        seg_index = std::min(seg_index, m_keyframes.size() - 2);
        float local_t = segment_t - seg_index;

        // Get 4 keyframes for Catmull-Rom
        size_t i0 = seg_index > 0 ? seg_index - 1 : 0;
        size_t i1 = seg_index;
        size_t i2 = seg_index + 1;
        size_t i3 = std::min(seg_index + 2, m_keyframes.size() - 1);

        result.position = evaluate_catmull_rom(
            m_keyframes[i0].position,
            m_keyframes[i1].position,
            m_keyframes[i2].position,
            m_keyframes[i3].position,
            local_t
        );
        result.rotation = interpolate_linear(m_keyframes[i1].rotation, m_keyframes[i2].rotation, local_t);
        result.fov = interpolate_linear(m_keyframes[i1].fov, m_keyframes[i2].fov, local_t);
        result.near_plane = interpolate_linear(m_keyframes[i1].near_plane, m_keyframes[i2].near_plane, local_t);
        result.far_plane = interpolate_linear(m_keyframes[i1].far_plane, m_keyframes[i2].far_plane, local_t);
        return result;
    }

    switch (k0.interpolation) {
        case InterpolationMode::Step:
            result = k0;
            result.time = time;
            break;

        case InterpolationMode::Linear:
            result.position = interpolate_linear(k0.position, k1.position, t);
            result.rotation = interpolate_linear(k0.rotation, k1.rotation, t);
            result.fov = interpolate_linear(k0.fov, k1.fov, t);
            result.near_plane = interpolate_linear(k0.near_plane, k1.near_plane, t);
            result.far_plane = interpolate_linear(k0.far_plane, k1.far_plane, t);
            result.focus_distance = interpolate_linear(k0.focus_distance, k1.focus_distance, t);
            result.aperture = interpolate_linear(k0.aperture, k1.aperture, t);
            break;

        case InterpolationMode::Bezier:
            result.position = interpolate_bezier(k0.position, k1.position,
                                                  k0.tangent_out, k1.tangent_in, t);
            result.rotation = interpolate_linear(k0.rotation, k1.rotation, t);
            result.fov = interpolate_linear(k0.fov, k1.fov, t);
            break;

        case InterpolationMode::CatmullRom: {
            // Get 4 keyframes for Catmull-Rom
            size_t i0 = index > 0 ? index - 1 : 0;
            size_t i1 = index;
            size_t i2 = index + 1;
            size_t i3 = std::min(index + 2, m_keyframes.size() - 1);

            result.position = evaluate_catmull_rom(
                m_keyframes[i0].position,
                m_keyframes[i1].position,
                m_keyframes[i2].position,
                m_keyframes[i3].position,
                t
            );
            result.rotation = interpolate_linear(k0.rotation, k1.rotation, t);
            result.fov = interpolate_linear(k0.fov, k1.fov, t);
            break;
        }
    }

    // Apply Orbit rail transform if active
    // For Orbit: keyframe position.x = angle (radians), position.y = height, position.z = radius
    if (m_rail_type == CameraRailType::Orbit) {
        result.position = compute_orbit_position(result.position);
    }

    return result;
}

void CameraTrack::sort_keyframes() {
    std::sort(m_keyframes.begin(), m_keyframes.end(),
        [](const CameraKeyframe& a, const CameraKeyframe& b) {
            return a.time < b.time;
        });
}

size_t CameraTrack::find_keyframe_index(float time) const {
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

Vec3 CameraTrack::apply_shake(const Vec3& position, float time) const {
    Vec3 result = position;

    for (const auto& [start_time, shake] : m_shakes) {
        if (time >= start_time && time < start_time + shake.duration) {
            float shake_time = time - start_time;
            float falloff = 1.0f - std::pow(shake_time / shake.duration, shake.falloff);

            // Generate shake offset using sin/cos at different frequencies
            float phase = shake_time * shake.frequency * 6.28318f;
            Vec3 offset;
            offset.x = std::sin(phase) * shake.direction.x;
            offset.y = std::sin(phase * 1.3f + 1.0f) * shake.direction.y;
            offset.z = std::sin(phase * 0.7f + 2.0f) * shake.direction.z;

            result += offset * shake.amplitude * falloff;
        }
    }

    return result;
}

Vec3 CameraTrack::compute_orbit_position(const Vec3& orbit_params) const {
    // orbit_params: x = angle (radians), y = height offset, z = radius
    float angle = orbit_params.x;
    float height = orbit_params.y;
    float radius = orbit_params.z;

    Vec3 position;
    position.x = m_orbit_center.x + radius * std::cos(angle);
    position.y = m_orbit_center.y + height;
    position.z = m_orbit_center.z + radius * std::sin(angle);

    return position;
}

void CameraTrack::serialize(nlohmann::json& j) const {
    j["keyframes"] = nlohmann::json::array();
    for (const auto& kf : m_keyframes) {
        j["keyframes"].push_back({
            {"time", kf.time},
            {"position", {kf.position.x, kf.position.y, kf.position.z}},
            {"rotation", {kf.rotation.w, kf.rotation.x, kf.rotation.y, kf.rotation.z}},
            {"fov", kf.fov},
            {"near_plane", kf.near_plane},
            {"far_plane", kf.far_plane},
            {"focus_distance", kf.focus_distance},
            {"aperture", kf.aperture},
            {"interpolation", static_cast<int>(kf.interpolation)},
            {"easing", static_cast<int>(kf.easing)}
        });
    }

    j["shakes"] = nlohmann::json::array();
    for (const auto& [time, shake] : m_shakes) {
        j["shakes"].push_back({
            {"time", time},
            {"amplitude", shake.amplitude},
            {"frequency", shake.frequency},
            {"direction", {shake.direction.x, shake.direction.y, shake.direction.z}},
            {"duration", shake.duration},
            {"falloff", shake.falloff}
        });
    }

    j["rail_type"] = static_cast<int>(m_rail_type);
    j["orbit_center"] = {m_orbit_center.x, m_orbit_center.y, m_orbit_center.z};
}

void CameraTrack::deserialize(const nlohmann::json& j) {
    m_keyframes.clear();
    m_shakes.clear();

    if (j.contains("keyframes")) {
        for (const auto& kf_json : j["keyframes"]) {
            CameraKeyframe kf;
            kf.time = kf_json.value("time", 0.0f);
            if (kf_json.contains("position")) {
                auto& pos = kf_json["position"];
                kf.position = Vec3{pos[0], pos[1], pos[2]};
            }
            if (kf_json.contains("rotation")) {
                auto& rot = kf_json["rotation"];
                kf.rotation = Quat{rot[0], rot[1], rot[2], rot[3]};
            }
            kf.fov = kf_json.value("fov", 60.0f);
            kf.near_plane = kf_json.value("near_plane", 0.1f);
            kf.far_plane = kf_json.value("far_plane", 1000.0f);
            kf.focus_distance = kf_json.value("focus_distance", 10.0f);
            kf.aperture = kf_json.value("aperture", 2.8f);
            kf.interpolation = static_cast<InterpolationMode>(kf_json.value("interpolation", 0));
            kf.easing = static_cast<EaseType>(kf_json.value("easing", 0));
            m_keyframes.push_back(kf);
        }
    }

    if (j.contains("shakes")) {
        for (const auto& shake_json : j["shakes"]) {
            CameraShake shake;
            float time = shake_json.value("time", 0.0f);
            shake.amplitude = shake_json.value("amplitude", 0.0f);
            shake.frequency = shake_json.value("frequency", 10.0f);
            if (shake_json.contains("direction")) {
                auto& dir = shake_json["direction"];
                shake.direction = Vec3{dir[0], dir[1], dir[2]};
            }
            shake.duration = shake_json.value("duration", 0.0f);
            shake.falloff = shake_json.value("falloff", 1.0f);
            m_shakes.emplace_back(time, shake);
        }
    }

    m_rail_type = static_cast<CameraRailType>(j.value("rail_type", 0));

    if (j.contains("orbit_center")) {
        auto& center = j["orbit_center"];
        m_orbit_center = Vec3{center[0], center[1], center[2]};
    }
}

} // namespace engine::cinematic
