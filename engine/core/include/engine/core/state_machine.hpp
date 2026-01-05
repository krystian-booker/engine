#pragma once

#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include <variant>

namespace engine::core {

// ============================================================================
// State - Base interface for state definitions
// ============================================================================

template<typename Context>
class IState {
public:
    virtual ~IState() = default;

    // Called when entering this state
    virtual void on_enter(Context& ctx) {}

    // Called every update while in this state
    virtual void on_update(Context& ctx, float dt) {}

    // Called when exiting this state
    virtual void on_exit(Context& ctx) {}

    // State name for debugging/serialization
    virtual const std::string& get_name() const = 0;
};

// ============================================================================
// LambdaState - State defined by lambda functions
// ============================================================================

template<typename Context>
class LambdaState : public IState<Context> {
public:
    using EnterFn = std::function<void(Context&)>;
    using UpdateFn = std::function<void(Context&, float)>;
    using ExitFn = std::function<void(Context&)>;

    explicit LambdaState(std::string name,
                         EnterFn on_enter = nullptr,
                         UpdateFn on_update = nullptr,
                         ExitFn on_exit = nullptr)
        : m_name(std::move(name))
        , m_on_enter(std::move(on_enter))
        , m_on_update(std::move(on_update))
        , m_on_exit(std::move(on_exit)) {}

    void on_enter(Context& ctx) override {
        if (m_on_enter) m_on_enter(ctx);
    }

    void on_update(Context& ctx, float dt) override {
        if (m_on_update) m_on_update(ctx, dt);
    }

    void on_exit(Context& ctx) override {
        if (m_on_exit) m_on_exit(ctx);
    }

    const std::string& get_name() const override {
        return m_name;
    }

private:
    std::string m_name;
    EnterFn m_on_enter;
    UpdateFn m_on_update;
    ExitFn m_on_exit;
};

// ============================================================================
// Transition - Defines when to switch between states
// ============================================================================

template<typename Context>
struct Transition {
    using ConditionFn = std::function<bool(const Context&)>;

    std::string from_state;     // Empty = "any state" transition
    std::string to_state;
    ConditionFn condition;
    int priority = 0;           // Higher priority transitions are checked first

    bool can_trigger(const Context& ctx) const {
        return condition && condition(ctx);
    }
};

// ============================================================================
// StateMachine - Generic finite state machine
// ============================================================================

template<typename Context>
class StateMachine {
public:
    using StatePtr = std::unique_ptr<IState<Context>>;
    using ConditionFn = typename Transition<Context>::ConditionFn;

    StateMachine() = default;
    ~StateMachine() = default;

    // Non-copyable but movable
    StateMachine(const StateMachine&) = delete;
    StateMachine& operator=(const StateMachine&) = delete;
    StateMachine(StateMachine&&) = default;
    StateMachine& operator=(StateMachine&&) = default;

    // ========================================================================
    // State Management
    // ========================================================================

    // Add a state (takes ownership)
    void add_state(StatePtr state) {
        if (state) {
            std::string name = state->get_name();
            m_states[name] = std::move(state);
        }
    }

    // Add a lambda-based state
    void add_state(const std::string& name,
                   typename LambdaState<Context>::EnterFn on_enter = nullptr,
                   typename LambdaState<Context>::UpdateFn on_update = nullptr,
                   typename LambdaState<Context>::ExitFn on_exit = nullptr) {
        m_states[name] = std::make_unique<LambdaState<Context>>(
            name, std::move(on_enter), std::move(on_update), std::move(on_exit));
    }

    // Remove a state
    void remove_state(const std::string& name) {
        m_states.erase(name);
    }

    // Get state by name
    IState<Context>* get_state(const std::string& name) {
        auto it = m_states.find(name);
        return it != m_states.end() ? it->second.get() : nullptr;
    }

    const IState<Context>* get_state(const std::string& name) const {
        auto it = m_states.find(name);
        return it != m_states.end() ? it->second.get() : nullptr;
    }

    // Check if state exists
    bool has_state(const std::string& name) const {
        return m_states.find(name) != m_states.end();
    }

    // Set the initial state (must be called before first update)
    void set_initial_state(const std::string& name) {
        m_initial_state = name;
    }

    // ========================================================================
    // Transition Management
    // ========================================================================

