#include <engine/render/animation_events.hpp>
#include <engine/core/log.hpp>
#include <algorithm>

namespace engine::render {

// ============================================================================
// AnimationEventTrack
// ============================================================================

void AnimationEventTrack::add_event(const AnimationEventData& event) {
    m_events.push_back(event);
    m_sorted = false;
}

void AnimationEventTrack::remove_event(size_t index) {
    if (index < m_events.size()) {
        m_events.erase(m_events.begin() + index);
    }
}

void AnimationEventTrack::clear() {
    m_events.clear();
    m_sorted = true;
}

std::vector<const AnimationEventData*> AnimationEventTrack::get_events_in_range(
    float from_time, float to_time) const {

    std::vector<const AnimationEventData*> result;

    for (const auto& event : m_events) {
        // Handle wrap-around case (animation looping)
        if (from_time <= to_time) {
            if (event.time >= from_time && event.time < to_time) {
                result.push_back(&event);
            }
        } else {
            // Wrapped: check [from_time, 1.0] and [0.0, to_time]
            if (event.time >= from_time || event.time < to_time) {
                result.push_back(&event);
            }
        }
    }

    return result;
}

void AnimationEventTrack::sort() {
    if (!m_sorted) {
        std::sort(m_events.begin(), m_events.end(),
            [](const AnimationEventData& a, const AnimationEventData& b) {
                return a.time < b.time;
            });
        m_sorted = true;
    }
}

// ============================================================================
// AnimationEventDispatcher
// ============================================================================

AnimationEventDispatcher& AnimationEventDispatcher::instance() {
    static AnimationEventDispatcher instance;
    return instance;
}

uint32_t AnimationEventDispatcher::register_handler(const std::string& event_type,
                                                     AnimationEventHandler handler) {
    uint32_t id = m_next_handler_id++;
    m_handlers[event_type].push_back({id, std::move(handler)});
    return id;
}

void AnimationEventDispatcher::unregister_handler(const std::string& event_type,
                                                   uint32_t handler_id) {
    auto it = m_handlers.find(event_type);
    if (it != m_handlers.end()) {
        auto& handlers = it->second;
        handlers.erase(
            std::remove_if(handlers.begin(), handlers.end(),
                [handler_id](const HandlerEntry& e) { return e.id == handler_id; }),
            handlers.end()
        );
    }
}

void AnimationEventDispatcher::unregister_all(const std::string& event_type) {
    m_handlers.erase(event_type);
}

void AnimationEventDispatcher::dispatch(uint32_t entity, const AnimationEventData& event) {
    auto it = m_handlers.find(event.event_type);
    if (it != m_handlers.end()) {
        for (const auto& entry : it->second) {
            if (entry.handler) {
                entry.handler(entity, event);
            }
        }
    }

    // Also check for wildcard handlers
    auto wildcard_it = m_handlers.find("*");
    if (wildcard_it != m_handlers.end()) {
        for (const auto& entry : wildcard_it->second) {
            if (entry.handler) {
                entry.handler(entity, event);
            }
        }
    }
}

void AnimationEventDispatcher::clear() {
    m_handlers.clear();
}

// ============================================================================
// Auto footstep detection
// ============================================================================

void auto_detect_footsteps(AnimationClip& clip,
                           const Skeleton& skeleton,
                           const std::string& left_foot_bone,
                           const std::string& right_foot_bone,
                           float height_threshold) {
    // This would analyze foot bone positions over time and detect when
    // feet touch the ground (y position crosses threshold from above)

    // Placeholder implementation - would need actual animation curve access
    core::log(core::LogLevel::Debug,
              "Auto-detecting footsteps for bones: {}, {}",
              left_foot_bone, right_foot_bone);
}

} // namespace engine::render
