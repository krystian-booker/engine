#pragma once

#include <engine/cinematic/track.hpp>
#include <engine/scene/entity.hpp>
#include <cassert>

namespace engine::cinematic {

// Camera keyframe with position, rotation, and FOV
struct CameraKeyframe : KeyframeBase {
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    float fov = 60.0f;
    float near_plane = 0.1f;
    float far_plane = 1000.0f;

    // Optional depth of field
    float focus_distance = 10.0f;
    float aperture = 2.8f;

    CameraKeyframe() = default;
    CameraKeyframe(float t, const Vec3& pos, const Quat& rot, float field_of_view)
        : position(pos), rotation(rot), fov(field_of_view) { time = t; }
};

// Camera shake parameters
struct CameraShake {
    float amplitude = 0.0f;
    float frequency = 10.0f;
    Vec3 direction{1.0f, 1.0f, 0.0f}; // Shake axes
    float duration = 0.0f;
    float falloff = 1.0f; // How quickly shake diminishes
};

// Rail types for camera movement
enum class CameraRailType : uint8_t {
    None,       // Free camera
    Spline,     // Follow a spline path
    Dolly,      // Straight line movement
    Orbit,      // Orbit around a point
    Track       // Follow entity at distance
};

// Camera track for cinematic camera control
class CameraTrack : public Track {
public:
    explicit CameraTrack(const std::string& name);
    ~CameraTrack() override = default;

    // Keyframe management
    void add_keyframe(const CameraKeyframe& keyframe);
    void remove_keyframe(size_t index);
    void clear_keyframes();

    size_t keyframe_count() const { return m_keyframes.size(); }
    CameraKeyframe& get_keyframe(size_t index) {
        assert(index < m_keyframes.size() && "Keyframe index out of bounds");
        return m_keyframes[index];
    }
    const CameraKeyframe& get_keyframe(size_t index) const {
        assert(index < m_keyframes.size() && "Keyframe index out of bounds");
        return m_keyframes[index];
    }

    // Set target camera entity
    void set_target_camera(scene::Entity camera) { m_target_camera = camera; }
    scene::Entity get_target_camera() const { return m_target_camera; }

    // Look-at target (optional)
    void set_look_at_target(scene::Entity target) { m_look_at_target = target; }
    void clear_look_at_target() { m_look_at_target = scene::NullEntity; }

    // Rail settings
    void set_rail_type(CameraRailType type) { m_rail_type = type; }
    CameraRailType get_rail_type() const { return m_rail_type; }

    // Camera shake
    void add_shake(float start_time, const CameraShake& shake);

    // Track interface
    float get_duration() const override;
    void evaluate(float time, scene::World& world) override;
    void reset() override;

    // Serialization
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;

    // Get interpolated values
    CameraKeyframe sample(float time) const;

private:
    void sort_keyframes();
    size_t find_keyframe_index(float time) const;
    Vec3 apply_shake(const Vec3& position, float time) const;

    std::vector<CameraKeyframe> m_keyframes;
    std::vector<std::pair<float, CameraShake>> m_shakes;

    scene::Entity m_target_camera = scene::NullEntity;
    scene::Entity m_look_at_target = scene::NullEntity;
    CameraRailType m_rail_type = CameraRailType::None;

    // Initial state for reset
    CameraKeyframe m_initial_state;
    bool m_has_initial_state = false;
};

} // namespace engine::cinematic