    // Add a transition from one state to another
    void add_transition(const std::string& from, const std::string& to,
                        ConditionFn condition, int priority = 0) {
        Transition<Context> transition;
        transition.from_state = from;
        transition.to_state = to;
        transition.condition = std::move(condition);
        transition.priority = priority;
        m_transitions.push_back(std::move(transition));
        sort_transitions();
    }

    // Add a transition from any state
    void add_any_transition(const std::string& to, ConditionFn condition, int priority = 0) {
        Transition<Context> transition;
        transition.from_state = "";  // Empty = any state
        transition.to_state = to;
        transition.condition = std::move(condition);
        transition.priority = priority;
        m_any_transitions.push_back(std::move(transition));
        sort_transitions();
    }

    // Remove all transitions from a state
    void remove_transitions_from(const std::string& from) {
        m_transitions.erase(
            std::remove_if(m_transitions.begin(), m_transitions.end(),
                [&from](const Transition<Context>& t) { return t.from_state == from; }),
            m_transitions.end()
        );
    }

    // Clear all transitions
    void clear_transitions() {
        m_transitions.clear();
        m_any_transitions.clear();
    }

    // ========================================================================
    // Update
    // ========================================================================

    // Update the state machine - evaluates transitions and updates current state
    void update(Context& ctx, float dt) {
        if (!m_started) {
            start(ctx);
        }

        // Evaluate transitions
        evaluate_transitions(ctx);

        // Update current state
        if (auto* state = get_state(m_current_state)) {
            state->on_update(ctx, dt);
        }

        // Update time in state
        m_time_in_state += dt;
    }

    // ========================================================================
    // State Control
    // ========================================================================

    // Start the state machine (enters initial state)
    void start(Context& ctx) {
        if (m_started) return;

        m_current_state = m_initial_state;
        m_previous_state = "";
        m_time_in_state = 0.0f;
        m_started = true;

        if (auto* state = get_state(m_current_state)) {
            state->on_enter(ctx);
        }
    }

    // Force immediate state change (no transition evaluation)
    void set_state(Context& ctx, const std::string& name) {
        if (name == m_current_state) return;
        if (!has_state(name)) return;

        // Exit current state
        if (auto* current = get_state(m_current_state)) {
            current->on_exit(ctx);
        }

        m_previous_state = m_current_state;
        m_current_state = name;
        m_time_in_state = 0.0f;

        // Enter new state
        if (auto* new_state = get_state(name)) {
            new_state->on_enter(ctx);
        }
    }

    // Stop the state machine
    void stop(Context& ctx) {
        if (!m_started) return;

        if (auto* state = get_state(m_current_state)) {
            state->on_exit(ctx);
        }

        m_started = false;
    }

    // Reset to initial state
    void reset(Context& ctx) {
        stop(ctx);
        start(ctx);
    }

    // ========================================================================
    // Queries
    // ========================================================================

    const std::string& get_current_state() const { return m_current_state; }
    const std::string& get_previous_state() const { return m_previous_state; }
    float get_time_in_state() const { return m_time_in_state; }
    bool is_started() const { return m_started; }

    bool is_in_state(const std::string& name) const {
        return m_current_state == name;
    }

    // ========================================================================
    // Serialization
    // ========================================================================

    std::string serialize() const {
        return m_current_state;
    }

    void deserialize(Context& ctx, const std::string& state_name) {
        if (has_state(state_name)) {
            if (!m_started) {
                m_initial_state = state_name;
                start(ctx);
            } else {
                set_state(ctx, state_name);
            }
        }
    }

private:
    void sort_transitions() {
        auto comparator = [](const Transition<Context>& a, const Transition<Context>& b) {
            return a.priority > b.priority;  // Higher priority first
        };
        std::sort(m_transitions.begin(), m_transitions.end(), comparator);
        std::sort(m_any_transitions.begin(), m_any_transitions.end(), comparator);
    }

    void evaluate_transitions(Context& ctx) {
        // Check any-state transitions first (usually higher priority)
        for (const auto& transition : m_any_transitions) {
            if (transition.to_state != m_current_state && transition.can_trigger(ctx)) {
                set_state(ctx, transition.to_state);
                return;
            }
        }

        // Check state-specific transitions
        for (const auto& transition : m_transitions) {
            if (transition.from_state == m_current_state &&
                transition.can_trigger(ctx)) {
                set_state(ctx, transition.to_state);
                return;
            }
        }
    }

