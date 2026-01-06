#pragma once

#include <engine/achievements/achievement_definition.hpp>
#include <string>
#include <cstdint>

namespace engine::achievements {

// ============================================================================
// Achievement Unlocked Event
// ============================================================================

struct AchievementUnlockedEvent {
    std::string achievement_id;
    std::string display_name;
    std::string description;
    std::string icon_path;
    int points;
    uint64_t timestamp;
};

// ============================================================================
// Achievement Progress Event
// ============================================================================

struct AchievementProgressEvent {
    std::string achievement_id;
    int current_count;
    int target_count;
    float progress_percent;
    bool newly_started;             // First time progress was made
};

// ============================================================================
// Achievement Tier Unlocked Event
// ============================================================================

struct AchievementTierUnlockedEvent {
    std::string achievement_id;
    int tier_index;
    std::string tier_name;
    int tier_points;
    int total_tiers;
    bool is_final_tier;
};

// ============================================================================
// Achievement Reset Event
// ============================================================================

struct AchievementResetEvent {
    std::string achievement_id;     // Empty if all were reset
    bool all_reset;
};

// ============================================================================
// Achievement Sync Event (Platform)
// ============================================================================

struct AchievementSyncEvent {
    int synced_count;
    int new_unlocks;                // Unlocks from platform not in local
    bool success;
    std::string error_message;
};

// ============================================================================
// Event Registration
// ============================================================================

void register_achievement_events();

} // namespace engine::achievements
