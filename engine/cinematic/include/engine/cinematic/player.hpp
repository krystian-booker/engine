#pragma once

#include <engine/cinematic/sequence.hpp>
#include <engine/scene/world.hpp>
#include <functional>

namespace engine::cinematic {

// Playback state
enum class PlaybackState : uint8_t {
    Stopped,
    Playing,
    Paused
};

// Playback direction
enum class PlaybackDirection : uint8_t {
    Forward,
    Backward
};

// Playback events
enum class PlaybackEvent : uint8_t {
    Started,
    Paused,
    Resumed,
    Stopped,
    Finished,
    Looped,
    MarkerReached,
    SectionEntered,
    SectionExited
};

// Event callback
using PlaybackEventCallback = std::function<void(PlaybackEvent, const std::string& data)>;

// Sequence player - handles playback of cinematic sequences
class SequencePlayer {
public:
    SequencePlayer();
    ~SequencePlayer();

    // Load sequence
    void load(std::unique_ptr<Sequence> sequence);
    void load(const std::string& path);
    void unload();

    bool has_sequence() const { return m_sequence != nullptr; }
    Sequence* get_sequence() { return m_sequence.get(); }
    const Sequence* get_sequence() const { return m_sequence.get(); }

    // Playback control
    void play();
    void pause();
    void stop();
    void toggle_play_pause();

    // Seek
    void seek(float time);
    void seek_to_start();
    void seek_to_end();
    void seek_to_marker(const std::string& marker_name);

    // Frame stepping
    void step_forward();
    void step_backward();
    void set_frame_rate(float fps) { m_frame_rate = fps; }

    // Playback settings
    void set_playback_speed(float speed) { m_playback_speed = speed; }
    float get_playback_speed() const { return m_playback_speed; }

    void set_looping(bool loop) { m_looping = loop; }
    bool is_looping() const { return m_looping; }

    void set_direction(PlaybackDirection dir) { m_direction = dir; }
    PlaybackDirection get_direction() const { return m_direction; }

    // Restrict playback to a range
    void set_play_range(float start, float end);
    void clear_play_range();

    // State queries
    PlaybackState get_state() const { return m_state; }
    bool is_playing() const { return m_state == PlaybackState::Playing; }
    bool is_paused() const { return m_state == PlaybackState::Paused; }
    bool is_stopped() const { return m_state == PlaybackState::Stopped; }

    float get_current_time() const { return m_current_time; }
    float get_duration() const;
    float get_progress() const; // 0-1

    // Update (call every frame)
    void update(scene::World& world, float delta_time);

    // Event callbacks
    void set_event_callback(PlaybackEventCallback callback) { m_event_callback = std::move(callback); }

    // Skip/skip points (for player-controlled skipping)
    void enable_skipping(bool enable) { m_skip_enabled = enable; }
    void add_skip_point(float time);
    void skip_to_next_point();
    bool can_skip() const;

    // Blend in/out
    void set_blend_in_time(float time) { m_blend_in_time = time; }
    void set_blend_out_time(float time) { m_blend_out_time = time; }
    float get_blend_weight() const;

private:
    void fire_event(PlaybackEvent event, const std::string& data = "");
    void check_markers(float old_time, float new_time);
    void check_sections(float old_time, float new_time);

    std::unique_ptr<Sequence> m_sequence;
    PlaybackState m_state = PlaybackState::Stopped;
    PlaybackDirection m_direction = PlaybackDirection::Forward;

    float m_current_time = 0.0f;
    float m_playback_speed = 1.0f;
    float m_frame_rate = 30.0f;
    bool m_looping = false;

    // Play range
    bool m_use_play_range = false;
    float m_play_range_start = 0.0f;
    float m_play_range_end = 0.0f;

    // Blend in/out
    float m_blend_in_time = 0.0f;
    float m_blend_out_time = 0.0f;

    // Skip points
    bool m_skip_enabled = false;
    std::vector<float> m_skip_points;

    // Current section tracking
    std::string m_current_section;

    PlaybackEventCallback m_event_callback;
};

// Cinematic manager (singleton for managing multiple sequences)
class CinematicManager {
public:
    static CinematicManager& instance();

    // Sequence management
    void register_sequence(const std::string& name, std::unique_ptr<Sequence> sequence);
    void unregister_sequence(const std::string& name);
    Sequence* get_sequence(const std::string& name);

    // Quick play
    SequencePlayer* play_sequence(const std::string& name);
    void stop_all();

    // Active players
    SequencePlayer* get_active_player() { return m_active_player.get(); }

    // Update all active players
    void update(scene::World& world, float delta_time);

    // Preload sequences
    void preload(const std::string& path);
    void preload_async(const std::string& path);

private:
    CinematicManager() = default;

    std::unordered_map<std::string, std::unique_ptr<Sequence>> m_sequences;
    std::unique_ptr<SequencePlayer> m_active_player;
    std::vector<std::unique_ptr<SequencePlayer>> m_background_players;
};

} // namespace engine::cinematic
