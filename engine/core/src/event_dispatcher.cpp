#include <engine/core/event_dispatcher.hpp>

namespace engine::core {

void EventDispatcher::flush() {
    // Swap queued events to local vector to minimize lock time
    std::vector<QueuedEvent> events_to_dispatch;
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        events_to_dispatch = std::move(m_queued_events);
        m_queued_events.clear();
    }

    // Dispatch all queued events
    for (const auto& queued : events_to_dispatch) {
        queued.dispatch_fn();
    }
}

// Global event dispatcher singleton
EventDispatcher& events() {
    static EventDispatcher instance;
    return instance;
}

} // namespace engine::core
