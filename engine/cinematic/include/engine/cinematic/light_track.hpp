#pragma once

#include <engine/cinematic/track.hpp>
#include <engine/scene/entity.hpp>
#include <cassert>

namespace engine::cinematic {

// Light keyframe matching scene::Light component
struct LightKeyframe : KeyframeBase {
    Vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float range = 10.0f;             // Point/Spot only
    float spot_inner_angle = 30.0f;  // Spot only (degrees)
    float spot_outer_angle = 45.0f;  // Spot only (degrees)

    LightKeyframe() = default;
    LightKeyframe(float t, const Vec3& col, float intens)
        : color(col), intensity(intens) { time = t; }
};

// Light track for animating light properties during cinematics
class LightTrack : public Track {
public:
    explicit LightTrack(const std::string& name);
    ~LightTrack() override = default;

    // Set target light entity (must have Light component)
    void set_target_entity(scene::Entity entity) { m_target_entity = entity; }
    scene::Entity get_target_entity() const { return m_target_entity; }

    // Keyframe management
    void add_keyframe(const LightKeyframe& keyframe);
    void remove_keyframe(size_t index);
    void clear_keyframes();

    size_t keyframe_count() const { return m_keyframes.size(); }
    LightKeyframe& get_keyframe(size_t index) {
        assert(index < m_keyframes.size() && "Keyframe index out of bounds");
        return m_keyframes[index];
    }
    const LightKeyframe& get_keyframe(size_t index) const {
        assert(index < m_keyframes.size() && "Keyframe index out of bounds");
        return m_keyframes[index];
    }

    // Track interface
    float get_duration() const override;
    void evaluate(float time, scene::World& world) override;
    void reset() override;

    // Serialization
    void serialize(nlohmann::json& j) const override;
    void deserialize(const nlohmann::json& j) override;

    // Get interpolated values at time
    LightKeyframe sample(float time) const;

private:
    void sort_keyframes();
    size_t find_keyframe_index(float time) const;

    std::vector<LightKeyframe> m_keyframes;
    scene::Entity m_target_entity = scene::NullEntity;
    scene::World* m_world = nullptr;

    // Initial state for reset
    LightKeyframe m_initial_state;
    bool m_has_initial_state = false;
};

} // namespace engine::cinematic
