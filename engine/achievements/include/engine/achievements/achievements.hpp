#pragma once

// ============================================================================
// Engine Achievement System - Umbrella Header
// ============================================================================
//
// Achievement tracking with progress, unlock conditions, and platform hooks.
//
// Quick Start:
// ------------
// 1. Define achievements:
//    achievement()
//        .id("first_blood")
//        .name("First Blood")
//        .description("Defeat your first enemy")
//        .type(AchievementType::Binary)
//        .category(AchievementCategory::Combat)
//        .points(10)
//        .register_achievement();
//
// 2. Track progress:
//    achievements().increment("kill_count", 1);
//    achievements().unlock("first_blood");
//
// 3. Query status:
//    if (achievements().is_unlocked("first_blood")) { ... }
//    float percent = achievements().get_progress_percent("collector");
//
// Achievement Types:
// ------------------
// - Binary: Simple unlock (complete/incomplete)
// - Counter: Track count toward target
// - Progress: Percentage-based completion
// - Tiered: Multiple unlock tiers (bronze/silver/gold)
//
// Platform Integration:
// ---------------------
// achievements().set_platform_callback([](const std::string& id, const std::string& platform_id) {
//     // Call Steam/Xbox/PlayStation API
// });
//
// ============================================================================

#include <engine/achievements/achievement_definition.hpp>
#include <engine/achievements/achievement_manager.hpp>
#include <engine/achievements/achievement_events.hpp>
