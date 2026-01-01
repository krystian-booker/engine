#pragma once

#include <engine/core/math.hpp>
#include <engine/scene/entity.hpp>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace engine::render {

using namespace engine::core;

// Standard animation event types
namespace AnimEvents {
    constexpr const char* FOOTSTEP_LEFT = "footstep_left";
    constexpr const char* FOOTSTEP_RIGHT = "footstep_right";
    constexpr const char* ATTACK_START = "attack_start";
    constexpr const char* ATTACK_HIT = "attack_hit";
    constexpr const char* ATTACK_END = "attack_end";
    constexpr const char* SOUND = "sound";
    constexpr const char* EFFECT = "effect";
    constexpr const char* ENABLE_HITBOX = "enable_hitbox";
    constexpr const char* DISABLE_HITBOX = "disable_hitbox";
    constexpr const char* WEAPON_TRAIL_START = "weapon_trail_start";
    constexpr const char* WEAPON_TRAIL_END = "weapon_trail_end";
}

// Animation event with typed data
struct AnimationEventData {
    std::string event_type;
    float time = 0.0f;  // Normalized time (0-1) within the animation

    // Optional payload
    std::string string_param;
    float float_param = 0.0f;
    int int_param = 0;
    Vec3 vec3_param{0.0f};

    // Bone attachment (for localized events)
    std::string bone_name;
};

// Animation event track (stores events for a clip)
class AnimationEventTrack {
public:
    AnimationEventTrack() = default;

    void add_event(const AnimationEventData& event);
    void remove_event(size_t index);
    void clear();

    // Get events in time range (for checking during playback)
    std::vector<const AnimationEventData*> get_events_in_range(float from_time, float to_time) const;

    // Get all events
    const std::vector<AnimationEventData>& get_events() const { return m_events; }

    // Sort events by time
    void sort();

    size_t event_count() const { return m_events.size(); }

private:
    std::vector<AnimationEventData> m_events;
    bool m_sorted = true;
};

// Handler callback type
using AnimationEventHandler = std::function<void(scene::Entity, const AnimationEventData&)>;

// Global event handler registry
class AnimationEventDispatcher {
public:
    static AnimationEventDispatcher& instance();

    // Register global handler for event type
    uint32_t register_handler(const std::string& event_type, AnimationEventHandler handler);

    // Unregister handler
    void unregister_handler(const std::string& event_type, uint32_t handler_id);
    void unregister_all(const std::string& event_type);

    // Dispatch event (called by animation system)
    void dispatch(scene::Entity entity, const AnimationEventData& event);

    // Clear all handlers
    void clear();

private:
    AnimationEventDispatcher() = default;

    struct HandlerEntry {
        uint32_t id;
        AnimationEventHandler handler;
    };

    std::unordered_map<std::string, std::vector<HandlerEntry>> m_handlers;
    uint32_t m_next_handler_id = 1;
};

// Auto footstep detection from animation curves
void auto_detect_footsteps(class AnimationClip& clip,
                           const class Skeleton& skeleton,
                           const std::string& left_foot_bone,
                           const std::string& right_foot_bone,
                           float height_threshold = 0.1f);

} // namespace engine::render
