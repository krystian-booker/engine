#include <engine/dialogue/dialogue_player.hpp>
#include <engine/dialogue/dialogue_graph.hpp>
#include <engine/core/event_dispatcher.hpp>
#include <engine/core/log.hpp>
#include <algorithm>

namespace engine::dialogue {

// ============================================================================
// Singleton Instance
// ============================================================================

DialoguePlayer::DialoguePlayer() {
    // Default text getter (just returns key)
    m_text_getter = [](const std::string& key) { return key; };
}

DialoguePlayer& DialoguePlayer::instance() {
    static DialoguePlayer s_instance;
    return s_instance;
}

// ============================================================================
// Dialogue Control
// ============================================================================

bool DialoguePlayer::start(const std::string& graph_id, scene::Entity initiator,
                            scene::Entity target) {
    DialogueGraph* graph = DialogueLibrary::instance().get_graph(graph_id);
    if (!graph) {
        core::log(core::LogLevel::Warn, "Cannot start unknown dialogue: {}", graph_id);
        return false;
    }
    return start(graph, initiator, target);
}

bool DialoguePlayer::start(DialogueGraph* graph, scene::Entity initiator,
                            scene::Entity target) {
    if (!graph) return false;

    if (m_state != DialoguePlayerState::Inactive) {
        stop("interrupted");
    }

    m_current_graph = graph;
    m_initiator = initiator;
    m_target = target;
    m_visited_nodes.clear();

    DialogueNode* entry = graph->get_entry_node();
    if (!entry) {
        core::log(core::LogLevel::Warn, "Dialogue '{}' has no valid entry point", graph->get_id());
        return false;
    }

    m_state = DialoguePlayerState::Playing;
    dispatch_event(DialogueStartedEvent{graph->get_id(), initiator, target});

    enter_node(entry->id);

    core::log(core::LogLevel::Info, "Dialogue started: {}", graph->get_id());
    return true;
}

void DialoguePlayer::stop(const std::string& reason) {
    if (m_state == DialoguePlayerState::Inactive) return;

    if (m_current_node) {
        exit_current_node();
    }

    std::string graph_id = m_current_graph ? m_current_graph->get_id() : "";

    m_state = DialoguePlayerState::Inactive;
    m_current_graph = nullptr;
    m_current_node = nullptr;
    m_initiator = scene::NullEntity;
    m_target = scene::NullEntity;

    dispatch_event(DialogueEndedEvent{graph_id, reason});

    core::log(core::LogLevel::Info, "Dialogue ended: {} ({})", graph_id, reason);
}

void DialoguePlayer::pause() {
    if (m_state == DialoguePlayerState::Playing ||
        m_state == DialoguePlayerState::WaitingForInput) {
        m_state = DialoguePlayerState::Paused;
    }
}

void DialoguePlayer::resume() {
    if (m_state == DialoguePlayerState::Paused) {
        m_state = has_choices() ? DialoguePlayerState::WaitingForInput : DialoguePlayerState::Playing;
    }
}

// ============================================================================
// Navigation
// ============================================================================

void DialoguePlayer::advance() {
    if (!m_current_node || m_state == DialoguePlayerState::Inactive) return;

    // If typewriter still revealing, skip to end
    if (m_typewriter_enabled && !is_text_complete()) {
        skip_typewriter();
        return;
    }

    // Check minimum display time
    if (m_node_time < m_current_node->min_display_time) {
        return;
    }

    // If we have choices, wait for selection
    if (has_choices()) {
        return;
    }

    // Exit current node
    exit_current_node();

    // Check for exit point
    if (m_current_node->is_exit_point || m_current_node->next_node_id.empty()) {
        stop("completed");
        return;
    }

    // Enter next node
    enter_node(m_current_node->next_node_id);
}

void DialoguePlayer::select_choice(int32_t index) {
    if (!m_current_node || m_state != DialoguePlayerState::WaitingForInput) return;

    auto choices = get_available_choices();
    if (index < 0 || index >= static_cast<int32_t>(choices.size())) return;

    const DialogueChoice* choice = choices[index];
    select_choice(choice->id);
}

void DialoguePlayer::select_choice(const std::string& id) {
    if (!m_current_node || m_state != DialoguePlayerState::WaitingForInput) return;

    const DialogueChoice* selected = nullptr;
    for (const auto& choice : m_current_node->choices) {
        if (choice.id == id) {
            selected = &choice;
            break;
        }
    }

    if (!selected) {
        core::log(core::LogLevel::Warn, "Invalid choice: {}", id);
        return;
    }

    // Check conditions
    if (!check_conditions(selected->conditions)) {
        return;
    }

    // Execute choice actions
    execute_actions(selected->actions);

    dispatch_event(DialogueChoiceMadeEvent{
        m_current_graph->get_id(),
        m_current_node->id,
        id
    });

    // Exit current node
    exit_current_node();

    // Handle exit choice
    if (selected->is_exit) {
        stop("completed");
        return;
    }

    // Enter target node
    if (!selected->target_node_id.empty()) {
        enter_node(selected->target_node_id);
    } else {
        stop("completed");
    }
}

bool DialoguePlayer::can_advance() const {
    if (!m_current_node || m_state == DialoguePlayerState::Inactive) return false;
    if (m_state == DialoguePlayerState::Paused) return false;
    if (has_choices()) return false;
    return true;
}

bool DialoguePlayer::has_choices() const {
    return m_current_node && !get_available_choices().empty();
}

// ============================================================================
// Current State Getters
// ============================================================================

const DialogueSpeaker* DialoguePlayer::get_current_speaker() const {
    if (!m_current_graph || !m_current_node) return nullptr;
    return m_current_graph->get_speaker(m_current_node->speaker_id);
}

std::string DialoguePlayer::get_current_text() const {
    return m_current_localized_text;
}

std::string DialoguePlayer::get_revealed_text() const {
    if (!m_typewriter_enabled || m_text_progress >= 1.0f) {
        return m_current_localized_text;
    }
    return m_current_localized_text.substr(0, m_revealed_chars);
}

std::vector<const DialogueChoice*> DialoguePlayer::get_available_choices() const {
    std::vector<const DialogueChoice*> result;

    if (!m_current_node) return result;

    for (const auto& choice : m_current_node->choices) {
        if (check_conditions(choice.conditions)) {
            result.push_back(&choice);
        } else if (choice.show_unavailable) {
            // Could add to separate list for grayed-out choices
        }
    }

    return result;
}

int32_t DialoguePlayer::get_choice_count() const {
    return static_cast<int32_t>(get_available_choices().size());
}

// ============================================================================
// Text Display
// ============================================================================

void DialoguePlayer::skip_typewriter() {
    m_text_progress = 1.0f;
    m_revealed_chars = m_current_localized_text.size();

    dispatch_event(DialogueTextRevealedEvent{
        m_current_graph ? m_current_graph->get_id() : "",
        m_current_node ? m_current_node->id : "",
        1.0f,
        true
    });
}

// ============================================================================
// Variables
// ============================================================================

void DialoguePlayer::set_variable(const std::string& key, const std::any& value) {
    m_variables[key] = value;
}

std::any DialoguePlayer::get_variable(const std::string& key) const {
    auto it = m_variables.find(key);
    return it != m_variables.end() ? it->second : std::any{};
}

bool DialoguePlayer::has_variable(const std::string& key) const {
    return m_variables.count(key) > 0;
}

void DialoguePlayer::clear_variables() {
    m_variables.clear();
}

// ============================================================================
// History
// ============================================================================

bool DialoguePlayer::has_visited_node(const std::string& node_id) const {
    return std::find(m_visited_nodes.begin(), m_visited_nodes.end(), node_id) != m_visited_nodes.end();
}

void DialoguePlayer::clear_history() {
    m_visited_nodes.clear();
}

void DialoguePlayer::set_npc_dialogue_state(scene::Entity npc, const std::string& key, const std::any& value) {
    m_npc_states[static_cast<uint32_t>(npc)][key] = value;
}

std::any DialoguePlayer::get_npc_dialogue_state(scene::Entity npc, const std::string& key) const {
    auto npc_it = m_npc_states.find(static_cast<uint32_t>(npc));
    if (npc_it == m_npc_states.end()) return std::any{};

    auto it = npc_it->second.find(key);
    return it != npc_it->second.end() ? it->second : std::any{};
}

// ============================================================================
// Update
// ============================================================================

void DialoguePlayer::update(float dt) {
    if (m_state == DialoguePlayerState::Inactive || m_state == DialoguePlayerState::Paused) {
        return;
    }

    if (!m_current_node) return;

    m_node_time += dt;

    // Update typewriter effect
    if (m_typewriter_enabled && m_text_progress < 1.0f) {
        float chars_to_reveal = m_typewriter_speed * dt;
        m_revealed_chars = std::min(
            m_revealed_chars + static_cast<size_t>(chars_to_reveal),
            m_current_localized_text.size()
        );

        m_text_progress = m_current_localized_text.empty() ? 1.0f :
            static_cast<float>(m_revealed_chars) / m_current_localized_text.size();

        if (m_text_progress >= 1.0f) {
            m_text_progress = 1.0f;
            dispatch_event(DialogueTextRevealedEvent{
                m_current_graph->get_id(),
                m_current_node->id,
                1.0f,
                true
            });
        }
    }

    // Auto-advance
    if (m_current_node->auto_advance_delay > 0.0f && is_text_complete()) {
        m_auto_advance_timer += dt;
        if (m_auto_advance_timer >= m_current_node->auto_advance_delay) {
            advance();
        }
    }

    // Update state based on choices
    if (is_text_complete() && has_choices()) {
        m_state = DialoguePlayerState::WaitingForInput;
    }
}

// ============================================================================
// Callbacks
// ============================================================================

void DialoguePlayer::set_action_handler(DialogueAction::Type type, ActionHandler handler) {
    m_action_handlers[type] = std::move(handler);
}

void DialoguePlayer::set_condition_checker(DialogueCondition::Type type, ConditionChecker checker) {
    m_condition_checkers[type] = std::move(checker);
}

void DialoguePlayer::set_skill_check_handler(SkillCheckHandler handler) {
    m_skill_check_handler = std::move(handler);
}

void DialoguePlayer::set_text_getter(std::function<std::string(const std::string&)> getter) {
    m_text_getter = std::move(getter);
}

// ============================================================================
// Private Methods
// ============================================================================

void DialoguePlayer::enter_node(const std::string& node_id) {
    if (!m_current_graph) return;

    DialogueNode* node = m_current_graph->get_node(node_id);
    if (!node) {
        core::log(core::LogLevel::Warn, "Cannot enter unknown node: {}", node_id);
        stop("error");
        return;
    }

    // Check once_only
    if (node->once_only && node->shown) {
        // Skip to next node
        if (!node->next_node_id.empty()) {
            enter_node(node->next_node_id);
        } else {
            stop("completed");
        }
        return;
    }

    m_current_node = node;
    m_node_time = 0.0f;
    m_auto_advance_timer = 0.0f;
    m_text_progress = m_typewriter_enabled ? 0.0f : 1.0f;
    m_revealed_chars = m_typewriter_enabled ? 0 : m_current_localized_text.size();

    // Get localized text
    m_current_localized_text = m_text_getter(node->text_key);
    if (!m_typewriter_enabled) {
        m_revealed_chars = m_current_localized_text.size();
    }

    // Mark as visited
    m_visited_nodes.push_back(node_id);
    node->shown = true;

    // Execute on_enter actions
    execute_actions(node->on_enter_actions);

    // Dispatch event
    dispatch_event(DialogueNodeEnteredEvent{
        m_current_graph->get_id(),
        node_id,
        node->speaker_id,
        node->text_key
    });

    // Play voice clip
    if (!node->voice_clip.empty()) {
        // Would integrate with audio system
    }

    // Set camera
    if (!node->camera_shot.empty()) {
        // Would integrate with cinematic system
    }

    // Update state
    if (has_choices() && is_text_complete()) {
        m_state = DialoguePlayerState::WaitingForInput;
    } else {
        m_state = DialoguePlayerState::Playing;
    }
}

void DialoguePlayer::exit_current_node() {
    if (!m_current_node) return;

    execute_actions(m_current_node->on_exit_actions);
}

void DialoguePlayer::execute_actions(const std::vector<DialogueAction>& actions) {
    for (const auto& action : actions) {
        execute_action(action);
    }
}

bool DialoguePlayer::check_conditions(const std::vector<DialogueCondition>& conditions) const {
    for (const auto& condition : conditions) {
        if (!check_condition(condition)) {
            return false;
        }
    }
    return true;
}

bool DialoguePlayer::check_condition(const DialogueCondition& condition) const {
    // Check custom checker first
    auto it = m_condition_checkers.find(condition.type);
    if (it != m_condition_checkers.end()) {
        bool result = it->second(condition);
        return condition.negate ? !result : result;
    }

    // Default: use condition's own evaluate
    return condition.evaluate();
}

void DialoguePlayer::execute_action(const DialogueAction& action) {
    // Check custom handler first
    auto it = m_action_handlers.find(action.type);
    if (it != m_action_handlers.end()) {
        it->second(action);
        return;
    }

    // Default handling
    switch (action.type) {
        case DialogueAction::Type::Custom:
            if (action.custom_action) {
                action.custom_action();
            }
            break;
        default:
            // Would integrate with game systems
            core::log(core::LogLevel::Debug, "Unhandled dialogue action type");
            break;
    }
}

void DialoguePlayer::dispatch_event(const DialogueStartedEvent& event) {
    core::events().dispatch(event);
}

void DialoguePlayer::dispatch_event(const DialogueEndedEvent& event) {
    core::events().dispatch(event);
}

void DialoguePlayer::dispatch_event(const DialogueNodeEnteredEvent& event) {
    core::events().dispatch(event);
}

void DialoguePlayer::dispatch_event(const DialogueChoiceMadeEvent& event) {
    core::events().dispatch(event);
}

void DialoguePlayer::dispatch_event(const DialogueTextRevealedEvent& event) {
    core::events().dispatch(event);
}

} // namespace engine::dialogue
