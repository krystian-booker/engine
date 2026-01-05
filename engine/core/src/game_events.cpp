#include <engine/core/game_events.hpp>
#include <algorithm>

namespace engine::core {

// ============================================================================
// GameEventBus Implementation
// ============================================================================

GameEventBus::GameEventBus() = default;
GameEventBus::~GameEventBus() = default;

GameEventBus& GameEventBus::instance() {
    static GameEventBus s_instance;
    return s_instance;
}

// ============================================================================
// Dynamic Events
// ============================================================================

ScopedConnection GameEventBus::subscribe_dynamic(const std::string& event_name,
                                                  DynamicCallback callback,
                                                  EventPriority priority) {
    uint64_t handler_id = m_next_handler_id++;

    {
        std::lock_guard<std::mutex> lock(m_handlers_mutex);
        auto& handlers = m_dynamic_handlers[event_name];
        handlers.push_back({handler_id, priority, std::move(callback)});
        sort_dynamic_handlers(handlers);
    }

    return ScopedConnection([this, event_name, handler_id]() {
        std::lock_guard<std::mutex> lock(m_handlers_mutex);
        auto it = m_dynamic_handlers.find(event_name);
        if (it != m_dynamic_handlers.end()) {
            auto& handlers = it->second;
            handlers.erase(
                std::remove_if(handlers.begin(), handlers.end(),
                    [handler_id](const DynamicHandlerEntry& h) { return h.id == handler_id; }),
                handlers.end()
            );
            if (handlers.empty()) {
                m_dynamic_handlers.erase(it);
            }
        }
    });
}

bool GameEventBus::emit_dynamic(const std::string& event_name, const std::any& data) {
    std::vector<DynamicHandlerEntry> handlers_copy;

    {
        std::lock_guard<std::mutex> lock(m_handlers_mutex);
        auto it = m_dynamic_handlers.find(event_name);
        if (it != m_dynamic_handlers.end()) {
            handlers_copy = it->second;
        }
    }

    if (handlers_copy.empty()) {
        return false;
    }

    m_events_emitted++;

    for (const auto& handler : handlers_copy) {
        if (handler.callback(data)) {
            m_events_consumed++;
            return true;
        }
    }

    return false;
}

void GameEventBus::broadcast_dynamic(const std::string& event_name, const std::any& data) {
    std::vector<DynamicHandlerEntry> handlers_copy;

    {
        std::lock_guard<std::mutex> lock(m_handlers_mutex);
        auto it = m_dynamic_handlers.find(event_name);
        if (it != m_dynamic_handlers.end()) {
            handlers_copy = it->second;
        }
    }

    if (handlers_copy.empty()) {
        return;
    }

    m_events_emitted++;

    for (const auto& handler : handlers_copy) {
        handler.callback(data);  // Ignore return value
    }
}

void GameEventBus::emit_dynamic_deferred(const std::string& event_name, std::any data) {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    m_deferred_dynamic_events.push_back({event_name, std::move(data)});
}

// ============================================================================
// Processing
// ============================================================================

void GameEventBus::flush() {
    // Process typed deferred events
    std::vector<DeferredEvent> typed_events;
    std::vector<DeferredDynamicEvent> dynamic_events;

    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        typed_events = std::move(m_deferred_events);
        m_deferred_events.clear();
        dynamic_events = std::move(m_deferred_dynamic_events);
        m_deferred_dynamic_events.clear();
    }

    // Dispatch typed events
    for (auto& event : typed_events) {
        event.dispatch_fn();
    }

    // Dispatch dynamic events
    for (auto& event : dynamic_events) {
        emit_dynamic(event.name, event.data);
    }
}

bool GameEventBus::has_queued_events() const {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    return !m_deferred_events.empty() || !m_deferred_dynamic_events.empty();
}

size_t GameEventBus::queued_event_count() const {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    return m_deferred_events.size() + m_deferred_dynamic_events.size();
}

// ============================================================================
// Utility
// ============================================================================

void GameEventBus::clear_dynamic_handlers(const std::string& event_name) {
    std::lock_guard<std::mutex> lock(m_handlers_mutex);
    m_dynamic_handlers.erase(event_name);
}

void GameEventBus::clear_all() {
    std::lock_guard<std::mutex> lock(m_handlers_mutex);
    m_typed_handlers.clear();
    m_dynamic_handlers.clear();
}

void GameEventBus::clear_queue() {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    m_deferred_events.clear();
    m_deferred_dynamic_events.clear();
}

// ============================================================================
// Statistics
// ============================================================================

GameEventBus::Stats GameEventBus::get_stats() const {
    Stats stats;

    {
        std::lock_guard<std::mutex> lock(m_handlers_mutex);
        for (const auto& [type, handlers] : m_typed_handlers) {
            stats.typed_handler_count += handlers.size();
        }
        for (const auto& [name, handlers] : m_dynamic_handlers) {
            stats.dynamic_handler_count += handlers.size();
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        stats.queued_events = m_deferred_events.size() + m_deferred_dynamic_events.size();
    }

    stats.events_emitted_this_frame = m_events_emitted.load();
    stats.events_consumed_this_frame = m_events_consumed.load();

    return stats;
}

void GameEventBus::reset_frame_stats() {
    m_events_emitted = 0;
    m_events_consumed = 0;
}

// ============================================================================
// Internal Helpers
// ============================================================================

void GameEventBus::sort_handlers(std::vector<HandlerEntry>& handlers) {
    std::stable_sort(handlers.begin(), handlers.end(),
        [](const HandlerEntry& a, const HandlerEntry& b) {
            return static_cast<int>(a.priority) < static_cast<int>(b.priority);
        });
}

void GameEventBus::sort_dynamic_handlers(std::vector<DynamicHandlerEntry>& handlers) {
    std::stable_sort(handlers.begin(), handlers.end(),
        [](const DynamicHandlerEntry& a, const DynamicHandlerEntry& b) {
            return static_cast<int>(a.priority) < static_cast<int>(b.priority);
        });
}

void GameEventBus::remove_handler(std::type_index type_idx, uint64_t handler_id) {
    std::lock_guard<std::mutex> lock(m_handlers_mutex);
    auto it = m_typed_handlers.find(type_idx);
    if (it != m_typed_handlers.end()) {
        auto& handlers = it->second;
        handlers.erase(
            std::remove_if(handlers.begin(), handlers.end(),
                [handler_id](const HandlerEntry& h) { return h.id == handler_id; }),
            handlers.end()
        );
        if (handlers.empty()) {
            m_typed_handlers.erase(it);
        }
    }
}

} // namespace engine::core
