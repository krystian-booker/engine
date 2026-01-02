#include <engine/cinematic/event_track.hpp>
#include <engine/scene/world.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>

namespace engine::cinematic {

// Static member
std::unordered_map<std::string, SequenceEventHandler> EventTrack::s_global_handlers;

// ============================================================================
// EventTrack
// ============================================================================

EventTrack::EventTrack(const std::string& name)
    : Track(name, TrackType::Event) {
}

void EventTrack::add_event(const SequenceEvent& event) {
    m_events.push_back(event);
    m_event_fired.push_back(false);
    sort_events();
}

void EventTrack::remove_event(size_t index) {
    if (index < m_events.size()) {
        m_events.erase(m_events.begin() + index);
        m_event_fired.erase(m_event_fired.begin() + index);
    }
}

void EventTrack::clear_events() {
    m_events.clear();
    m_event_fired.clear();
}

void EventTrack::register_global_handler(const std::string& event_name, SequenceEventHandler handler) {
    s_global_handlers[event_name] = std::move(handler);
}

void EventTrack::unregister_global_handler(const std::string& event_name) {
    s_global_handlers.erase(event_name);
}

float EventTrack::get_duration() const {
    if (m_events.empty()) {
        return 0.0f;
    }
    return m_events.back().time;
}

void EventTrack::evaluate(float time, scene::World& /*world*/) {
    if (!m_enabled) {
        return;
    }

    // Handle seeking backwards
    if (time < m_last_time) {
        for (size_t i = 0; i < m_events.size(); ++i) {
            if (m_events[i].time > time) {
                m_event_fired[i] = false;
            }
        }
    }

    // Fire events in range
    for (size_t i = 0; i < m_events.size(); ++i) {
        if (!m_event_fired[i] && m_events[i].time > m_last_time && m_events[i].time <= time) {
            fire_event(m_events[i]);
            m_event_fired[i] = true;
        }
    }

    m_last_time = time;
}

void EventTrack::reset() {
    std::fill(m_event_fired.begin(), m_event_fired.end(), false);
    m_last_time = -1.0f;
}

std::vector<const SequenceEvent*> EventTrack::get_events_in_range(float from, float to) const {
    std::vector<const SequenceEvent*> result;
    for (const auto& event : m_events) {
        if (event.time >= from && event.time < to) {
            result.push_back(&event);
        }
    }
    return result;
}

void EventTrack::sort_events() {
    // Sort events and fired flags together
    std::vector<std::pair<SequenceEvent, bool>> combined;
    for (size_t i = 0; i < m_events.size(); ++i) {
        combined.emplace_back(m_events[i], m_event_fired[i]);
    }

    std::sort(combined.begin(), combined.end(),
        [](const auto& a, const auto& b) {
            return a.first.time < b.first.time;
        });

    for (size_t i = 0; i < combined.size(); ++i) {
        m_events[i] = combined[i].first;
        m_event_fired[i] = combined[i].second;
    }
}

void EventTrack::fire_event(const SequenceEvent& event) {
    // Call track-specific handler
    if (m_handler) {
        m_handler(event);
    }

    // Call global handlers
    auto it = s_global_handlers.find(event.event_name);
    if (it != s_global_handlers.end() && it->second) {
        it->second(event);
    }

    // Wildcard handler
    auto wildcard_it = s_global_handlers.find("*");
    if (wildcard_it != s_global_handlers.end() && wildcard_it->second) {
        wildcard_it->second(event);
    }
}

// ============================================================================
// SubtitleTrack
// ============================================================================

SubtitleTrack::SubtitleTrack(const std::string& name)
    : Track(name, TrackType::Event) {
}

void SubtitleTrack::add_subtitle(const Subtitle& subtitle) {
    m_subtitles.push_back(subtitle);
    std::sort(m_subtitles.begin(), m_subtitles.end(),
        [](const Subtitle& a, const Subtitle& b) {
            return a.start_time < b.start_time;
        });
}

void SubtitleTrack::clear_subtitles() {
    m_subtitles.clear();
}

const SubtitleTrack::Subtitle* SubtitleTrack::get_active_subtitle(float time) const {
    for (const auto& subtitle : m_subtitles) {
        if (time >= subtitle.start_time && time < subtitle.start_time + subtitle.duration) {
            return &subtitle;
        }
    }
    return nullptr;
}

float SubtitleTrack::get_duration() const {
    float duration = 0.0f;
    for (const auto& subtitle : m_subtitles) {
        float end = subtitle.start_time + subtitle.duration;
        duration = std::max(duration, end);
    }
    return duration;
}

void SubtitleTrack::evaluate(float time, scene::World& /*world*/) {
    if (!m_enabled) {
        return;
    }

    const Subtitle* active = get_active_subtitle(time);

    if (active != m_current_subtitle) {
        // Subtitle changed
        if (m_callback) {
            if (m_current_subtitle) {
                m_callback(m_current_subtitle, false); // Hide old
            }
            if (active) {
                m_callback(active, true); // Show new
            }
        }
        m_current_subtitle = active;
    }
}

void SubtitleTrack::reset() {
    if (m_callback && m_current_subtitle) {
        m_callback(m_current_subtitle, false);
    }
    m_current_subtitle = nullptr;
}

// ============================================================================
// EventTrack Serialization
// ============================================================================

// Helper to serialize variant payload
static nlohmann::json serialize_payload(const EventPayload& payload) {
    nlohmann::json j;
    std::visit([&j](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            j["type"] = "none";
        } else if constexpr (std::is_same_v<T, bool>) {
            j["type"] = "bool";
            j["value"] = arg;
        } else if constexpr (std::is_same_v<T, int>) {
            j["type"] = "int";
            j["value"] = arg;
        } else if constexpr (std::is_same_v<T, float>) {
            j["type"] = "float";
            j["value"] = arg;
        } else if constexpr (std::is_same_v<T, std::string>) {
            j["type"] = "string";
            j["value"] = arg;
        } else if constexpr (std::is_same_v<T, Vec3>) {
            j["type"] = "vec3";
            j["value"] = {arg.x, arg.y, arg.z};
        } else if constexpr (std::is_same_v<T, scene::Entity>) {
            j["type"] = "entity";
            j["value"] = static_cast<uint32_t>(arg);
        }
    }, payload);
    return j;
}

