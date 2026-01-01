#pragma once

#include <engine/render/animation_graph.hpp>
#include <engine/render/blend_tree.hpp>
#include <engine/render/skeleton.hpp>
#include <engine/core/math.hpp>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

namespace engine::render {

using namespace engine::core;

// Transition condition operators
enum class ConditionOperator {
    Equals,
    NotEquals,
    Greater,
    Less,
    GreaterOrEqual,
    LessOrEqual
};

// Condition for triggering a transition
struct TransitionCondition {
    std::string parameter;
    ConditionOperator op = ConditionOperator::Equals;
    float value = 0.0f;

    bool evaluate(const AnimationContext& ctx) const;
};

// Transition between animation states
struct AnimationTransition {
    std::string from_state;  // Empty string for "Any State" transitions
    std::string to_state;

    // Conditions (all must be true for transition to trigger)
    std::vector<TransitionCondition> conditions;

    // Transition timing
    float duration = 0.25f;            // Blend duration in seconds
    float exit_time = -1.0f;           // Normalized time to exit (-1 = any time)
    bool has_exit_time = false;        // If true, wait for exit_time before transitioning
    float offset = 0.0f;               // Start offset in destination state (0-1)

    // Transition behavior
    bool can_transition_to_self = false;
    bool interrupt_source = true;      // Interrupt if new transition starts
    int priority = 0;                  // Higher = takes precedence

    // Evaluate if this transition should trigger
    bool should_trigger(const AnimationContext& ctx, float normalized_time) const;
};

// Animation state (node in the state machine)
struct AnimGraphState {
    std::string name;
    std::unique_ptr<IAnimGraphNode> motion;  // Blend tree or single clip

    // Playback settings
    float speed = 1.0f;
    std::string speed_parameter;  // If set, multiply speed by this parameter
    bool loop = true;

    // Foot IK / Root motion
    bool apply_foot_ik = false;
    bool apply_root_motion = false;

    // State events
    std::vector<AnimationEvent> events;

    // Internal state
    float time = 0.0f;
    float normalized_time = 0.0f;
};

// Root motion data extracted during animation evaluation
struct RootMotionData {
    Vec3 translation_delta{0.0f};
    Quat rotation_delta{1.0f, 0.0f, 0.0f, 0.0f};

    void reset() {
        translation_delta = Vec3{0.0f};
        rotation_delta = Quat{1.0f, 0.0f, 0.0f, 0.0f};
    }
};

// Animation state machine
class AnimationStateMachine {
public:
    AnimationStateMachine() = default;
    ~AnimationStateMachine() = default;

    // Non-copyable but movable
    AnimationStateMachine(const AnimationStateMachine&) = delete;
    AnimationStateMachine& operator=(const AnimationStateMachine&) = delete;
    AnimationStateMachine(AnimationStateMachine&&) = default;
    AnimationStateMachine& operator=(AnimationStateMachine&&) = default;

    // Skeleton
    void set_skeleton(const Skeleton* skeleton) { m_skeleton = skeleton; }
    const Skeleton* get_skeleton() const { return m_skeleton; }

    // State management
    void add_state(const std::string& name, std::unique_ptr<IAnimGraphNode> motion);
    void remove_state(const std::string& name);
    AnimGraphState* get_state(const std::string& name);
    const AnimGraphState* get_state(const std::string& name) const;
    void set_default_state(const std::string& name) { m_default_state = name; }
    const std::string& get_default_state() const { return m_default_state; }

    // Transition management
    void add_transition(AnimationTransition transition);
    void add_any_state_transition(AnimationTransition transition);
    void remove_transitions_from(const std::string& from_state);
    void clear_transitions();

    // Parameter management
    void add_parameter(const std::string& name, AnimationParameter::Type type);
    void set_float(const std::string& name, float value);
    void set_int(const std::string& name, int value);
    void set_bool(const std::string& name, bool value);
    void set_trigger(const std::string& name);
    float get_float(const std::string& name) const;
    int get_int(const std::string& name) const;
    bool get_bool(const std::string& name) const;
    void reset_trigger(const std::string& name);
    bool has_parameter(const std::string& name) const;

