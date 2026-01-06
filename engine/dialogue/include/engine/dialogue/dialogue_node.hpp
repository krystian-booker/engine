#pragma once

#include <engine/core/types.hpp>
#include <engine/scene/entity.hpp>
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <variant>

namespace engine::dialogue {

// ============================================================================
// Dialogue Speaker
// ============================================================================

struct DialogueSpeaker {
    std::string id;
    std::string display_name_key;   // Localization key
    std::string portrait;           // Portrait image/sprite ID
    std::string voice_id;           // Voice bank ID for TTS or voice matching

    // Optional entity reference (for dynamic speakers)
    scene::Entity entity = scene::NullEntity;

    // Display customization
    Vec4 name_color{1.0f, 1.0f, 1.0f, 1.0f};
    std::string text_style;         // Font/style override
};

// ============================================================================
// Dialogue Condition
// ============================================================================

struct DialogueCondition {
    enum class Type {
        Flag,               // Game flag check
        Counter,            // Counter comparison
        QuestState,         // Quest in specific state
        QuestComplete,      // Quest completed
        HasItem,            // Player has item
        Reputation,         // Faction reputation
        Custom              // Custom function
    };

    Type type = Type::Flag;
    std::string key;            // Flag name, quest ID, item ID, etc.
    std::string compare_op;     // "==", "!=", "<", ">", "<=", ">="
    int32_t value = 0;          // For counter/reputation comparisons
    bool negate = false;        // Invert result

    std::function<bool()> custom_check;

    bool evaluate() const;      // Implemented in cpp
};

// ============================================================================
// Dialogue Action
// ============================================================================

struct DialogueAction {
    enum class Type {
        SetFlag,            // Set game flag
        ClearFlag,          // Clear game flag
        IncrementCounter,   // Increment counter
        SetCounter,         // Set counter value
        StartQuest,         // Start quest
        CompleteObjective,  // Complete quest objective
        GiveItem,           // Give item to player
        TakeItem,           // Take item from player
        ChangeReputation,   // Change faction reputation
        PlaySound,          // Play sound effect
        PlayAnimation,      // Play animation on speaker
        TriggerEvent,       // Trigger custom event
        StartCinematic,     // Start cinematic sequence
        Custom              // Custom callback
    };

    Type type = Type::SetFlag;
    std::string key;            // Flag, quest ID, item ID, etc.
    std::string value;          // Item ID, animation name, etc.
    int32_t amount = 1;         // For counters, reputation, items

    std::function<void()> custom_action;
};

// ============================================================================
// Dialogue Choice
// ============================================================================

struct DialogueChoice {
    std::string id;
    std::string text_key;           // Localization key for choice text
    std::string target_node_id;     // Node to go to when selected

    // Conditions for this choice to be available
    std::vector<DialogueCondition> conditions;

    // Actions to execute when this choice is selected
    std::vector<DialogueAction> actions;

    // Display
    bool is_highlighted = false;    // Special styling (important choice)
    bool is_exit = false;           // Ends dialogue
    bool show_unavailable = false;  // Show grayed out if conditions fail
    std::string unavailable_reason_key;

    // For skill checks
    std::string skill_check_type;   // "persuasion", "intimidate", etc.
    int32_t skill_check_value = 0;  // Required value
    bool skill_check_passed = false;
};

// ============================================================================
// Dialogue Node
// ============================================================================

struct DialogueNode {
    std::string id;
    std::string speaker_id;         // Speaker for this node
    std::string text_key;           // Localization key for dialogue text

    std::vector<DialogueChoice> choices;

    // Actions executed when entering this node
    std::vector<DialogueAction> on_enter_actions;

    // Actions executed when leaving this node
    std::vector<DialogueAction> on_exit_actions;

    // Audio
    std::string voice_clip;         // Voice line audio file
    float voice_delay = 0.0f;       // Delay before playing voice

    // Timing
    float auto_advance_delay = 0.0f;    // Auto-advance after delay (0 = manual)
    float min_display_time = 0.0f;      // Minimum time before advancing

    // Animation/Expression
    std::string speaker_animation;
    std::string speaker_expression;

    // Camera
    std::string camera_shot;        // Cinematic camera shot ID
    bool camera_focus_speaker = false;

    // Next node (for linear dialogue without choices)
    std::string next_node_id;

    // Flags
    bool is_entry_point = false;
    bool is_exit_point = false;
    bool once_only = false;         // Only show once per conversation
    bool shown = false;             // Has been shown (for once_only)

    // Helper methods
    bool has_choices() const { return !choices.empty(); }
    bool has_next() const { return !next_node_id.empty() || !choices.empty(); }
    bool is_terminal() const { return is_exit_point || (!has_choices() && next_node_id.empty()); }
};

// ============================================================================
// Dialogue Node Builder
// ============================================================================

class DialogueNodeBuilder {
public:
    DialogueNodeBuilder(const std::string& id) {
        m_node.id = id;
    }

