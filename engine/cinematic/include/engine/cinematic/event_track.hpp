#pragma once

#include <engine/cinematic/track.hpp>
#include <engine/scene/entity.hpp>
#include <functional>
#include <any>
#include <variant>

namespace engine::cinematic {

// Event payload types
using EventPayload = std::variant<
    std::monostate,     // No data
    bool,
    int,
    float,
    std::string,
    Vec3,
    scene::Entity
>;

// Generic event on timeline
struct SequenceEvent {
    float time = 0.0f;
    std::string event_name;
    EventPayload payload;

    // Optional target entity
    scene::Entity target = scene::NullEntity;

    SequenceEvent() = default;
    SequenceEvent(float t, const std::string& name) : time(t), event_name(name) {}

    template<typename T>
    SequenceEvent(float t, const std::string& name, const T& data)
        : time(t), event_name(name), payload(data) {}
};

// Event handler callback
using SequenceEventHandler = std::function<void(const SequenceEvent&)>;

// Event track for triggering game events during cinematics
class EventTrack : public Track {
public:
    explicit EventTrack(const std::string& name);
    ~EventTrack() override = default;

    // Add events
    void add_event(const SequenceEvent& event);
    void remove_event(size_t index);
    void clear_events();

    size_t event_count() const { return m_events.size(); }
    SequenceEvent& get_event(size_t index) { return m_events[index]; }
    const SequenceEvent& get_event(size_t index) const { return m_events[index]; }

    // Register event handlers
    void set_handler(SequenceEventHandler handler) { m_handler = std::move(handler); }

    // Static global handlers (for common events)
    static void register_global_handler(const std::string& event_name, SequenceEventHandler handler);
    static void unregister_global_handler(const std::string& event_name);

    // Track interface
    float get_duration() const override;
    void evaluate(float time) override;
    void reset() override;

    // Get events in time range
    std::vector<const SequenceEvent*> get_events_in_range(float from, float to) const;

private:
    void sort_events();
    void fire_event(const SequenceEvent& event);

    std::vector<SequenceEvent> m_events;
    SequenceEventHandler m_handler;

    std::vector<bool> m_event_fired;
    float m_last_time = -1.0f;

    static std::unordered_map<std::string, SequenceEventHandler> s_global_handlers;
};

// Common event names
namespace SequenceEvents {
    constexpr const char* CUTSCENE_START = "cutscene_start";
    constexpr const char* CUTSCENE_END = "cutscene_end";
    constexpr const char* DIALOGUE_START = "dialogue_start";
    constexpr const char* DIALOGUE_END = "dialogue_end";
    constexpr const char* SUBTITLE = "subtitle";
    constexpr const char* SPAWN_ENTITY = "spawn_entity";
    constexpr const char* DESTROY_ENTITY = "destroy_entity";
    constexpr const char* ENABLE_ENTITY = "enable_entity";
    constexpr const char* DISABLE_ENTITY = "disable_entity";
    constexpr const char* TRIGGER_ANIMATION = "trigger_animation";
    constexpr const char* SET_VARIABLE = "set_variable";
    constexpr const char* BRANCH = "branch";
    constexpr const char* SLOW_MOTION_START = "slow_motion_start";
    constexpr const char* SLOW_MOTION_END = "slow_motion_end";
    constexpr const char* SCREEN_FADE = "screen_fade";
    constexpr const char* LETTERBOX_START = "letterbox_start";
    constexpr const char* LETTERBOX_END = "letterbox_end";
}

// Subtitle track (specialized event track)
class SubtitleTrack : public Track {
public:
    explicit SubtitleTrack(const std::string& name);
    ~SubtitleTrack() override = default;

    struct Subtitle {
        float start_time;
        float duration;
        std::string text;
        std::string speaker;   // Optional speaker name
        std::string style;     // Style/color hint
    };

    void add_subtitle(const Subtitle& subtitle);
    void clear_subtitles();

    // Get active subtitle at time
    const Subtitle* get_active_subtitle(float time) const;

    // Callback for UI integration
    using SubtitleCallback = std::function<void(const Subtitle*, bool show)>;
    void set_callback(SubtitleCallback callback) { m_callback = std::move(callback); }

    // Track interface
    float get_duration() const override;
    void evaluate(float time) override;
    void reset() override;

private:
    std::vector<Subtitle> m_subtitles;
    SubtitleCallback m_callback;
    const Subtitle* m_current_subtitle = nullptr;
};

} // namespace engine::cinematic