    // State machine update
    void update(float delta_time);

    // Get the final pose after update
    const std::vector<BoneTransform>& get_pose() const { return m_final_pose; }
    std::vector<BoneTransform>& get_pose() { return m_final_pose; }

    // Get root motion delta (call after update)
    const RootMotionData& get_root_motion() const { return m_root_motion; }

    // State queries
    const std::string& get_current_state_name() const { return m_current_state; }
    bool is_in_transition() const { return m_is_transitioning; }
    float get_transition_progress() const { return m_transition_progress; }
    float get_current_normalized_time() const;

    // Force immediate state change (no transition)
    void set_state(const std::string& state_name);

    // Event callback
    using EventCallback = std::function<void(const std::string& state, const std::string& event)>;
    void set_event_callback(EventCallback callback) { m_event_callback = callback; }

    // Layer support (for partial body animations)
    struct Layer {
        std::string name;
        AnimationStateMachine* state_machine = nullptr;  // Sub-state machine for this layer
        std::vector<float> bone_mask;  // Per-bone mask (0 = base layer, 1 = this layer)
        float weight = 1.0f;
        AnimationBlendMode blend_mode = AnimationBlendMode::Override;
    };
    void add_layer(const std::string& name, float weight = 1.0f);
    void set_layer_mask(const std::string& layer_name, const std::vector<int32_t>& bone_indices);
    void set_layer_weight(const std::string& layer_name, float weight);
    Layer* get_layer(const std::string& name);

    // Reset to initial state
    void reset();

    // Start/stop the state machine
    void start();
    void stop() { m_is_running = false; }
    bool is_running() const { return m_is_running; }

private:
    void evaluate_transitions();
    void start_transition(const AnimationTransition& transition);
    void update_transition(float delta_time);
    void finish_transition();
    void evaluate_current_state(float delta_time);
    void apply_layers();
    void check_events(AnimGraphState& state, float prev_time, float curr_time);
    void reset_consumed_triggers();

    const Skeleton* m_skeleton = nullptr;

    // States
    std::unordered_map<std::string, AnimGraphState> m_states;
    std::string m_default_state;
    std::string m_current_state;
    std::string m_previous_state;

    // Transitions
    std::vector<AnimationTransition> m_transitions;
    std::vector<AnimationTransition> m_any_state_transitions;

    // Parameters
    std::unordered_map<std::string, AnimationParameter> m_parameters;
    std::vector<std::string> m_consumed_triggers;  // Triggers to reset after update

    // Current transition state
    bool m_is_transitioning = false;
    AnimationTransition m_active_transition;
    float m_transition_time = 0.0f;
    float m_transition_progress = 0.0f;
    std::vector<BoneTransform> m_transition_from_pose;

    // Layers
    std::vector<Layer> m_layers;

    // Output
    std::vector<BoneTransform> m_final_pose;
    std::vector<BoneTransform> m_temp_pose;
    RootMotionData m_root_motion;

    // Animation context
    AnimationContext m_context;

    // Event handling
    EventCallback m_event_callback;

    // Running state
    bool m_is_running = false;
    bool m_first_update = true;
};

// ECS Component for entities with animation state machines
struct AnimatorComponent {
    std::shared_ptr<AnimationStateMachine> state_machine;
    SkeletonInstance skeleton_instance;

    // Root motion application
    bool apply_root_motion = true;
    Vec3 accumulated_root_translation{0.0f};
    Quat accumulated_root_rotation{1.0f, 0.0f, 0.0f, 0.0f};

    // Initialize with skeleton
    void init(const Skeleton* skeleton) {
        state_machine = std::make_shared<AnimationStateMachine>();
        state_machine->set_skeleton(skeleton);
        skeleton_instance.set_skeleton(skeleton);
    }
};

} // namespace engine::render
