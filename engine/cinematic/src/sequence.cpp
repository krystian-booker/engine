#include <engine/cinematic/sequence.hpp>
#include <engine/scene/world.hpp>
#include <engine/core/log.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

namespace engine::cinematic {

Sequence::Sequence()
    : Sequence("Untitled Sequence") {
}

Sequence::Sequence(const std::string& name) {
    m_info.name = name;
}

Sequence::~Sequence() = default;

float Sequence::get_duration() const {
    float duration = 0.0f;
    for (const auto& track : m_tracks) {
        duration = std::max(duration, track->get_duration());
    }
    return duration;
}

CameraTrack* Sequence::add_camera_track(const std::string& name) {
    auto track = std::make_unique<CameraTrack>(name);
    CameraTrack* ptr = track.get();
    m_track_lookup[name] = ptr;
    m_tracks.push_back(std::move(track));
    return ptr;
}

AnimationTrack* Sequence::add_animation_track(const std::string& name) {
    auto track = std::make_unique<AnimationTrack>(name);
    AnimationTrack* ptr = track.get();
    m_track_lookup[name] = ptr;
    m_tracks.push_back(std::move(track));
    return ptr;
}

TransformTrack* Sequence::add_transform_track(const std::string& name) {
    auto track = std::make_unique<TransformTrack>(name);
    TransformTrack* ptr = track.get();
    m_track_lookup[name] = ptr;
    m_tracks.push_back(std::move(track));
    return ptr;
}

AudioTrack* Sequence::add_audio_track(const std::string& name) {
    auto track = std::make_unique<AudioTrack>(name);
    AudioTrack* ptr = track.get();
    m_track_lookup[name] = ptr;
    m_tracks.push_back(std::move(track));
    return ptr;
}

MusicTrack* Sequence::add_music_track(const std::string& name) {
    auto track = std::make_unique<MusicTrack>(name);
    MusicTrack* ptr = track.get();
    m_track_lookup[name] = ptr;
    m_tracks.push_back(std::move(track));
    return ptr;
}

EventTrack* Sequence::add_event_track(const std::string& name) {
    auto track = std::make_unique<EventTrack>(name);
    EventTrack* ptr = track.get();
    m_track_lookup[name] = ptr;
    m_tracks.push_back(std::move(track));
    return ptr;
}

SubtitleTrack* Sequence::add_subtitle_track(const std::string& name) {
    auto track = std::make_unique<SubtitleTrack>(name);
    SubtitleTrack* ptr = track.get();
    m_track_lookup[name] = ptr;
    m_tracks.push_back(std::move(track));
    return ptr;
}

LightTrack* Sequence::add_light_track(const std::string& name) {
    auto track = std::make_unique<LightTrack>(name);
    LightTrack* ptr = track.get();
    m_track_lookup[name] = ptr;
    m_tracks.push_back(std::move(track));
    return ptr;
}

PostProcessTrack* Sequence::add_postprocess_track(const std::string& name) {
    auto track = std::make_unique<PostProcessTrack>(name);
    PostProcessTrack* ptr = track.get();
    m_track_lookup[name] = ptr;
    m_tracks.push_back(std::move(track));
    return ptr;
}

Track* Sequence::get_track(const std::string& name) {
    auto it = m_track_lookup.find(name);
    return it != m_track_lookup.end() ? it->second : nullptr;
}

const Track* Sequence::get_track(const std::string& name) const {
    auto it = m_track_lookup.find(name);
    return it != m_track_lookup.end() ? it->second : nullptr;
}

void Sequence::remove_track(const std::string& name) {
    m_track_lookup.erase(name);
    m_tracks.erase(
        std::remove_if(m_tracks.begin(), m_tracks.end(),
            [&name](const std::unique_ptr<Track>& t) {
                return t->get_name() == name;
            }),
        m_tracks.end()
    );
}

void Sequence::clear_tracks() {
    m_tracks.clear();
    m_track_lookup.clear();
}

TrackGroup* Sequence::create_group(const std::string& name) {
    m_groups.push_back({name, {}, false, false});
    return &m_groups.back();
}

TrackGroup* Sequence::get_group(const std::string& name) {
    for (auto& group : m_groups) {
        if (group.name == name) {
            return &group;
        }
    }
    return nullptr;
}

void Sequence::add_track_to_group(Track* track, const std::string& group_name) {
    TrackGroup* group = get_group(group_name);
    if (group) {
        group->tracks.push_back(track);
    }
}

void Sequence::remove_group(const std::string& name) {
    m_groups.erase(
        std::remove_if(m_groups.begin(), m_groups.end(),
            [&name](const TrackGroup& g) { return g.name == name; }),
        m_groups.end()
    );
}

void Sequence::evaluate(float time, scene::World& world) {
    for (auto& track : m_tracks) {
        if (track->is_enabled() && !track->is_muted()) {
            track->evaluate(time, world);
        }
    }
}

void Sequence::reset() {
    for (auto& track : m_tracks) {
        track->reset();
    }
}

void Sequence::add_marker(const std::string& name, float time) {
    m_markers[name] = time;
}

void Sequence::remove_marker(const std::string& name) {
    m_markers.erase(name);
}

float Sequence::get_marker_time(const std::string& name) const {
    auto it = m_markers.find(name);
    return it != m_markers.end() ? it->second : -1.0f;
}

void Sequence::add_section(const Section& section) {
    m_sections.push_back(section);
    std::sort(m_sections.begin(), m_sections.end(),
        [](const Section& a, const Section& b) {
            return a.start_time < b.start_time;
        });
}

const Sequence::Section* Sequence::get_section(const std::string& name) const {
    for (const auto& section : m_sections) {
        if (section.name == name) {
            return &section;
        }
    }
    return nullptr;
}

bool Sequence::save(const std::string& path) const {
    try {
        nlohmann::json j;

        // Info
        j["info"]["name"] = m_info.name;
        j["info"]["description"] = m_info.description;
        j["info"]["author"] = m_info.author;
        j["info"]["frame_rate"] = m_info.frame_rate;
        j["info"]["looping"] = m_info.is_looping;

        // Markers
        j["markers"] = m_markers;

        // Sections
        for (const auto& section : m_sections) {
            j["sections"].push_back({
                {"name", section.name},
                {"start", section.start_time},
                {"end", section.end_time},
                {"color", section.color}
            });
        }

        // Tracks with full data serialization
        j["tracks"] = nlohmann::json::array();
        for (const auto& track : m_tracks) {
            nlohmann::json track_json;
            track_json["name"] = track->get_name();
            track_json["type"] = static_cast<int>(track->get_type());
            track_json["enabled"] = track->is_enabled();
            track_json["muted"] = track->is_muted();
            track_json["locked"] = track->is_locked();
            track->serialize(track_json["data"]);
            j["tracks"].push_back(track_json);
        }

        std::ofstream file(path);
        file << j.dump(2);
        return true;
    } catch (const std::exception& e) {
        core::log(core::LogLevel::Error, "Failed to save sequence: {}", e.what());
        return false;
    }
}

bool Sequence::load(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json j = nlohmann::json::parse(file);

        // Info
        if (j.contains("info")) {
            m_info.name = j["info"].value("name", "");
            m_info.description = j["info"].value("description", "");
            m_info.author = j["info"].value("author", "");
            m_info.frame_rate = j["info"].value("frame_rate", 30.0f);
            m_info.is_looping = j["info"].value("looping", false);
        }

        // Markers
        if (j.contains("markers")) {
            m_markers = j["markers"].get<std::unordered_map<std::string, float>>();
        }

        // Sections
        if (j.contains("sections")) {
            for (const auto& section_json : j["sections"]) {
                Section section;
                section.name = section_json.value("name", "");
                section.start_time = section_json.value("start", 0.0f);
                section.end_time = section_json.value("end", 0.0f);
                section.color = section_json.value("color", 0xFFFFFFFF);
                m_sections.push_back(section);
            }
        }

        // Load tracks with full data deserialization
        clear_tracks();
        if (j.contains("tracks")) {
            for (const auto& track_json : j["tracks"]) {
                TrackType type = static_cast<TrackType>(track_json.value("type", 0));
                std::string name = track_json.value("name", "");
                Track* track = nullptr;

                switch (type) {
                    case TrackType::Camera:
                        track = add_camera_track(name);
                        break;
                    case TrackType::Animation:
                        track = add_animation_track(name);
                        break;
                    case TrackType::Transform:
                        track = add_transform_track(name);
                        break;
                    case TrackType::Audio:
                        track = add_audio_track(name);
                        break;
                    case TrackType::Event:
                        track = add_event_track(name);
                        break;
                    case TrackType::Light:
                        track = add_light_track(name);
                        break;
                    case TrackType::PostProcess:
                        track = add_postprocess_track(name);
                        break;
                    default:
                        break;
                }

                if (track) {
                    track->set_enabled(track_json.value("enabled", true));
                    track->set_muted(track_json.value("muted", false));
                    track->set_locked(track_json.value("locked", false));
                    if (track_json.contains("data")) {
                        track->deserialize(track_json["data"]);
                    }
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        core::log(core::LogLevel::Error, "Failed to load sequence: {}", e.what());
        return false;
    }
}

} // namespace engine::cinematic
