#pragma once

#include <engine/cinematic/track.hpp>
#include <engine/render/post_process.hpp>
#include <cassert>

namespace engine::cinematic {

// Post-process keyframe matching render::PostProcessConfig
struct PostProcessKeyframe : KeyframeBase {
    // Tonemapping
    float exposure = 1.0f;

    // Bloom
    float bloom_threshold = 1.0f;
    float bloom_intensity = 0.5f;

    // Vignette
    float vignette_intensity = 0.0f;
    float vignette_smoothness = 0.5f;

    // Chromatic aberration
    float ca_intensity = 0.0f;

    PostProcessKeyframe() = default;
    PostProcessKeyframe(float t, float exp, float bloom)
        : exposure(exp), bloom_intensity(bloom) { time = t; }
};

// Post-process track for animating post-processing effects during cinematics
class PostProcessTrack : public Track {
public:
    explicit PostProcessTrack(const std::string& name);
    ~PostProcessTrack() override = default;

    // Set target post-process system
    void set_post_process_system(render::PostProcessSystem* system) { m_post_process = system; }

    // Keyframe management
    void add_keyframe(const PostProcessKeyframe& keyframe);
    void remove_keyframe(size_t index);
    void clear_keyframes();

    size_t keyframe_count() const { return m_keyframes.size(); }
    PostProcessKeyframe& get_keyframe(size_t index) {
        assert(index < m_keyframes.size() && "Keyframe index out of bounds");
        return m_keyframes[index];
    }
    const PostProcessKeyframe& get_keyframe(size_t index) const {
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
    PostProcessKeyframe sample(float time) const;

private:
    void sort_keyframes();
    size_t find_keyframe_index(float time) const;

    std::vector<PostProcessKeyframe> m_keyframes;
    render::PostProcessSystem* m_post_process = nullptr;

    // Initial state for reset
    render::PostProcessConfig m_initial_config;
    bool m_has_initial_state = false;
};

} // namespace engine::cinematic
