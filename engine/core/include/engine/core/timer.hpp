#pragma once

#include <functional>
#include <vector>
#include <cstdint>
#include <memory>
#include <mutex>

namespace engine::core {

// ============================================================================
// TimerHandle - Unique identifier for timer management
// ============================================================================

struct TimerHandle {
    uint64_t id = 0;

    bool valid() const { return id != 0; }
    explicit operator bool() const { return valid(); }

    bool operator==(const TimerHandle& other) const { return id == other.id; }
    bool operator!=(const TimerHandle& other) const { return id != other.id; }
};

// ============================================================================
// TimerConfig - Configuration for timer creation
// ============================================================================

struct TimerConfig {
    float delay = 0.0f;             // Initial delay before first execution
    float interval = 0.0f;          // Repeat interval (0 = one-shot after delay)
    int repeat_count = 0;           // 0 = one-shot, -1 = infinite, N = repeat N times
    bool use_scaled_time = true;    // Respects time scale (pausing/slowmo)
    bool start_paused = false;      // Start in paused state
};

// ============================================================================
// TimerManager - Manages all timers in the engine
// ============================================================================

class TimerManager {
public:
    using Callback = std::function<void()>;
    using ConditionCallback = std::function<bool()>;

    // Singleton access
    static TimerManager& instance();

    // Delete copy/move
    TimerManager(const TimerManager&) = delete;
    TimerManager& operator=(const TimerManager&) = delete;
    TimerManager(TimerManager&&) = delete;
    TimerManager& operator=(TimerManager&&) = delete;

    // ========================================================================
    // Timer Creation
    // ========================================================================

    // One-shot timer - executes callback once after delay
    TimerHandle set_timeout(float delay, Callback callback);

    // Repeating timer - executes callback at interval
    TimerHandle set_interval(float interval, Callback callback);

    // Repeating timer with count - executes N times
    TimerHandle set_interval(float interval, int count, Callback callback);

    // Configurable timer with full options
    TimerHandle create_timer(const TimerConfig& config, Callback callback);

    // ========================================================================
    // Timer Control
    // ========================================================================

    // Cancel a timer
    void cancel(TimerHandle handle);

    // Pause a timer (time doesn't advance)
    void pause(TimerHandle handle);

    // Resume a paused timer
    void resume(TimerHandle handle);

    // Check if timer is active (not cancelled, may be paused)
    bool is_active(TimerHandle handle) const;

    // Check if timer is paused
    bool is_paused(TimerHandle handle) const;

    // Get remaining time until next execution
    float get_remaining(TimerHandle handle) const;

    // Reset timer to initial delay
    void reset(TimerHandle handle);

    // ========================================================================
    // Bulk Operations
    // ========================================================================

    // Cancel all timers
    void cancel_all();

    // Pause all timers
    void pause_all();

    // Resume all timers
    void resume_all();

    // ========================================================================
    // Sequence Builder - Coroutine-like sequential execution
    // ========================================================================

    class SequenceBuilder {
    public:
        explicit SequenceBuilder(TimerManager& manager);

        // Wait for specified duration
        SequenceBuilder& delay(float seconds);

        // Execute callback
        SequenceBuilder& then(Callback callback);

        // Wait until condition returns true
        SequenceBuilder& wait_until(ConditionCallback condition);

        // Loop the sequence (count = -1 for infinite)
        SequenceBuilder& loop(int count = -1);

        // Start the sequence, returns handle for control
        TimerHandle start();

    private:
        friend class TimerManager;

        enum class StepType { Delay, Callback, WaitUntil };

        struct Step {
            StepType type;
            float delay = 0.0f;
            Callback callback;
            ConditionCallback condition;
        };

        TimerManager& m_manager;
        std::vector<Step> m_steps;
        int m_loop_count = 0;
    };

    // Create a sequence builder
    SequenceBuilder sequence();

    // ========================================================================
    // Update
    // ========================================================================

    // Update all timers - call once per frame
    // dt: delta time in seconds
    // time_scale: multiplier for scaled timers (1.0 = normal, 0 = paused)
    void update(float dt, float time_scale = 1.0f);

    // ========================================================================
    // Statistics
    // ========================================================================

    struct Stats {
        size_t active_timers = 0;
        size_t active_sequences = 0;
        size_t timers_fired_this_frame = 0;
        size_t total_timers_created = 0;
    };

    Stats get_stats() const;

private:
    TimerManager();
    ~TimerManager();

    struct Timer {
        TimerHandle handle;
        Callback callback;
        float initial_delay;
        float remaining_time;
        float interval;
        int remaining_repeats;  // -1 = infinite
        bool paused = false;
        bool use_scaled_time = true;
        bool marked_for_removal = false;
    };

    struct Sequence {
        TimerHandle handle;
        std::vector<SequenceBuilder::Step> steps;
        size_t current_step = 0;
        int loop_count = 0;        // -1 = infinite, 0 = no loop
        int remaining_loops = 0;
        float step_timer = 0.0f;
        bool paused = false;
        bool marked_for_removal = false;
    };

    TimerHandle allocate_handle();
    Timer* find_timer(TimerHandle handle);
    const Timer* find_timer(TimerHandle handle) const;
    Sequence* find_sequence(TimerHandle handle);
    const Sequence* find_sequence(TimerHandle handle) const;

    void update_timers(float dt, float time_scale);
    void update_sequences(float dt, float time_scale);
    void cleanup_removed();

    mutable std::mutex m_mutex;
    std::vector<Timer> m_timers;
    std::vector<Sequence> m_sequences;
    uint64_t m_next_id = 1;
    size_t m_timers_fired_this_frame = 0;
    size_t m_total_timers_created = 0;
};

// ============================================================================
// Global Access
// ============================================================================

inline TimerManager& timers() { return TimerManager::instance(); }

} // namespace engine::core