// Helper to deserialize variant payload
static EventPayload deserialize_payload(const nlohmann::json& j) {
    std::string type = j.value("type", "none");
    if (type == "bool") {
        return j.value("value", false);
    } else if (type == "int") {
        return j.value("value", 0);
    } else if (type == "float") {
        return j.value("value", 0.0f);
    } else if (type == "string") {
        return j.value("value", std::string{});
    } else if (type == "vec3") {
        auto& v = j["value"];
        return Vec3{v[0], v[1], v[2]};
    } else if (type == "entity") {
        return scene::Entity{j.value("value", 0u)};
    }
    return std::monostate{};
}

void EventTrack::serialize(nlohmann::json& j) const {
    j["events"] = nlohmann::json::array();
    for (const auto& event : m_events) {
        j["events"].push_back({
            {"time", event.time},
            {"event_name", event.event_name},
            {"payload", serialize_payload(event.payload)},
            {"target", static_cast<uint32_t>(event.target)}
        });
    }
}

void EventTrack::deserialize(const nlohmann::json& j) {
    m_events.clear();
    m_event_fired.clear();

    if (j.contains("events")) {
        for (const auto& event_json : j["events"]) {
            SequenceEvent event;
            event.time = event_json.value("time", 0.0f);
            event.event_name = event_json.value("event_name", "");
            if (event_json.contains("payload")) {
                event.payload = deserialize_payload(event_json["payload"]);
            }
            event.target = scene::Entity{event_json.value("target", 0u)};
            m_events.push_back(event);
            m_event_fired.push_back(false);
        }
    }
}

// ============================================================================
// SubtitleTrack Serialization
// ============================================================================

void SubtitleTrack::serialize(nlohmann::json& j) const {
    j["subtitles"] = nlohmann::json::array();
    for (const auto& subtitle : m_subtitles) {
        j["subtitles"].push_back({
            {"start_time", subtitle.start_time},
            {"duration", subtitle.duration},
            {"text", subtitle.text},
            {"speaker", subtitle.speaker},
            {"style", subtitle.style}
        });
    }
}

void SubtitleTrack::deserialize(const nlohmann::json& j) {
    m_subtitles.clear();

    if (j.contains("subtitles")) {
        for (const auto& sub_json : j["subtitles"]) {
            Subtitle subtitle;
            subtitle.start_time = sub_json.value("start_time", 0.0f);
            subtitle.duration = sub_json.value("duration", 0.0f);
            subtitle.text = sub_json.value("text", "");
            subtitle.speaker = sub_json.value("speaker", "");
            subtitle.style = sub_json.value("style", "");
            m_subtitles.push_back(subtitle);
        }
    }
}

} // namespace engine::cinematic
