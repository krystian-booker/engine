#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <any>

namespace engine::core {

// ============================================================================
// ScopedConnection - RAII handle for event subscriptions
// ============================================================================

class ScopedConnection {
public:
    ScopedConnection() = default;

    explicit ScopedConnection(std::function<void()> disconnect_fn)
        : m_disconnect(std::move(disconnect_fn)) {}

    ~ScopedConnection() {
        disconnect();
    }

    // Non-copyable
    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;

    // Movable
    ScopedConnection(ScopedConnection&& other) noexcept
        : m_disconnect(std::move(other.m_disconnect)) {
        other.m_disconnect = nullptr;
    }

    ScopedConnection& operator=(ScopedConnection&& other) noexcept {
        if (this != &other) {
            disconnect();
            m_disconnect = std::move(other.m_disconnect);
            other.m_disconnect = nullptr;
        }
        return *this;
    }

    void disconnect() {
        if (m_disconnect) {
            m_disconnect();
            m_disconnect = nullptr;
        }
    }

    bool connected() const {
        return m_disconnect != nullptr;
    }

    // Release ownership without disconnecting
    void release() {
        m_disconnect = nullptr;
    }

private:
    std::function<void()> m_disconnect;
};

// ============================================================================
// EventDispatcher - Type-safe event pub/sub system
// ============================================================================

class EventDispatcher {
public:
    EventDispatcher() = default;
    ~EventDispatcher() = default;

    // Non-copyable, non-movable (singleton-like usage)
    EventDispatcher(const EventDispatcher&) = delete;
    EventDispatcher& operator=(const EventDispatcher&) = delete;
    EventDispatcher(EventDispatcher&&) = delete;
    EventDispatcher& operator=(EventDispatcher&&) = delete;

    // ========================================================================
    // Subscription
    // ========================================================================

    // Subscribe to event type T with a callback
    // Returns a ScopedConnection that auto-unsubscribes on destruction
    template<typename T>
    ScopedConnection subscribe(std::function<void(const T&)> callback) {
        static_assert(std::is_class_v<T>, "Event type must be a class/struct");

        auto type_idx = std::type_index(typeid(T));
        uint64_t handler_id = m_next_handler_id++;

        // Wrap the typed callback
        auto wrapper = [callback = std::move(callback)](const void* event) {
            callback(*static_cast<const T*>(event));
        };

        {
            std::lock_guard<std::mutex> lock(m_handlers_mutex);
            m_handlers[type_idx].push_back({handler_id, std::move(wrapper)});
        }

        // Return connection that removes this handler
        return ScopedConnection([this, type_idx, handler_id]() {
            std::lock_guard<std::mutex> lock(m_handlers_mutex);
            auto it = m_handlers.find(type_idx);
            if (it != m_handlers.end()) {
                auto& handlers = it->second;
                handlers.erase(
                    std::remove_if(handlers.begin(), handlers.end(),
                        [handler_id](const Handler& h) { return h.id == handler_id; }),
                    handlers.end()
                );
            }
        });
    }

    // Subscribe with raw function pointer (no automatic cleanup)
    template<typename T>
    uint64_t subscribe_raw(void(*callback)(const T&)) {
        return subscribe<T>([callback](const T& e) { callback(e); }).release(), m_next_handler_id - 1;
    }

    // Unsubscribe by handler ID
    template<typename T>
    void unsubscribe(uint64_t handler_id) {
        auto type_idx = std::type_index(typeid(T));
        std::lock_guard<std::mutex> lock(m_handlers_mutex);
        auto it = m_handlers.find(type_idx);
        if (it != m_handlers.end()) {
            auto& handlers = it->second;
            handlers.erase(
                std::remove_if(handlers.begin(), handlers.end(),
                    [handler_id](const Handler& h) { return h.id == handler_id; }),
                handlers.end()
            );
        }
    }

    // ========================================================================
    // Immediate Dispatch
    // ========================================================================

    // Dispatch event immediately to all subscribers
    // Calls all handlers synchronously in order of subscription
    template<typename T>
    void dispatch(const T& event) {
        static_assert(std::is_class_v<T>, "Event type must be a class/struct");

        auto type_idx = std::type_index(typeid(T));
        std::vector<Handler> handlers_copy;

        {
            std::lock_guard<std::mutex> lock(m_handlers_mutex);
            auto it = m_handlers.find(type_idx);
            if (it != m_handlers.end()) {
                handlers_copy = it->second;
            }
        }

        for (const auto& handler : handlers_copy) {
            handler.callback(&event);
        }
    }

    // Dispatch with move semantics - Removed to prevent recursion with forwarding reference
    // const T& overload handles both lvalues and rvalues (binding to const ref)
    // template<typename T>
    // void dispatch(T&& event) {
    //     dispatch(static_cast<const T&>(event));
    // }

    // ========================================================================
    // Deferred Dispatch (Queue)
    // ========================================================================

    // Queue an event for deferred dispatch
    // Thread-safe: can be called from any thread
    // Events are dispatched when flush() is called on the main thread
    template<typename T>
    void queue(T event) {
        static_assert(std::is_class_v<T>, "Event type must be a class/struct");

        auto type_idx = std::type_index(typeid(T));

        // Store event and dispatch function
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_queued_events.push_back({
            type_idx,
            [this, event = std::move(event)]() {
                dispatch(event);
            }
        });
    }

    // Process all queued events
    // Should be called once per frame on the main thread
    void flush();

    // Check if there are queued events
    bool has_queued_events() const {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        return !m_queued_events.empty();
    }

    // Get number of queued events
    size_t queued_event_count() const {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        return m_queued_events.size();
    }

    // ========================================================================
    // Utility
    // ========================================================================

    // Clear all handlers for a specific event type
    template<typename T>
    void clear_handlers() {
        auto type_idx = std::type_index(typeid(T));
        std::lock_guard<std::mutex> lock(m_handlers_mutex);
        m_handlers.erase(type_idx);
    }

    // Clear all handlers for all event types
    void clear_all_handlers() {
        std::lock_guard<std::mutex> lock(m_handlers_mutex);
        m_handlers.clear();
    }

    // Clear all queued events without dispatching
    void clear_queue() {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_queued_events.clear();
    }

    // Get handler count for a specific event type
    template<typename T>
    size_t handler_count() const {
        auto type_idx = std::type_index(typeid(T));
        std::lock_guard<std::mutex> lock(m_handlers_mutex);
        auto it = m_handlers.find(type_idx);
        return it != m_handlers.end() ? it->second.size() : 0;
    }

private:
    struct Handler {
        uint64_t id;
        std::function<void(const void*)> callback;
    };

    struct QueuedEvent {
        std::type_index type;
        std::function<void()> dispatch_fn;
    };

    mutable std::mutex m_handlers_mutex;
    std::unordered_map<std::type_index, std::vector<Handler>> m_handlers;

    mutable std::mutex m_queue_mutex;
    std::vector<QueuedEvent> m_queued_events;

    std::atomic<uint64_t> m_next_handler_id{1};
};

// ============================================================================
// Global Event Dispatcher
// ============================================================================

// Get the global event dispatcher instance
// Thread-safe singleton
EventDispatcher& events();

} // namespace engine::core
