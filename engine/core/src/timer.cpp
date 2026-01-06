#include <engine/core/timer.hpp>
#include <algorithm>

namespace engine::core {

// ============================================================================
// TimerManager Implementation
// ============================================================================

TimerManager::TimerManager() = default;
TimerManager::~TimerManager() = default;

TimerManager& TimerManager::instance() {
    static TimerManager s_instance;
    return s_instance;
}

TimerHandle TimerManager::allocate_handle() {
    return TimerHandle{m_next_id++};
}

TimerManager::Timer* TimerManager::find_timer(TimerHandle handle) {
    for (auto& timer : m_timers) {
        if (timer.handle == handle && !timer.marked_for_removal) {
            return &timer;
        }
    }
    return nullptr;
}

const TimerManager::Timer* TimerManager::find_timer(TimerHandle handle) const {
    for (const auto& timer : m_timers) {
        if (timer.handle == handle && !timer.marked_for_removal) {
            return &timer;
        }
    }
    return nullptr;
}

TimerManager::Sequence* TimerManager::find_sequence(TimerHandle handle) {
    for (auto& seq : m_sequences) {
        if (seq.handle == handle && !seq.marked_for_removal) {
            return &seq;
        }
    }
    return nullptr;
}

const TimerManager::Sequence* TimerManager::find_sequence(TimerHandle handle) const {
    for (const auto& seq : m_sequences) {
        if (seq.handle == handle && !seq.marked_for_removal) {
            return &seq;
        }
    }
    return nullptr;
}

// ============================================================================
// Timer Creation
// ============================================================================

TimerHandle TimerManager::set_timeout(float delay, Callback callback) {
    TimerConfig config;
    config.delay = delay;
    config.repeat_count = 0;  // One-shot
    return create_timer(config, std::move(callback));
}

TimerHandle TimerManager::set_interval(float interval, Callback callback) {
    TimerConfig config;
    config.delay = interval;
    config.interval = interval;
    config.repeat_count = -1;  // Infinite
    return create_timer(config, std::move(callback));
}

TimerHandle TimerManager::set_interval(float interval, int count, Callback callback) {
    TimerConfig config;
    config.delay = interval;
    config.interval = interval;
    config.repeat_count = count;
    return create_timer(config, std::move(callback));
}

TimerHandle TimerManager::create_timer(const TimerConfig& config, Callback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);

    Timer timer;
    timer.handle = allocate_handle();
    timer.callback = std::move(callback);
    timer.remaining_time = config.delay;
    timer.interval = config.interval;
    timer.remaining_repeats = config.repeat_count;
    timer.paused = config.start_paused;
    timer.use_scaled_time = config.use_scaled_time;
    timer.marked_for_removal = false;

    m_timers.push_back(std::move(timer));
    m_total_timers_created++;

    return m_timers.back().handle;
}

// ============================================================================
// Timer Control
// ============================================================================

void TimerManager::cancel(TimerHandle handle) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (auto* timer = find_timer(handle)) {
        timer->marked_for_removal = true;
        return;
    }

    if (auto* seq = find_sequence(handle)) {
        seq->marked_for_removal = true;
    }
}

void TimerManager::pause(TimerHandle handle) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (auto* timer = find_timer(handle)) {
        timer->paused = true;
        return;
    }

    if (auto* seq = find_sequence(handle)) {
        seq->paused = true;
    }
}

void TimerManager::resume(TimerHandle handle) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (auto* timer = find_timer(handle)) {
        timer->paused = false;
        return;
    }

    if (auto* seq = find_sequence(handle)) {
        seq->paused = false;
    }
}

bool TimerManager::is_active(TimerHandle handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return find_timer(handle) != nullptr || find_sequence(handle) != nullptr;
}

bool TimerManager::is_paused(TimerHandle handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (const auto* timer = find_timer(handle)) {
        return timer->paused;
    }

    for (const auto& seq : m_sequences) {
        if (seq.handle == handle && !seq.marked_for_removal) {
            return seq.paused;
        }
    }

    return false;
}

float TimerManager::get_remaining(TimerHandle handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (const auto* timer = find_timer(handle)) {
        return timer->remaining_time;
    }

    return 0.0f;
}

