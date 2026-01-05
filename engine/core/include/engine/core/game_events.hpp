#pragma once

#include <engine/core/event_dispatcher.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <queue>
#include <string>
#include <any>

namespace engine::core {

// ============================================================================
// EventPriority - Handler execution order
// ============================================================================

enum class EventPriority : int {
    First = -1000,      // Execute first (highest priority)
    High = -100,
    Normal = 0,
    Low = 100,
    Last = 1000         // Execute last (lowest priority)
};

// ============================================================================
// GameEventBus - Enhanced event system with priority and consumption
// ============================================================================

class GameEventBus {
public:
    // Callback that returns true to consume the event (stop propagation)
    template<typename T>
    using ConsumableCallback = std::function<bool(const T&)>;

    // Regular callback (does not consume)
    template<typename T>
    using Callback = std::function<void(const T&)>;

    // Singleton access
    static GameEventBus& instance();

    // Delete copy/move
    GameEventBus(const GameEventBus&) = delete;
    GameEventBus& operator=(const GameEventBus&) = delete;
    GameEventBus(GameEventBus&&) = delete;
    GameEventBus& operator=(GameEventBus&&) = delete;

    // ========================================================================
    // Subscription
    // ========================================================================

    // Subscribe with priority (does not consume events)
    template<typename T>
    ScopedConnection subscribe(Callback<T> callback,
                                EventPriority priority = EventPriority::Normal) {
        static_assert(std::is_class_v<T>, "Event type must be a class/struct");

        auto wrapper = [cb = std::move(callback)](const T& e) -> bool {
            cb(e);
            return false;  // Never consume
        };

        return subscribe_internal<T>(std::move(wrapper), priority);
    }

    // Subscribe with consumable callback (can stop propagation)
    template<typename T>
    ScopedConnection subscribe_consumable(ConsumableCallback<T> callback,
                                          EventPriority priority = EventPriority::Normal) {
        static_assert(std::is_class_v<T>, "Event type must be a class/struct");
        return subscribe_internal<T>(std::move(callback), priority);
    }

    // ========================================================================
    // Dispatch
    // ========================================================================

    // Dispatch event immediately (can be consumed)
    // Returns true if the event was consumed
    template<typename T>
    bool emit(const T& event) {
        static_assert(std::is_class_v<T>, "Event type must be a class/struct");
        return dispatch_internal<T>(event, true);
    }

    // Broadcast to ALL handlers (ignores consumption)
    template<typename T>
    void broadcast(const T& event) {
        static_assert(std::is_class_v<T>, "Event type must be a class/struct");
        dispatch_internal<T>(event, false);
    }

    // Queue event for deferred dispatch (next flush)
    template<typename T>
    void emit_deferred(T event) {
        static_assert(std::is_class_v<T>, "Event type must be a class/struct");

        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_deferred_events.push_back({
            std::type_index(typeid(T)),
            [this, e = std::move(event)]() {
                emit(e);
            }
        });
    }

    // ========================================================================
    // Dynamic Events (string-based for Lua)
    // ========================================================================

    using DynamicCallback = std::function<bool(const std::any& data)>;

    // Subscribe to dynamic event by name
    ScopedConnection subscribe_dynamic(const std::string& event_name,
                                       DynamicCallback callback,
                                       EventPriority priority = EventPriority::Normal);

    // Emit dynamic event by name
    bool emit_dynamic(const std::string& event_name, const std::any& data = {});

    // Broadcast dynamic event (ignores consumption)
    void broadcast_dynamic(const std::string& event_name, const std::any& data = {});

    // Queue dynamic event for deferred dispatch
    void emit_dynamic_deferred(const std::string& event_name, std::any data = {});

    // ========================================================================
    // Processing
    // ========================================================================

    // Process all deferred events
    // Call once per frame in PostUpdate phase
    void flush();

    // Check for queued events
    bool has_queued_events() const;

    // Get queued event count
    size_t queued_event_count() const;

    // ========================================================================
    // Utility
    // ========================================================================

    // Clear all handlers for a type
    template<typename T>
    void clear_handlers() {
        auto type_idx = std::type_index(typeid(T));
        std::lock_guard<std::mutex> lock(m_handlers_mutex);
        m_typed_handlers.erase(type_idx);
    }

    // Clear dynamic handlers for an event name
    void clear_dynamic_handlers(const std::string& event_name);

