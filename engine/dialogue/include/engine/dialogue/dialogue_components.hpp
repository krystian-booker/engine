#pragma once

#include <engine/scene/entity.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace engine::dialogue {

// ============================================================================
// Dialogue Trigger Component
// ============================================================================

struct DialogueTriggerComponent {
    std::string dialogue_id;        // ID of the dialogue graph

    // Trigger settings
    float interaction_range = 3.0f;
    bool require_interaction = true;    // vs auto-start when in range
    bool face_player = true;

    // Priority for multiple dialogue options
    int32_t priority = 0;

    // Conditions to show this dialogue
    std::vector<std::string> required_flags;
    std::vector<std::string> excluded_flags;    // Flags that prevent this dialogue
    std::string required_quest;
    std::string required_quest_state;           // "active", "completed", etc.

    // One-shot dialogues
    bool once_per_session = false;
    bool once_ever = false;
    bool triggered = false;

    // State
    bool enabled = true;
    bool in_range = false;
};

// ============================================================================
// Dialogue State Component (tracks dialogue history per entity)
// ============================================================================

struct DialogueStateComponent {
    // Track which dialogue nodes have been seen
    std::vector<std::string> seen_nodes;

    // Track dialogue choices made
    std::unordered_map<std::string, std::string> choice_history;  // node_id -> choice_id

    // Track number of times each dialogue has been started
    std::unordered_map<std::string, int32_t> dialogue_counts;

    // Custom state variables
    std::unordered_map<std::string, std::string> state_vars;

    // Relationship/affinity tracking (optional)
    int32_t affinity = 0;
    std::string relationship_level;     // "stranger", "acquaintance", "friend", etc.

    // Timestamps
    float last_dialogue_time = 0.0f;
    float total_dialogue_time = 0.0f;

    // Helper methods
    bool has_seen_node(const std::string& node_id) const {
        for (const auto& n : seen_nodes) {
            if (n == node_id) return true;
        }
        return false;
    }

    void mark_node_seen(const std::string& node_id) {
        if (!has_seen_node(node_id)) {
            seen_nodes.push_back(node_id);
        }
    }

    std::string get_choice(const std::string& node_id) const {
        auto it = choice_history.find(node_id);
        return it != choice_history.end() ? it->second : "";
    }

    void set_choice(const std::string& node_id, const std::string& choice_id) {
        choice_history[node_id] = choice_id;
    }

    int32_t get_dialogue_count(const std::string& dialogue_id) const {
        auto it = dialogue_counts.find(dialogue_id);
        return it != dialogue_counts.end() ? it->second : 0;
    }

    void increment_dialogue_count(const std::string& dialogue_id) {
        dialogue_counts[dialogue_id]++;
    }
};

// ============================================================================
// Dialogue Speaker Component (marks entity as a dialogue participant)
// ============================================================================

struct DialogueSpeakerComponent {
    std::string speaker_id;         // Links to DialogueSpeaker in graph
    std::string display_name_key;   // Localization key for name

    // Visual
    std::string portrait;
    std::string voice_bank;

    // Behavior during dialogue
    bool face_player_during_dialogue = true;
    bool stop_movement_during_dialogue = true;
    std::string idle_animation;
    std::string talk_animation;

    // Audio
    float voice_pitch = 1.0f;
    float voice_volume = 1.0f;
};

// ============================================================================
// Dialogue Camera Component (defines camera shots for dialogue)
// ============================================================================

struct DialogueCameraComponent {
    std::string shot_id;

    // Camera position relative to speakers
    enum class ShotType {
        CloseUp,        // Close-up on speaker
        MediumShot,     // Waist-up
        WideShot,       // Full body + environment
        OverShoulder,   // Over shoulder of listener
        TwoShot,        // Both speakers
        Custom          // Custom position
    };
    ShotType shot_type = ShotType::MediumShot;

    // For custom shots
    Vec3 position_offset{0.0f, 1.5f, 2.0f};
    Vec3 look_at_offset{0.0f, 1.5f, 0.0f};

    // Animation
    float transition_time = 0.5f;
    bool smooth_transition = true;

    // Depth of field
    bool enable_dof = true;
    float focus_distance = 2.0f;
    float aperture = 2.8f;
};

// ============================================================================
// Barks Component (ambient dialogue/reactions)
// ============================================================================

struct BarksComponent {
    struct Bark {
        std::string id;
        std::string text_key;           // Localization key
        std::string voice_clip;
        float cooldown = 30.0f;         // Minimum time between plays
        float last_played = -1000.0f;   // Time last played

        // Conditions
        std::vector<std::string> required_flags;
        float trigger_chance = 1.0f;    // 0.0 to 1.0
    };

    // Bark categories
    std::vector<Bark> idle_barks;       // Random idle chatter
    std::vector<Bark> combat_barks;     // During combat
    std::vector<Bark> alert_barks;      // When alerted
    std::vector<Bark> damage_barks;     // When taking damage
    std::vector<Bark> death_barks;      // On death
    std::vector<Bark> greeting_barks;   // When player approaches
    std::vector<Bark> reaction_barks;   // Reactions to events

    // Settings
    bool enabled = true;
    float bark_range = 15.0f;           // Max distance for player to hear
    float min_bark_interval = 10.0f;    // Minimum time between any bark
    float last_bark_time = 0.0f;
};

// ============================================================================
// Subtitle Component (for displaying subtitles)
// ============================================================================

struct SubtitleComponent {
    bool show_subtitles = true;
    bool show_speaker_name = true;

    // Style
    std::string font_style;
    float font_size = 24.0f;
    Vec4 text_color{1.0f, 1.0f, 1.0f, 1.0f};
    Vec4 background_color{0.0f, 0.0f, 0.0f, 0.7f};

    // Position
    Vec2 screen_position{0.5f, 0.9f};   // Normalized (0-1)
    float max_width = 0.8f;             // Normalized

    // Timing
    float min_display_time = 2.0f;
    float chars_per_second = 15.0f;     // For calculating display time
};

// ============================================================================
// Component Registration
// ============================================================================

void register_dialogue_components();

} // namespace engine::dialogue