void TimerManager::reset(TimerHandle handle) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (auto* timer = find_timer(handle)) {
        timer->remaining_time = timer->interval > 0.0f ? timer->interval : 0.0f;
    }
}

// ============================================================================
// Bulk Operations
// ============================================================================

void TimerManager::cancel_all() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& timer : m_timers) {
        timer.marked_for_removal = true;
    }

    for (auto& seq : m_sequences) {
        seq.marked_for_removal = true;
    }
}

void TimerManager::pause_all() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& timer : m_timers) {
        timer.paused = true;
    }

    for (auto& seq : m_sequences) {
        seq.paused = true;
    }
}

void TimerManager::resume_all() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& timer : m_timers) {
        timer.paused = false;
    }

    for (auto& seq : m_sequences) {
        seq.paused = false;
    }
}

// ============================================================================
// Sequence Builder
// ============================================================================

TimerManager::SequenceBuilder::SequenceBuilder(TimerManager& manager)
    : m_manager(manager) {}

TimerManager::SequenceBuilder& TimerManager::SequenceBuilder::delay(float seconds) {
    Step step;
    step.type = StepType::Delay;
    step.delay = seconds;
    m_steps.push_back(std::move(step));
    return *this;
}

TimerManager::SequenceBuilder& TimerManager::SequenceBuilder::then(Callback callback) {
    Step step;
    step.type = StepType::Callback;
    step.callback = std::move(callback);
    m_steps.push_back(std::move(step));
    return *this;
}

TimerManager::SequenceBuilder& TimerManager::SequenceBuilder::wait_until(ConditionCallback condition) {
    Step step;
    step.type = StepType::WaitUntil;
    step.condition = std::move(condition);
    m_steps.push_back(std::move(step));
    return *this;
}

TimerManager::SequenceBuilder& TimerManager::SequenceBuilder::loop(int count) {
    m_loop_count = count;
    return *this;
}

TimerHandle TimerManager::SequenceBuilder::start() {
    if (m_steps.empty()) {
        return TimerHandle{0};
    }

    std::lock_guard<std::mutex> lock(m_manager.m_mutex);

    Sequence seq;
    seq.handle = m_manager.allocate_handle();
    seq.steps = std::move(m_steps);
    seq.current_step = 0;
    seq.loop_count = m_loop_count;
    seq.remaining_loops = m_loop_count;
    seq.step_timer = 0.0f;
    seq.paused = false;
    seq.marked_for_removal = false;

    // Initialize first step timer if it's a delay
    if (!seq.steps.empty() && seq.steps[0].type == StepType::Delay) {
        seq.step_timer = seq.steps[0].delay;
    }

    m_manager.m_sequences.push_back(std::move(seq));
    m_manager.m_total_timers_created++;

    return m_manager.m_sequences.back().handle;
}

TimerManager::SequenceBuilder TimerManager::sequence() {
    return SequenceBuilder(*this);
}

// ============================================================================
// Update
// ============================================================================

void TimerManager::update(float dt, float time_scale) {
    m_timers_fired_this_frame = 0;

    update_timers(dt, time_scale);
    update_sequences(dt, time_scale);
    cleanup_removed();
}

void TimerManager::update_timers(float dt, float time_scale) {
    // Copy handles to avoid issues with callbacks that modify timers
    std::vector<std::pair<TimerHandle, Callback>> to_fire;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (auto& timer : m_timers) {
            if (timer.marked_for_removal || timer.paused) {
                continue;
            }

            // Apply time scale if timer respects it
            float effective_dt = timer.use_scaled_time ? dt * time_scale : dt;

            timer.remaining_time -= effective_dt;

            if (timer.remaining_time <= 0.0f) {
                // Timer fired
                to_fire.push_back({timer.handle, timer.callback});

                if (timer.remaining_repeats == 0) {
                    // One-shot, remove
                    timer.marked_for_removal = true;
                } else {
                    // Reset for next interval
                    timer.remaining_time += timer.interval;

                    // Handle overshoot (multiple fires in one frame)
                    while (timer.remaining_time <= 0.0f && timer.interval > 0.0f) {
                        to_fire.push_back({timer.handle, timer.callback});
                        timer.remaining_time += timer.interval;

                        if (timer.remaining_repeats > 0) {
                            timer.remaining_repeats--;
                            if (timer.remaining_repeats == 0) {
                                timer.marked_for_removal = true;
                                break;
                            }
                        }
                    }

                    // Decrement repeat count if not infinite
                    if (timer.remaining_repeats > 0) {
                        timer.remaining_repeats--;
                        if (timer.remaining_repeats == 0) {
                            timer.marked_for_removal = true;
                        }
                    }
                }
            }
        }
    }

    // Fire callbacks outside the lock
    for (const auto& [handle, callback] : to_fire) {
        if (callback) {
            callback();
            m_timers_fired_this_frame++;
        }
    }
}

