#include <engine/render/animation_state_machine.hpp>
#include <algorithm>
#include <cmath>

namespace engine::render {

// TransitionCondition implementation

bool TransitionCondition::evaluate(const AnimationContext& ctx) const {
    float param_value = ctx.get_float(parameter);

    switch (op) {
        case ConditionOperator::Equals:
            return std::abs(param_value - value) < 0.0001f;
        case ConditionOperator::NotEquals:
            return std::abs(param_value - value) >= 0.0001f;
        case ConditionOperator::Greater:
            return param_value > value;
        case ConditionOperator::Less:
            return param_value < value;
        case ConditionOperator::GreaterOrEqual:
            return param_value >= value;
        case ConditionOperator::LessOrEqual:
            return param_value <= value;
    }
    return false;
}

// AnimationTransition implementation

bool AnimationTransition::should_trigger(const AnimationContext& ctx, float normalized_time) const {
    // Check exit time condition
    if (has_exit_time) {
        if (exit_time >= 0.0f && normalized_time < exit_time) {
            return false;
        }
    }

    // Check all conditions
    for (const auto& condition : conditions) {
        if (!condition.evaluate(ctx)) {
            return false;
        }
    }

    return true;
}

// AnimationStateMachine implementation

void AnimationStateMachine::add_state(const std::string& name, std::unique_ptr<IAnimGraphNode> motion) {
    AnimGraphState state;
    state.name = name;
    state.motion = std::move(motion);
    m_states[name] = std::move(state);

    // Set as default if first state
    if (m_default_state.empty()) {
        m_default_state = name;
    }
}

void AnimationStateMachine::remove_state(const std::string& name) {
    m_states.erase(name);
    if (m_default_state == name) {
        m_default_state = m_states.empty() ? "" : m_states.begin()->first;
    }
    if (m_current_state == name) {
        m_current_state = m_default_state;
    }
}

AnimGraphState* AnimationStateMachine::get_state(const std::string& name) {
    auto it = m_states.find(name);
    return it != m_states.end() ? &it->second : nullptr;
}

const AnimGraphState* AnimationStateMachine::get_state(const std::string& name) const {
    auto it = m_states.find(name);
    return it != m_states.end() ? &it->second : nullptr;
}

void AnimationStateMachine::add_transition(AnimationTransition transition) {
    m_transitions.push_back(std::move(transition));
    // Sort by priority (higher priority first)
    std::sort(m_transitions.begin(), m_transitions.end(),
        [](const AnimationTransition& a, const AnimationTransition& b) {
            return a.priority > b.priority;
        });
}

void AnimationStateMachine::add_any_state_transition(AnimationTransition transition) {
    transition.from_state = "";  // Mark as any-state transition
    m_any_state_transitions.push_back(std::move(transition));
    std::sort(m_any_state_transitions.begin(), m_any_state_transitions.end(),
        [](const AnimationTransition& a, const AnimationTransition& b) {
            return a.priority > b.priority;
        });
}

void AnimationStateMachine::remove_transitions_from(const std::string& from_state) {
    m_transitions.erase(
        std::remove_if(m_transitions.begin(), m_transitions.end(),
            [&from_state](const AnimationTransition& t) {
                return t.from_state == from_state;
            }),
        m_transitions.end()
    );
}

void AnimationStateMachine::clear_transitions() {
    m_transitions.clear();
    m_any_state_transitions.clear();
}

void AnimationStateMachine::add_parameter(const std::string& name, AnimationParameter::Type type) {
    AnimationParameter param;
    param.type = type;
    switch (type) {
        case AnimationParameter::Type::Float:
            param.value = 0.0f;
            break;
        case AnimationParameter::Type::Int:
            param.value = 0;
            break;
        case AnimationParameter::Type::Bool:
        case AnimationParameter::Type::Trigger:
            param.value = false;
            break;
    }
    m_parameters[name] = param;
}

void AnimationStateMachine::set_float(const std::string& name, float value) {
    auto it = m_parameters.find(name);
    if (it != m_parameters.end()) {
        it->second.value = value;
    }
}

void AnimationStateMachine::set_int(const std::string& name, int value) {
    auto it = m_parameters.find(name);
    if (it != m_parameters.end()) {
        it->second.value = value;
    }
}

void AnimationStateMachine::set_bool(const std::string& name, bool value) {
    auto it = m_parameters.find(name);
    if (it != m_parameters.end()) {
        it->second.value = value;
    }
}

void AnimationStateMachine::set_trigger(const std::string& name) {
    auto it = m_parameters.find(name);
    if (it != m_parameters.end() && it->second.type == AnimationParameter::Type::Trigger) {
        it->second.value = true;
    }
}

float AnimationStateMachine::get_float(const std::string& name) const {
    auto it = m_parameters.find(name);
    return it != m_parameters.end() ? it->second.as_float() : 0.0f;
}

int AnimationStateMachine::get_int(const std::string& name) const {
    auto it = m_parameters.find(name);
    return it != m_parameters.end() ? it->second.as_int() : 0;
}

bool AnimationStateMachine::get_bool(const std::string& name) const {
    auto it = m_parameters.find(name);
    return it != m_parameters.end() ? it->second.as_bool() : false;
}

void AnimationStateMachine::reset_trigger(const std::string& name) {
    auto it = m_parameters.find(name);
    if (it != m_parameters.end() && it->second.type == AnimationParameter::Type::Trigger) {
        it->second.value = false;
    }
}

bool AnimationStateMachine::has_parameter(const std::string& name) const {
    return m_parameters.find(name) != m_parameters.end();
}

void AnimationStateMachine::update(float delta_time) {
    if (!m_is_running) {
        return;
    }

    // First update - initialize to default state
    if (m_first_update) {
        m_first_update = false;
        if (m_current_state.empty() && !m_default_state.empty()) {
            set_state(m_default_state);
        }
    }

    // Set up context
    m_context.parameters = &m_parameters;
    m_context.skeleton = m_skeleton;
    m_context.delta_time = delta_time;

    // Reset root motion
    m_root_motion.reset();

    // Check for transitions
    if (!m_is_transitioning) {
        evaluate_transitions();
    }

    // Update transition or current state
    if (m_is_transitioning) {
        update_transition(delta_time);
    } else {
        evaluate_current_state(delta_time);
    }

    // Apply layers on top of base pose
    apply_layers();

    // Reset consumed triggers
    reset_consumed_triggers();
}

void AnimationStateMachine::evaluate_transitions() {
    if (m_current_state.empty()) {
        return;
    }

    AnimGraphState* current = get_state(m_current_state);
    if (!current) {
        return;
    }

    float normalized_time = current->normalized_time;

    // Check any-state transitions first (typically higher priority)
    for (const auto& transition : m_any_state_transitions) {
        if (transition.to_state == m_current_state && !transition.can_transition_to_self) {
            continue;
        }
        if (transition.should_trigger(m_context, normalized_time)) {
            start_transition(transition);

            // Mark consumed triggers
            for (const auto& cond : transition.conditions) {
                auto it = m_parameters.find(cond.parameter);
                if (it != m_parameters.end() && it->second.type == AnimationParameter::Type::Trigger) {
                    m_consumed_triggers.push_back(cond.parameter);
                }
            }
            return;
        }
    }

    // Check state-specific transitions
    for (const auto& transition : m_transitions) {
        if (transition.from_state != m_current_state) {
            continue;
        }
        if (transition.should_trigger(m_context, normalized_time)) {
            start_transition(transition);

            // Mark consumed triggers
            for (const auto& cond : transition.conditions) {
                auto it = m_parameters.find(cond.parameter);
                if (it != m_parameters.end() && it->second.type == AnimationParameter::Type::Trigger) {
                    m_consumed_triggers.push_back(cond.parameter);
                }
            }
            return;
        }
    }
}

void AnimationStateMachine::start_transition(const AnimationTransition& transition) {
    m_is_transitioning = true;
    m_active_transition = transition;
    m_transition_time = 0.0f;
    m_transition_progress = 0.0f;
    m_previous_state = m_current_state;

    // Store current pose for blending
    m_transition_from_pose = m_final_pose;

    // Set up destination state
    AnimGraphState* dest_state = get_state(transition.to_state);
    if (dest_state) {
        if (dest_state->motion) {
            dest_state->motion->reset();
            // Apply offset
            if (transition.offset > 0.0f) {
                float duration = dest_state->motion->get_duration();
                dest_state->motion->set_time(transition.offset * duration);
            }
        }
        dest_state->time = 0.0f;
        dest_state->normalized_time = 0.0f;
    }

    m_current_state = transition.to_state;
}

void AnimationStateMachine::update_transition(float delta_time) {
    if (m_active_transition.duration <= 0.0f) {
        finish_transition();
        return;
    }

    m_transition_time += delta_time;
    m_transition_progress = std::clamp(m_transition_time / m_active_transition.duration, 0.0f, 1.0f);

    // Evaluate destination state
    AnimGraphState* dest_state = get_state(m_current_state);
    if (dest_state && dest_state->motion) {
        float speed = dest_state->speed;
        if (!dest_state->speed_parameter.empty()) {
            speed *= m_context.get_float(dest_state->speed_parameter);
        }

        float prev_time = dest_state->time;
        dest_state->motion->evaluate(delta_time * speed, m_context, m_temp_pose);
        dest_state->time = dest_state->motion->get_time();

        float duration = dest_state->motion->get_duration();
        dest_state->normalized_time = duration > 0.0f ? dest_state->time / duration : 0.0f;

        // Check events
        check_events(*dest_state, prev_time, dest_state->time);
    }

    // Blend from stored pose to new pose
    blend_poses(m_transition_from_pose, m_temp_pose, m_transition_progress, m_final_pose);

    // Check if transition is complete
    if (m_transition_progress >= 1.0f) {
        finish_transition();
    }
}

void AnimationStateMachine::finish_transition() {
    m_is_transitioning = false;
    m_transition_time = 0.0f;
    m_transition_progress = 0.0f;
    m_previous_state.clear();
}

void AnimationStateMachine::evaluate_current_state(float delta_time) {
    AnimGraphState* state = get_state(m_current_state);
    if (!state || !state->motion) {
        return;
    }

    float speed = state->speed;
    if (!state->speed_parameter.empty()) {
        speed *= m_context.get_float(state->speed_parameter);
    }

    float prev_time = state->time;
    state->motion->evaluate(delta_time * speed, m_context, m_final_pose);
    state->time = state->motion->get_time();

    float duration = state->motion->get_duration();
    state->normalized_time = duration > 0.0f ? state->time / duration : 0.0f;

    // Check events
    check_events(*state, prev_time, state->time);
}

void AnimationStateMachine::apply_layers() {
    if (m_layers.empty()) {
        return;
    }

    for (auto& layer : m_layers) {
        if (layer.weight <= 0.001f || !layer.state_machine) {
            continue;
        }

        // Update the layer's state machine
        layer.state_machine->update(m_context.delta_time);
        const auto& layer_pose = layer.state_machine->get_pose();

        if (layer_pose.empty()) {
            continue;
        }

        switch (layer.blend_mode) {
            case AnimationBlendMode::Override:
                if (layer.bone_mask.empty()) {
                    blend_poses(m_final_pose, layer_pose, layer.weight, m_final_pose);
                } else {
                    blend_poses_masked(m_final_pose, layer_pose, layer.weight, layer.bone_mask, m_final_pose);
                }
                break;
            case AnimationBlendMode::Additive:
                add_pose(m_final_pose, layer_pose, layer.weight, m_final_pose);
                break;
            case AnimationBlendMode::Blend:
                blend_poses(m_final_pose, layer_pose, layer.weight, m_final_pose);
                break;
        }
    }
}

void AnimationStateMachine::check_events(AnimGraphState& state, float prev_time, float curr_time) {
    if (!m_event_callback) {
        return;
    }

    for (const auto& event : state.events) {
        // Check if event time was crossed
        bool crossed = false;
        if (curr_time >= prev_time) {
            // Normal playback
            crossed = event.time > prev_time && event.time <= curr_time;
        } else {
            // Looped
            float duration = state.motion ? state.motion->get_duration() : 0.0f;
            crossed = event.time > prev_time || event.time <= curr_time;
        }

        if (crossed) {
            m_event_callback(state.name, event.name);
        }
    }
}

void AnimationStateMachine::reset_consumed_triggers() {
    for (const auto& trigger_name : m_consumed_triggers) {
        reset_trigger(trigger_name);
    }
    m_consumed_triggers.clear();
}

float AnimationStateMachine::get_current_normalized_time() const {
    const AnimGraphState* state = get_state(m_current_state);
    return state ? state->normalized_time : 0.0f;
}

void AnimationStateMachine::set_state(const std::string& state_name) {
    AnimGraphState* state = get_state(state_name);
    if (!state) {
        return;
    }

    m_current_state = state_name;
    m_is_transitioning = false;

    if (state->motion) {
        state->motion->reset();
        state->time = 0.0f;
        state->normalized_time = 0.0f;

        // Initialize pose with skeleton size if available
        if (m_skeleton) {
            m_final_pose = m_skeleton->get_bind_pose();
        }

        // Evaluate once to get initial pose
        AnimationContext ctx;
        ctx.parameters = &m_parameters;
        ctx.skeleton = m_skeleton;
        ctx.delta_time = 0.0f;
        state->motion->evaluate(0.0f, ctx, m_final_pose);
    }
}

void AnimationStateMachine::add_layer(const std::string& name, float weight) {
    Layer layer;
    layer.name = name;
    layer.weight = weight;
    layer.state_machine = nullptr;  // Will be set up by user
    m_layers.push_back(std::move(layer));
}

void AnimationStateMachine::set_layer_mask(const std::string& layer_name, const std::vector<int32_t>& bone_indices) {
    for (auto& layer : m_layers) {
        if (layer.name == layer_name) {
            // Create mask from bone indices
            if (m_skeleton) {
                layer.bone_mask.resize(m_skeleton->get_bone_count(), 0.0f);
                for (int32_t idx : bone_indices) {
                    if (idx >= 0 && idx < static_cast<int32_t>(layer.bone_mask.size())) {
                        layer.bone_mask[idx] = 1.0f;
                    }
                }
            }
            return;
        }
    }
}

void AnimationStateMachine::set_layer_weight(const std::string& layer_name, float weight) {
    for (auto& layer : m_layers) {
        if (layer.name == layer_name) {
            layer.weight = weight;
            return;
        }
    }
}

AnimationStateMachine::Layer* AnimationStateMachine::get_layer(const std::string& name) {
    for (auto& layer : m_layers) {
        if (layer.name == name) {
            return &layer;
        }
    }
    return nullptr;
}

void AnimationStateMachine::reset() {
    m_current_state.clear();
    m_previous_state.clear();
    m_is_transitioning = false;
    m_transition_time = 0.0f;
    m_transition_progress = 0.0f;
    m_first_update = true;

    // Reset all state times
    for (auto& [name, state] : m_states) {
        state.time = 0.0f;
        state.normalized_time = 0.0f;
        if (state.motion) {
            state.motion->reset();
        }
    }

    // Reset triggers
    for (auto& [name, param] : m_parameters) {
        if (param.type == AnimationParameter::Type::Trigger) {
            param.value = false;
        }
    }

    m_final_pose.clear();
    m_root_motion.reset();
}

void AnimationStateMachine::start() {
    m_is_running = true;
    m_first_update = true;
}

} // namespace engine::render