    // Clear all handlers
    void clear_all();

    // Clear deferred queue
    void clear_queue();

    // ========================================================================
    // Statistics
    // ========================================================================

    struct Stats {
        size_t typed_handler_count = 0;
        size_t dynamic_handler_count = 0;
        size_t queued_events = 0;
        size_t events_emitted_this_frame = 0;
        size_t events_consumed_this_frame = 0;
    };

    Stats get_stats() const;
    void reset_frame_stats();

private:
    GameEventBus();
    ~GameEventBus();

    // Internal handler entry
    struct HandlerEntry {
        uint64_t id;
        EventPriority priority;
        std::function<bool(const void*)> callback;  // Returns true to consume
    };

    struct DynamicHandlerEntry {
        uint64_t id;
        EventPriority priority;
        DynamicCallback callback;
    };

    struct DeferredEvent {
        std::type_index type;
        std::function<void()> dispatch_fn;
    };

    struct DeferredDynamicEvent {
        std::string name;
        std::any data;
    };

    template<typename T>
    ScopedConnection subscribe_internal(ConsumableCallback<T> callback,
                                        EventPriority priority) {
        auto type_idx = std::type_index(typeid(T));
        uint64_t handler_id = m_next_handler_id++;

        auto wrapper = [cb = std::move(callback)](const void* event) -> bool {
            return cb(*static_cast<const T*>(event));
        };

        {
            std::lock_guard<std::mutex> lock(m_handlers_mutex);
            auto& handlers = m_typed_handlers[type_idx];
            handlers.push_back({handler_id, priority, std::move(wrapper)});
            sort_handlers(handlers);
        }

        return ScopedConnection([this, type_idx, handler_id]() {
            remove_handler(type_idx, handler_id);
        });
    }

    template<typename T>
    bool dispatch_internal(const T& event, bool allow_consumption) {
        auto type_idx = std::type_index(typeid(T));
        std::vector<HandlerEntry> handlers_copy;

        {
            std::lock_guard<std::mutex> lock(m_handlers_mutex);
            auto it = m_typed_handlers.find(type_idx);
            if (it != m_typed_handlers.end()) {
                handlers_copy = it->second;
            }
        }

        m_events_emitted++;
        bool consumed = false;

        for (const auto& handler : handlers_copy) {
            if (handler.callback(&event)) {
                if (allow_consumption) {
                    m_events_consumed++;
                    consumed = true;
                    break;
                }
            }
        }

        return consumed;
    }

    void sort_handlers(std::vector<HandlerEntry>& handlers);
    void sort_dynamic_handlers(std::vector<DynamicHandlerEntry>& handlers);
    void remove_handler(std::type_index type_idx, uint64_t handler_id);

    mutable std::mutex m_handlers_mutex;
    std::unordered_map<std::type_index, std::vector<HandlerEntry>> m_typed_handlers;
    std::unordered_map<std::string, std::vector<DynamicHandlerEntry>> m_dynamic_handlers;

    mutable std::mutex m_queue_mutex;
    std::vector<DeferredEvent> m_deferred_events;
    std::vector<DeferredDynamicEvent> m_deferred_dynamic_events;

    std::atomic<uint64_t> m_next_handler_id{1};
    std::atomic<size_t> m_events_emitted{0};
    std::atomic<size_t> m_events_consumed{0};
};

// ============================================================================
// Global Access
// ============================================================================

inline GameEventBus& game_events() { return GameEventBus::instance(); }

// ============================================================================
// Common Game Events
// ============================================================================

// Game state events
struct GamePausedEvent {
    bool paused = true;
};

struct GameResumedEvent {};

struct LevelLoadedEvent {
    std::string level_name;
    std::string level_path;
};

struct LevelUnloadedEvent {
    std::string level_name;
};

// Entity lifecycle events
struct EntityCreatedEvent {
    uint32_t entity_id;
    std::string prefab_path;
};

struct EntityDestroyedEvent {
    uint32_t entity_id;
};

// Interaction events
struct InteractionStartedEvent {
    uint32_t interactor_id;
    uint32_t target_id;
    std::string interaction_type;
};

struct InteractionCompletedEvent {
    uint32_t interactor_id;
    uint32_t target_id;
    std::string interaction_type;
    bool success = true;
};

} // namespace engine::core
