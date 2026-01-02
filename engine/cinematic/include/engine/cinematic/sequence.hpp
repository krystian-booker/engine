#pragma once

#include <engine/cinematic/track.hpp>
#include <engine/cinematic/camera_track.hpp>
#include <engine/cinematic/animation_track.hpp>
#include <engine/cinematic/audio_track.hpp>
#include <engine/cinematic/event_track.hpp>
#include <engine/cinematic/light_track.hpp>
#include <engine/cinematic/postprocess_track.hpp>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

namespace engine::cinematic {

// Sequence metadata
struct SequenceInfo {
    std::string name;
    std::string description;
    std::string author;
    float frame_rate = 30.0f;       // For editor display
    bool is_looping = false;
};

// Sequence group (for organizing tracks)
struct TrackGroup {
    std::string name;
    std::vector<Track*> tracks;
    bool collapsed = false;
    bool muted = false;
};

// Main cinematic sequence class
class Sequence {
public:
    Sequence();
    explicit Sequence(const std::string& name);
    ~Sequence();

    // Sequence info
    const SequenceInfo& get_info() const { return m_info; }
    SequenceInfo& get_info() { return m_info; }
    const std::string& get_name() const { return m_info.name; }
    void set_name(const std::string& name) { m_info.name = name; }

    // Duration (max of all tracks)
    float get_duration() const;

    // Track management - typed factory methods
    CameraTrack* add_camera_track(const std::string& name);
    AnimationTrack* add_animation_track(const std::string& name);
    TransformTrack* add_transform_track(const std::string& name);
    AudioTrack* add_audio_track(const std::string& name);
    MusicTrack* add_music_track(const std::string& name);
    EventTrack* add_event_track(const std::string& name);
    SubtitleTrack* add_subtitle_track(const std::string& name);
    LightTrack* add_light_track(const std::string& name);
    PostProcessTrack* add_postprocess_track(const std::string& name);

    // Generic track access
    Track* get_track(const std::string& name);
    const Track* get_track(const std::string& name) const;

    template<typename T>
    T* get_track_as(const std::string& name) {
        return dynamic_cast<T*>(get_track(name));
    }

    void remove_track(const std::string& name);
    void clear_tracks();

    size_t track_count() const { return m_tracks.size(); }
    const std::vector<std::unique_ptr<Track>>& get_tracks() const { return m_tracks; }

    // Track groups
    TrackGroup* create_group(const std::string& name);
    TrackGroup* get_group(const std::string& name);
    void add_track_to_group(Track* track, const std::string& group_name);
    void remove_group(const std::string& name);

    // Evaluate all tracks at time
    void evaluate(float time, scene::World& world);

    // Reset all tracks
    void reset();

    // Markers (named points in time)
    void add_marker(const std::string& name, float time);
    void remove_marker(const std::string& name);
    float get_marker_time(const std::string& name) const;
    const std::unordered_map<std::string, float>& get_markers() const { return m_markers; }

    // Sections (named time ranges)
    struct Section {
        std::string name;
        float start_time;
        float end_time;
        uint32_t color = 0xFFFFFFFF; // For editor display
    };

    void add_section(const Section& section);
    const Section* get_section(const std::string& name) const;
    const std::vector<Section>& get_sections() const { return m_sections; }

    // Serialization
    bool save(const std::string& path) const;
    bool load(const std::string& path);

private:
    SequenceInfo m_info;
    std::vector<std::unique_ptr<Track>> m_tracks;
    std::unordered_map<std::string, Track*> m_track_lookup;
    std::vector<TrackGroup> m_groups;
    std::unordered_map<std::string, float> m_markers;
    std::vector<Section> m_sections;
};

} // namespace engine::cinematic
