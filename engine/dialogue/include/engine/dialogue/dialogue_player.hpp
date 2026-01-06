#pragma once

#include <engine/dialogue/dialogue_graph.hpp>
#include <engine/scene/world.hpp>
#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <unordered_map>
#include <any>

namespace engine::dialogue {

// ============================================================================
// Dialogue Events
// ============================================================================

struct DialogueStartedEvent {
    std::string graph_id;
    scene::Entity initiator;
    scene::Entity target;
};

struct DialogueEndedEvent {
    std::string graph_id;
    std::string exit_reason;    // "completed", "cancelled", "interrupted"
};

struct DialogueNodeEnteredEvent {
    std::string graph_id;
    std::string node_id;
    std::string speaker_id;
    std::string text_key;
};

struct DialogueChoiceMadeEvent {
    std::string graph_id;
    std::string node_id;
    std::string choice_id;
};

struct DialogueTextRevealedEvent {
    std::string graph_id;
    std::string node_id;
    float progress;             // 0.0 to 1.0
    bool complete;
};

// ============================================================================
// Dialogue Player State
// ============================================================================

enum class DialoguePlayerState {
    Inactive,
    Playing,
    WaitingForInput,
    Advancing,
    Paused
};

// ============================================================================
// Dialogue Player
// ============================================================================

class DialoguePlayer {
public:
    static DialoguePlayer& instance();

    // ========================================================================
    // Dialogue Control
    // ========================================================================

    bool start(const std::string& graph_id, scene::Entity initiator = scene::NullEntity,
               scene::Entity target = scene::NullEntity);
    bool start(DialogueGraph* graph, scene::Entity initiator = scene::NullEntity,
               scene::Entity target = scene::NullEntity);

    void stop(const std::string& reason = "cancelled");
    void pause();
    void resume();

    bool is_active() const { return m_state != DialoguePlayerState::Inactive; }
    bool is_paused() const { return m_state == DialoguePlayerState::Paused; }
    bool is_waiting_for_input() const { return m_state == DialoguePlayerState::WaitingForInput; }

    DialoguePlayerState get_state() const { return m_state; }

    // ========================================================================
    // Navigation
    // ========================================================================

    void advance();                                 // Advance to next node/skip text
    void select_choice(int32_t index);              // Select choice by index
    void select_choice(const std::string& id);      // Select choice by ID

    bool can_advance() const;
    bool has_choices() const;

    // ========================================================================
    // Current State Getters
    // ========================================================================

    const DialogueGraph* get_current_graph() const { return m_current_graph; }
    const DialogueNode* get_current_node() const { return m_current_node; }
    const DialogueSpeaker* get_current_speaker() const;

    std::string get_current_text() const;           // Localized text
    std::string get_revealed_text() const;          // Text revealed so far (typewriter)
    float get_text_progress() const { return m_text_progress; }
    bool is_text_complete() const { return m_text_progress >= 1.0f; }

    std::vector<const DialogueChoice*> get_available_choices() const;
    int32_t get_choice_count() const;

    scene::Entity get_initiator() const { return m_initiator; }
    scene::Entity get_target() const { return m_target; }

    // ========================================================================
    // Text Display Settings
    // ========================================================================

    void set_typewriter_enabled(bool enabled) { m_typewriter_enabled = enabled; }
    bool is_typewriter_enabled() const { return m_typewriter_enabled; }

    void set_typewriter_speed(float chars_per_second) { m_typewriter_speed = chars_per_second; }
    float get_typewriter_speed() const { return m_typewriter_speed; }

    void skip_typewriter();     // Instantly reveal all text

    // ========================================================================
    // Dialogue Variables (Blackboard)
    // ========================================================================

    void set_variable(const std::string& key, const std::any& value);
    std::any get_variable(const std::string& key) const;
    bool has_variable(const std::string& key) const;

    template<typename T>
    T get_variable(const std::string& key, const T& default_value) const {
        auto it = m_variables.find(key);
        if (it == m_variables.end()) return default_value;
        try {
            return std::any_cast<T>(it->second);
        } catch (...) {
            return default_value;
        }
    }

    void clear_variables();

    // ========================================================================
    // History
    // ========================================================================

    const std::vector<std::string>& get_visited_nodes() const { return m_visited_nodes; }
    bool has_visited_node(const std::string& node_id) const;
    void clear_history();

    // Per-NPC dialogue history (persisted)
    void set_npc_dialogue_state(scene::Entity npc, const std::string& key, const std::any& value);
    std::any get_npc_dialogue_state(scene::Entity npc, const std::string& key) const;

    // ========================================================================
    // Update
    // ========================================================================

    void update(float dt);

    // ========================================================================
    // Callbacks
    // ========================================================================

    using ActionHandler = std::function<void(const DialogueAction&)>;
    using ConditionChecker = std::function<bool(const DialogueCondition&)>;
    using SkillCheckHandler = std::function<bool(const std::string& skill, int32_t value)>;

    void set_action_handler(DialogueAction::Type type, ActionHandler handler);
    void set_condition_checker(DialogueCondition::Type type, ConditionChecker checker);
    void set_skill_check_handler(SkillCheckHandler handler);

    // Text getter for localization
    void set_text_getter(std::function<std::string(const std::string&)> getter);

private:
    DialoguePlayer();

    void enter_node(const std::string& node_id);
    void exit_current_node();
    void execute_actions(const std::vector<DialogueAction>& actions);
    bool check_conditions(const std::vector<DialogueCondition>& conditions) const;
    bool check_condition(const DialogueCondition& condition) const;
    void execute_action(const DialogueAction& action);

    void dispatch_event(const DialogueStartedEvent& event);
    void dispatch_event(const DialogueEndedEvent& event);
    void dispatch_event(const DialogueNodeEnteredEvent& event);
    void dispatch_event(const DialogueChoiceMadeEvent& event);
    void dispatch_event(const DialogueTextRevealedEvent& event);

    DialoguePlayerState m_state = DialoguePlayerState::Inactive;

    DialogueGraph* m_current_graph = nullptr;
    DialogueNode* m_current_node = nullptr;

    scene::Entity m_initiator = scene::NullEntity;
    scene::Entity m_target = scene::NullEntity;

    // Text display
    bool m_typewriter_enabled = true;
    float m_typewriter_speed = 30.0f;   // Characters per second
    float m_text_progress = 0.0f;
    size_t m_revealed_chars = 0;
    std::string m_current_localized_text;

    // Timing
    float m_node_time = 0.0f;
    float m_auto_advance_timer = 0.0f;

    // Variables
    std::unordered_map<std::string, std::any> m_variables;

    // History
    std::vector<std::string> m_visited_nodes;

    // Per-NPC state
    std::unordered_map<uint32_t, std::unordered_map<std::string, std::any>> m_npc_states;

    // Handlers
    std::unordered_map<DialogueAction::Type, ActionHandler> m_action_handlers;
    std::unordered_map<DialogueCondition::Type, ConditionChecker> m_condition_checkers;
    SkillCheckHandler m_skill_check_handler;
    std::function<std::string(const std::string&)> m_text_getter;
};

// Convenience accessor
inline DialoguePlayer& dialogue_player() { return DialoguePlayer::instance(); }

} // namespace engine::dialogue
