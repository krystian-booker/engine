#pragma once

#include <engine/ai/behavior_tree.hpp>
#include <limits>

namespace engine::ai {

// ============================================================================
// Inverter
// Inverts the result of its child
// ============================================================================

class BTInverter : public BTDecorator {
public:
    explicit BTInverter(std::string name = "Inverter")
        : BTDecorator(std::move(name)) {}

    BTStatus tick(BTContext& ctx) override {
        if (!m_child) {
            m_last_status = BTStatus::Failure;
            return BTStatus::Failure;
        }

        BTStatus status = m_child->tick(ctx);

        if (status == BTStatus::Success) {
            m_last_status = BTStatus::Failure;
            return BTStatus::Failure;
        }
        if (status == BTStatus::Failure) {
            m_last_status = BTStatus::Success;
            return BTStatus::Success;
        }

        m_last_status = BTStatus::Running;
        return BTStatus::Running;
    }
};

// ============================================================================
// Succeeder
// Always returns Success (unless child is Running)
// ============================================================================

class BTSucceeder : public BTDecorator {
public:
    explicit BTSucceeder(std::string name = "Succeeder")
        : BTDecorator(std::move(name)) {}

    BTStatus tick(BTContext& ctx) override {
        if (!m_child) {
            m_last_status = BTStatus::Success;
            return BTStatus::Success;
        }

        BTStatus status = m_child->tick(ctx);

        if (status == BTStatus::Running) {
            m_last_status = BTStatus::Running;
            return BTStatus::Running;
        }

        m_last_status = BTStatus::Success;
        return BTStatus::Success;
    }
};

// ============================================================================
// Failer
// Always returns Failure (unless child is Running)
// ============================================================================

class BTFailer : public BTDecorator {
public:
    explicit BTFailer(std::string name = "Failer")
        : BTDecorator(std::move(name)) {}

    BTStatus tick(BTContext& ctx) override {
        if (!m_child) {
            m_last_status = BTStatus::Failure;
            return BTStatus::Failure;
        }

        BTStatus status = m_child->tick(ctx);

        if (status == BTStatus::Running) {
            m_last_status = BTStatus::Running;
            return BTStatus::Running;
        }

        m_last_status = BTStatus::Failure;
        return BTStatus::Failure;
    }
};

// ============================================================================
// Repeater
// Repeats its child a number of times
// ============================================================================

class BTRepeater : public BTDecorator {
public:
    static constexpr int INFINITE = -1;

    explicit BTRepeater(std::string name = "Repeater", int repeat_count = INFINITE)
        : BTDecorator(std::move(name))
        , m_repeat_count(repeat_count) {}

    BTStatus tick(BTContext& ctx) override {
        if (!m_child) {
            m_last_status = BTStatus::Failure;
            return BTStatus::Failure;
        }

        // Check if we've completed all repetitions
        if (m_repeat_count != INFINITE && m_current_count >= m_repeat_count) {
            m_last_status = BTStatus::Success;
            return BTStatus::Success;
        }

        BTStatus status = m_child->tick(ctx);

        if (status == BTStatus::Running) {
            m_last_status = BTStatus::Running;
            return BTStatus::Running;
        }

        // Child completed (success or failure)
        m_current_count++;
        m_child->reset();

        // Check if we should continue
        if (m_repeat_count == INFINITE || m_current_count < m_repeat_count) {
            m_last_status = BTStatus::Running;
            return BTStatus::Running;
        }

        m_last_status = BTStatus::Success;
        return BTStatus::Success;
    }

    void reset() override {
        BTDecorator::reset();
        m_current_count = 0;
    }

    void set_repeat_count(int count) { m_repeat_count = count; }

private:
    int m_repeat_count;
    int m_current_count = 0;
};

// ============================================================================
// Repeat Until Fail
// Repeats child until it fails
// ============================================================================

class BTRepeatUntilFail : public BTDecorator {
public:
    explicit BTRepeatUntilFail(std::string name = "RepeatUntilFail")
        : BTDecorator(std::move(name)) {}

    BTStatus tick(BTContext& ctx) override {
        if (!m_child) {
            m_last_status = BTStatus::Success;
            return BTStatus::Success;
        }

        BTStatus status = m_child->tick(ctx);

        if (status == BTStatus::Failure) {
            m_last_status = BTStatus::Success;
            return BTStatus::Success;
        }

        if (status == BTStatus::Success) {
            m_child->reset();
        }

        m_last_status = BTStatus::Running;
        return BTStatus::Running;
    }
};

// ============================================================================
// Cooldown
// Prevents child from running more often than specified interval
// ============================================================================

class BTCooldown : public BTDecorator {
public:
    explicit BTCooldown(std::string name = "Cooldown", float cooldown_time = 1.0f)
        : BTDecorator(std::move(name))
        , m_cooldown_time(cooldown_time) {}

