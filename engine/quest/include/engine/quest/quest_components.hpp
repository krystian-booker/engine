#pragma once

#include <engine/quest/objective.hpp>
#include <engine/quest/waypoint.hpp>
#include <string>
#include <vector>

namespace engine::quest {

// Re-export components from other headers
using engine::quest::WaypointComponent;
using engine::quest::QuestTriggerComponent;
using engine::quest::QuestGiverComponent;

// ============================================================================
// Quest Log Component (for player entity)
// ============================================================================

struct QuestLogComponent {
    std::vector<std::string> active_quests;
    std::vector<std::string> completed_quests;
    std::vector<std::string> failed_quests;

    // UI state
    std::string selected_quest;
    bool log_open = false;

    // Statistics
    int32_t total_quests_completed = 0;
    int32_t total_objectives_completed = 0;
};

// ============================================================================
// Quest Participant Component
// ============================================================================

struct QuestParticipantComponent {
    // Marks an entity as part of a quest (escortee, target, etc.)
    std::string quest_id;
    std::string role;               // "escort_target", "kill_target", etc.

    // For escort quests
    bool must_survive = false;
    float current_health = 100.0f;
    float max_health = 100.0f;

    // For interaction tracking
    bool has_been_interacted = false;
    std::string required_interaction;
};

// ============================================================================
// Kill Tracker Component
// ============================================================================

struct KillTrackerComponent {
    std::string enemy_type;         // Type for kill objectives
    std::string faction;            // Faction for kill objectives

    // Auto-report kill on death
    bool report_on_death = true;
};

// ============================================================================
// Collection Item Component
// ============================================================================

struct CollectionItemComponent {
    std::string counter_key;        // Counter to increment when collected
    int32_t amount = 1;             // Amount to add

    // Collection behavior
    bool destroy_on_collect = true;
    bool require_interaction = false;   // Or auto-collect on touch

    // Feedback
    std::string collect_sound;
    std::string collect_effect;
};

// ============================================================================
// Quest Zone Component
// ============================================================================

struct QuestZoneComponent {
    std::string zone_id;
    std::string zone_name;

    // Quests available in this zone
    std::vector<std::string> zone_quests;

    // Discovery
    bool discovered = false;
    bool show_on_map = true;
};

// ============================================================================
// Component Registration
// ============================================================================

void register_quest_components();

} // namespace engine::quest