    DialogueNodeBuilder& speaker(const std::string& speaker_id) {
        m_node.speaker_id = speaker_id;
        return *this;
    }

    DialogueNodeBuilder& text(const std::string& text_key) {
        m_node.text_key = text_key;
        return *this;
    }

    DialogueNodeBuilder& voice(const std::string& clip) {
        m_node.voice_clip = clip;
        return *this;
    }

    DialogueNodeBuilder& next(const std::string& node_id) {
        m_node.next_node_id = node_id;
        return *this;
    }

    DialogueNodeBuilder& choice(DialogueChoice c) {
        m_node.choices.push_back(std::move(c));
        return *this;
    }

    DialogueNodeBuilder& choice(const std::string& id, const std::string& text_key,
                                 const std::string& target) {
        DialogueChoice c;
        c.id = id;
        c.text_key = text_key;
        c.target_node_id = target;
        m_node.choices.push_back(c);
        return *this;
    }

    DialogueNodeBuilder& exit_choice(const std::string& id, const std::string& text_key) {
        DialogueChoice c;
        c.id = id;
        c.text_key = text_key;
        c.is_exit = true;
        m_node.choices.push_back(c);
        return *this;
    }

    DialogueNodeBuilder& on_enter(DialogueAction action) {
        m_node.on_enter_actions.push_back(std::move(action));
        return *this;
    }

    DialogueNodeBuilder& on_exit(DialogueAction action) {
        m_node.on_exit_actions.push_back(std::move(action));
        return *this;
    }

    DialogueNodeBuilder& animation(const std::string& anim) {
        m_node.speaker_animation = anim;
        return *this;
    }

    DialogueNodeBuilder& expression(const std::string& expr) {
        m_node.speaker_expression = expr;
        return *this;
    }

    DialogueNodeBuilder& camera(const std::string& shot) {
        m_node.camera_shot = shot;
        return *this;
    }

    DialogueNodeBuilder& auto_advance(float delay) {
        m_node.auto_advance_delay = delay;
        return *this;
    }

    DialogueNodeBuilder& entry_point(bool value = true) {
        m_node.is_entry_point = value;
        return *this;
    }

    DialogueNodeBuilder& exit_point(bool value = true) {
        m_node.is_exit_point = value;
        return *this;
    }

    DialogueNodeBuilder& once_only(bool value = true) {
        m_node.once_only = value;
        return *this;
    }

    DialogueNode build() { return std::move(m_node); }

private:
    DialogueNode m_node;
};

inline DialogueNodeBuilder make_node(const std::string& id) {
    return DialogueNodeBuilder(id);
}

// ============================================================================
// Dialogue Choice Builder
// ============================================================================

class DialogueChoiceBuilder {
public:
    DialogueChoiceBuilder(const std::string& id) {
        m_choice.id = id;
    }

    DialogueChoiceBuilder& text(const std::string& text_key) {
        m_choice.text_key = text_key;
        return *this;
    }

    DialogueChoiceBuilder& target(const std::string& node_id) {
        m_choice.target_node_id = node_id;
        return *this;
    }

    DialogueChoiceBuilder& exit() {
        m_choice.is_exit = true;
        return *this;
    }

    DialogueChoiceBuilder& condition(DialogueCondition cond) {
        m_choice.conditions.push_back(std::move(cond));
        return *this;
    }

    DialogueChoiceBuilder& requires_flag(const std::string& flag) {
        DialogueCondition c;
        c.type = DialogueCondition::Type::Flag;
        c.key = flag;
        m_choice.conditions.push_back(c);
        return *this;
    }

    DialogueChoiceBuilder& requires_quest_complete(const std::string& quest_id) {
        DialogueCondition c;
        c.type = DialogueCondition::Type::QuestComplete;
        c.key = quest_id;
        m_choice.conditions.push_back(c);
        return *this;
    }

    DialogueChoiceBuilder& action(DialogueAction act) {
        m_choice.actions.push_back(std::move(act));
        return *this;
    }

    DialogueChoiceBuilder& sets_flag(const std::string& flag) {
        DialogueAction a;
        a.type = DialogueAction::Type::SetFlag;
        a.key = flag;
        m_choice.actions.push_back(a);
        return *this;
    }

    DialogueChoiceBuilder& starts_quest(const std::string& quest_id) {
        DialogueAction a;
        a.type = DialogueAction::Type::StartQuest;
        a.key = quest_id;
        m_choice.actions.push_back(a);
        return *this;
    }

    DialogueChoiceBuilder& highlighted(bool value = true) {
        m_choice.is_highlighted = value;
        return *this;
    }

    DialogueChoiceBuilder& skill_check(const std::string& skill, int32_t value) {
        m_choice.skill_check_type = skill;
        m_choice.skill_check_value = value;
        return *this;
    }

    DialogueChoice build() { return std::move(m_choice); }

private:
    DialogueChoice m_choice;
};

inline DialogueChoiceBuilder make_choice(const std::string& id) {
    return DialogueChoiceBuilder(id);
}

} // namespace engine::dialogue
