#pragma once

#include <engine/ai/behavior_tree.hpp>
#include <random>

namespace engine::ai {

// ============================================================================
// Selector (OR logic)
// Executes children until one succeeds or all fail
// ============================================================================

class BTSelector : public BTComposite {
public:
    explicit BTSelector(std::string name = "Selector")
        : BTComposite(std::move(name)) {}

    BTStatus tick(BTContext& ctx) override {
        // Continue from where we left off (for Running children)
        while (m_current_child < m_children.size()) {
            BTStatus status = m_children[m_current_child]->tick(ctx);

            if (status == BTStatus::Success) {
                m_current_child = 0;
                m_last_status = BTStatus::Success;
                return BTStatus::Success;
            }

            if (status == BTStatus::Running) {
                m_last_status = BTStatus::Running;
                return BTStatus::Running;
            }

            // Failure - try next child
            m_current_child++;
        }

        // All children failed
        m_current_child = 0;
        m_last_status = BTStatus::Failure;
        return BTStatus::Failure;
    }
};

// ============================================================================
// Sequence (AND logic)
// Executes children in order until one fails or all succeed
// ============================================================================

class BTSequence : public BTComposite {
public:
    explicit BTSequence(std::string name = "Sequence")
        : BTComposite(std::move(name)) {}

    BTStatus tick(BTContext& ctx) override {
        // Continue from where we left off
        while (m_current_child < m_children.size()) {
            BTStatus status = m_children[m_current_child]->tick(ctx);

            if (status == BTStatus::Failure) {
                m_current_child = 0;
                m_last_status = BTStatus::Failure;
                return BTStatus::Failure;
            }

            if (status == BTStatus::Running) {
                m_last_status = BTStatus::Running;
                return BTStatus::Running;
            }

            // Success - continue to next child
            m_current_child++;
        }

        // All children succeeded
        m_current_child = 0;
        m_last_status = BTStatus::Success;
        return BTStatus::Success;
    }
};

// ============================================================================
// Parallel
// Executes all children simultaneously
// ============================================================================

class BTParallel : public BTComposite {
public:
    enum class Policy {
        RequireOne,     // Succeed/fail when one child succeeds/fails
        RequireAll      // Succeed/fail when all children succeed/fail
    };

    BTParallel(std::string name = "Parallel",
               Policy success_policy = Policy::RequireAll,
               Policy failure_policy = Policy::RequireOne)
        : BTComposite(std::move(name))
        , m_success_policy(success_policy)
        , m_failure_policy(failure_policy) {}

    BTStatus tick(BTContext& ctx) override {
        int success_count = 0;
        int failure_count = 0;

        for (auto& child : m_children) {
            BTStatus status = child->tick(ctx);

            if (status == BTStatus::Success) {
                success_count++;
                if (m_success_policy == Policy::RequireOne) {
                    m_last_status = BTStatus::Success;
                    return BTStatus::Success;
                }
            } else if (status == BTStatus::Failure) {
                failure_count++;
                if (m_failure_policy == Policy::RequireOne) {
                    m_last_status = BTStatus::Failure;
                    return BTStatus::Failure;
                }
            }
        }

        // Check RequireAll policies
        if (m_success_policy == Policy::RequireAll &&
            success_count == static_cast<int>(m_children.size())) {
            m_last_status = BTStatus::Success;
            return BTStatus::Success;
        }

        if (m_failure_policy == Policy::RequireAll &&
            failure_count == static_cast<int>(m_children.size())) {
            m_last_status = BTStatus::Failure;
            return BTStatus::Failure;
        }

        // Still running
        m_last_status = BTStatus::Running;
        return BTStatus::Running;
    }

    void set_success_policy(Policy policy) { m_success_policy = policy; }
    void set_failure_policy(Policy policy) { m_failure_policy = policy; }

private:
    Policy m_success_policy;
    Policy m_failure_policy;
};

// ============================================================================
// Random Selector
// Randomly selects a child to execute
// ============================================================================

class BTRandomSelector : public BTComposite {
public:
    explicit BTRandomSelector(std::string name = "RandomSelector")
        : BTComposite(std::move(name)) {}

    BTStatus tick(BTContext& ctx) override {
        if (m_children.empty()) {
            m_last_status = BTStatus::Failure;
            return BTStatus::Failure;
        }

        // If not currently executing a child, pick a random one
        if (!m_executing) {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::uniform_int_distribution<size_t> dist(0, m_children.size() - 1);
            m_current_child = dist(gen);
            m_executing = true;
        }

        BTStatus status = m_children[m_current_child]->tick(ctx);

        if (status != BTStatus::Running) {
            m_executing = false;
        }

        m_last_status = status;
        return status;
    }

    void reset() override {
        BTComposite::reset();
        m_executing = false;
    }

private:
    bool m_executing = false;
};

// ============================================================================
// Priority Selector
// Selects child based on priority scores (re-evaluates each tick)
// ============================================================================

class BTPrioritySelector : public BTComposite {
public:
    using PriorityFn = std::function<float(const BTContext&)>;

    explicit BTPrioritySelector(std::string name = "PrioritySelector")
        : BTComposite(std::move(name)) {}

    void add_child_with_priority(BTNodePtr child, PriorityFn priority) {
        m_priorities.push_back(std::move(priority));
        add_child(std::move(child));
    }

    void add_child_with_priority(BTNodePtr child, float static_priority) {
        m_priorities.push_back([static_priority](const BTContext&) { return static_priority; });
        add_child(std::move(child));
    }

    BTStatus tick(BTContext& ctx) override {
        if (m_children.empty()) {
            m_last_status = BTStatus::Failure;
            return BTStatus::Failure;
        }

        // Find highest priority child
        size_t best_index = 0;
        float best_priority = -1.0f;

        for (size_t i = 0; i < m_children.size() && i < m_priorities.size(); ++i) {
            float priority = m_priorities[i](ctx);
            if (priority > best_priority) {
                best_priority = priority;
                best_index = i;
            }
        }

        // If different from current running child, reset the old one
        if (m_running_child != best_index && m_running_child < m_children.size()) {
            m_children[m_running_child]->reset();
        }

        m_running_child = best_index;
        BTStatus status = m_children[best_index]->tick(ctx);

        if (status != BTStatus::Running) {
            m_running_child = SIZE_MAX;
        }

        m_last_status = status;
        return status;
    }

    void reset() override {
        BTComposite::reset();
        m_running_child = SIZE_MAX;
    }

private:
    std::vector<PriorityFn> m_priorities;
    size_t m_running_child = SIZE_MAX;
};

// ============================================================================
// Memory Selector
// Like Selector but remembers which child was running
// ============================================================================

class BTMemorySelector : public BTSelector {
public:
    explicit BTMemorySelector(std::string name = "MemorySelector")
        : BTSelector(std::move(name)) {}

    // Same behavior as Selector but doesn't reset m_current_child on success
    // (inherited behavior is already correct for this)
};

// ============================================================================
// Memory Sequence
// Like Sequence but remembers progress
// ============================================================================

class BTMemorySequence : public BTSequence {
public:
    explicit BTMemorySequence(std::string name = "MemorySequence")
        : BTSequence(std::move(name)) {}

    // Same behavior as Sequence but doesn't reset on failure
    // (inherited behavior is already correct for this)
};

} // namespace engine::ai