void TimerManager::update_sequences(float dt, float time_scale) {
    std::vector<Callback> to_fire;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        for (auto& seq : m_sequences) {
            if (seq.marked_for_removal || seq.paused) {
                continue;
            }

            bool advance = true;

            while (advance && seq.current_step < seq.steps.size()) {
                auto& step = seq.steps[seq.current_step];

                switch (step.type) {
                    case SequenceBuilder::StepType::Delay: {
                        float effective_dt = dt * time_scale;
                        seq.step_timer -= effective_dt;

                        if (seq.step_timer <= 0.0f) {
                            seq.current_step++;
                            if (seq.current_step < seq.steps.size()) {
                                // Initialize next step
                                auto& next = seq.steps[seq.current_step];
                                if (next.type == SequenceBuilder::StepType::Delay) {
                                    seq.step_timer = next.delay;
                                }
                            }
                        } else {
                            advance = false;
                        }
                        break;
                    }

                    case SequenceBuilder::StepType::Callback: {
                        if (step.callback) {
                            to_fire.push_back(step.callback);
                        }
                        seq.current_step++;
                        if (seq.current_step < seq.steps.size()) {
                            auto& next = seq.steps[seq.current_step];
                            if (next.type == SequenceBuilder::StepType::Delay) {
                                seq.step_timer = next.delay;
                            }
                        }
                        break;
                    }

                    case SequenceBuilder::StepType::WaitUntil: {
                        if (step.condition && step.condition()) {
                            seq.current_step++;
                            if (seq.current_step < seq.steps.size()) {
                                auto& next = seq.steps[seq.current_step];
                                if (next.type == SequenceBuilder::StepType::Delay) {
                                    seq.step_timer = next.delay;
                                }
                            }
                        } else {
                            advance = false;
                        }
                        break;
                    }
                }
            }

            // Check if sequence completed
            if (seq.current_step >= seq.steps.size()) {
                // Check for looping
                if (seq.loop_count == -1 || seq.remaining_loops > 0) {
                    // Loop back
                    seq.current_step = 0;
                    if (seq.remaining_loops > 0) {
                        seq.remaining_loops--;
                    }
                    if (!seq.steps.empty() && seq.steps[0].type == SequenceBuilder::StepType::Delay) {
                        seq.step_timer = seq.steps[0].delay;
                    }
                } else {
                    // Done
                    seq.marked_for_removal = true;
                }
            }
        }
    }

    // Fire callbacks outside the lock
    for (const auto& callback : to_fire) {
        if (callback) {
            callback();
            m_timers_fired_this_frame++;
        }
    }
}

void TimerManager::cleanup_removed() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_timers.erase(
        std::remove_if(m_timers.begin(), m_timers.end(),
            [](const Timer& t) { return t.marked_for_removal; }),
        m_timers.end()
    );

    m_sequences.erase(
        std::remove_if(m_sequences.begin(), m_sequences.end(),
            [](const Sequence& s) { return s.marked_for_removal; }),
        m_sequences.end()
    );
}

// ============================================================================
// Statistics
// ============================================================================

TimerManager::Stats TimerManager::get_stats() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    Stats stats;
    stats.active_timers = 0;
    stats.active_sequences = 0;

    for (const auto& timer : m_timers) {
        if (!timer.marked_for_removal) {
            stats.active_timers++;
        }
    }

    for (const auto& seq : m_sequences) {
        if (!seq.marked_for_removal) {
            stats.active_sequences++;
        }
    }

    stats.timers_fired_this_frame = m_timers_fired_this_frame;
    stats.total_timers_created = m_total_timers_created;

    return stats;
}

} // namespace engine::core