    std::unordered_map<std::string, StatePtr> m_states;
    std::string m_initial_state;
    std::string m_current_state;
    std::string m_previous_state;

    std::vector<Transition<Context>> m_transitions;
    std::vector<Transition<Context>> m_any_transitions;

    float m_time_in_state = 0.0f;
    bool m_started = false;
};

// ============================================================================
// HierarchicalStateMachine - FSM with state stack support
// ============================================================================

template<typename Context>
class HierarchicalStateMachine : public StateMachine<Context> {
public:
    // Push a sub-state onto the stack
    void push_state(Context& ctx, const std::string& name) {
        if (!this->has_state(name)) return;

        // Don't exit current state, just pause it
        m_state_stack.push_back(this->get_current_state());

        // Directly enter new state without calling exit on current
        std::string prev = this->get_current_state();
        m_previous_for_push = prev;

        // Manually set state (bypass normal set_state to avoid exit)
        if (auto* state = this->get_state(name)) {
            state->on_enter(ctx);
        }
        m_pushed_state = name;
        m_has_pushed = true;
    }

    // Pop back to previous state
    void pop_state(Context& ctx) {
        if (m_state_stack.empty()) return;

        // Exit pushed state
        if (m_has_pushed && !m_pushed_state.empty()) {
            if (auto* state = this->get_state(m_pushed_state)) {
                state->on_exit(ctx);
            }
        }

        // Restore previous state (re-enter it)
        std::string prev = m_state_stack.back();
        m_state_stack.pop_back();

        this->set_state(ctx, prev);
        m_has_pushed = false;
        m_pushed_state.clear();
    }

    // Get the full state path (e.g., "Combat/Attacking")
    std::string get_state_path() const {
        std::string path;
        for (const auto& state : m_state_stack) {
            path += state + "/";
        }
        if (m_has_pushed && !m_pushed_state.empty()) {
            path += m_pushed_state;
        } else {
            path += this->get_current_state();
        }
        return path;
    }

    // Get stack depth
    size_t get_stack_depth() const { return m_state_stack.size(); }

    // Check if we're in a pushed state
    bool is_in_pushed_state() const { return m_has_pushed; }

private:
    std::vector<std::string> m_state_stack;
    std::string m_pushed_state;
    std::string m_previous_for_push;
    bool m_has_pushed = false;
};

// ============================================================================
// StateMachineComponent - ECS component for entity state machines
// ============================================================================

struct StateMachineComponent {
    std::string current_state;
    std::string previous_state;
    float state_time = 0.0f;

    // Blackboard for condition parameters
    std::unordered_map<std::string, float> float_params;
    std::unordered_map<std::string, int> int_params;
    std::unordered_map<std::string, bool> bool_params;
    std::unordered_map<std::string, std::string> string_params;

    // Set parameter
    void set(const std::string& name, float value) { float_params[name] = value; }
    void set(const std::string& name, int value) { int_params[name] = value; }
    void set(const std::string& name, bool value) { bool_params[name] = value; }
    void set_string(const std::string& name, const std::string& value) { string_params[name] = value; }

    // Get parameter with default
    float get_float(const std::string& name, float def = 0.0f) const {
        auto it = float_params.find(name);
        return it != float_params.end() ? it->second : def;
    }

    int get_int(const std::string& name, int def = 0) const {
        auto it = int_params.find(name);
        return it != int_params.end() ? it->second : def;
    }

    bool get_bool(const std::string& name, bool def = false) const {
        auto it = bool_params.find(name);
        return it != bool_params.end() ? it->second : def;
    }

    std::string get_string(const std::string& name, const std::string& def = "") const {
        auto it = string_params.find(name);
        return it != string_params.end() ? it->second : def;
    }

    // Check if parameter exists
    bool has_float(const std::string& name) const {
        return float_params.find(name) != float_params.end();
    }

    bool has_int(const std::string& name) const {
        return int_params.find(name) != int_params.end();
    }

    bool has_bool(const std::string& name) const {
        return bool_params.find(name) != bool_params.end();
    }

    bool has_string(const std::string& name) const {
        return string_params.find(name) != string_params.end();
    }

    // Clear all parameters
    void clear_params() {
        float_params.clear();
        int_params.clear();
        bool_params.clear();
        string_params.clear();
    }
};

} // namespace engine::core