    BTStatus tick(BTContext& ctx) override {
        // Update timer
        if (m_time_remaining > 0.0f) {
            m_time_remaining -= ctx.delta_time;
            if (m_time_remaining > 0.0f) {
                m_last_status = BTStatus::Failure;
                return BTStatus::Failure;  // Still on cooldown
            }
        }

        if (!m_child) {
            m_last_status = BTStatus::Failure;
            return BTStatus::Failure;
        }

        BTStatus status = m_child->tick(ctx);

        // Start cooldown when child completes
        if (status != BTStatus::Running) {
            m_time_remaining = m_cooldown_time;
        }

        m_last_status = status;
        return status;
    }

    void reset() override {
        BTDecorator::reset();
        m_time_remaining = 0.0f;
    }

    void set_cooldown_time(float time) { m_cooldown_time = time; }

private:
    float m_cooldown_time;
    float m_time_remaining = 0.0f;
};

// ============================================================================
// Time Limit
// Fails if child takes too long
// ============================================================================

class BTTimeLimit : public BTDecorator {
public:
    explicit BTTimeLimit(std::string name = "TimeLimit", float time_limit = 5.0f)
        : BTDecorator(std::move(name))
        , m_time_limit(time_limit) {}

    BTStatus tick(BTContext& ctx) override {
        if (!m_child) {
            m_last_status = BTStatus::Failure;
            return BTStatus::Failure;
        }

        // Check time limit
        m_elapsed_time += ctx.delta_time;
        if (m_elapsed_time >= m_time_limit) {
            m_child->reset();
            m_last_status = BTStatus::Failure;
            return BTStatus::Failure;
        }

        BTStatus status = m_child->tick(ctx);

        if (status != BTStatus::Running) {
            m_elapsed_time = 0.0f;
        }

        m_last_status = status;
        return status;
    }

    void reset() override {
        BTDecorator::reset();
        m_elapsed_time = 0.0f;
    }

    void set_time_limit(float limit) { m_time_limit = limit; }

private:
    float m_time_limit;
    float m_elapsed_time = 0.0f;
};

// ============================================================================
// Delay
// Waits before executing child
// ============================================================================

class BTDelay : public BTDecorator {
public:
    explicit BTDelay(std::string name = "Delay", float delay_time = 1.0f)
        : BTDecorator(std::move(name))
        , m_delay_time(delay_time) {}

    BTStatus tick(BTContext& ctx) override {
        if (!m_started) {
            m_started = true;
            m_elapsed_time = 0.0f;
        }

        m_elapsed_time += ctx.delta_time;

        if (m_elapsed_time < m_delay_time) {
            m_last_status = BTStatus::Running;
            return BTStatus::Running;
        }

        if (!m_child) {
            m_started = false;
            m_last_status = BTStatus::Success;
            return BTStatus::Success;
        }

        BTStatus status = m_child->tick(ctx);

        if (status != BTStatus::Running) {
            m_started = false;
        }

        m_last_status = status;
        return status;
    }

    void reset() override {
        BTDecorator::reset();
        m_started = false;
        m_elapsed_time = 0.0f;
    }

    void set_delay_time(float time) { m_delay_time = time; }

private:
    float m_delay_time;
    float m_elapsed_time = 0.0f;
    bool m_started = false;
};

// ============================================================================
// Conditional Decorator
// Only executes child if condition is true
// ============================================================================

class BTConditional : public BTDecorator {
public:
    using ConditionFn = std::function<bool(const BTContext&)>;

    BTConditional(std::string name, ConditionFn condition)
        : BTDecorator(std::move(name))
        , m_condition(std::move(condition)) {}

    BTStatus tick(BTContext& ctx) override {
        if (!m_condition || !m_condition(ctx)) {
            m_last_status = BTStatus::Failure;
            return BTStatus::Failure;
        }

        if (!m_child) {
            m_last_status = BTStatus::Success;
            return BTStatus::Success;
        }

        m_last_status = m_child->tick(ctx);
        return m_last_status;
    }

    void set_condition(ConditionFn condition) { m_condition = std::move(condition); }

private:
    ConditionFn m_condition;
};

// ============================================================================
// Until Success
// Keeps executing until child succeeds
// ============================================================================

class BTUntilSuccess : public BTDecorator {
public:
    explicit BTUntilSuccess(std::string name = "UntilSuccess")
        : BTDecorator(std::move(name)) {}

    BTStatus tick(BTContext& ctx) override {
        if (!m_child) {
            m_last_status = BTStatus::Failure;
            return BTStatus::Failure;
        }

        BTStatus status = m_child->tick(ctx);

        if (status == BTStatus::Success) {
            m_last_status = BTStatus::Success;
            return BTStatus::Success;
        }

        if (status == BTStatus::Failure) {
            m_child->reset();
        }

        m_last_status = BTStatus::Running;
        return BTStatus::Running;
    }
};

} // namespace engine::ai
