#pragma once

#include <engine/achievements/achievement_definition.hpp>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace engine::achievements {

// ============================================================================
// Achievement Progress
// ============================================================================

struct AchievementProgress {
    std::string achievement_id;
    int current_count = 0;
    int current_tier = 0;           // For tiered achievements
    bool unlocked = false;
    uint64_t unlock_timestamp = 0;
    uint64_t first_progress_timestamp = 0;
    std::vector<bool> tiers_unlocked;  // For tiered achievements
};

// ============================================================================
// Achievement Notification
// ============================================================================

struct AchievementNotification {
    std::string achievement_id;
    std::string display_name;
    std::string description;
    std::string icon_path;
    int points;
    bool is_tier_unlock;            // For tiered achievements
    int tier_index;
    uint64_t timestamp;
};

// ============================================================================
// Achievement Manager
// ============================================================================

class AchievementManager {
public:
    static AchievementManager& instance();

    // Delete copy/move
    AchievementManager(const AchievementManager&) = delete;
    AchievementManager& operator=(const AchievementManager&) = delete;

    // ========================================================================
    // Progress Tracking
    // ========================================================================

    // Increment counter (for Counter/Tiered types)
    void increment(const std::string& achievement_id, int amount = 1);

    // Set progress directly
    void set_progress(const std::string& achievement_id, int value);

    // Unlock directly (for Binary type)
    void unlock(const std::string& achievement_id);

    // Check condition and unlock if met
    void check_unlock(const std::string& achievement_id);

    // ========================================================================
    // Queries
    // ========================================================================

    bool is_unlocked(const std::string& achievement_id) const;
    int get_progress(const std::string& achievement_id) const;
    float get_progress_percent(const std::string& achievement_id) const;
    int get_current_tier(const std::string& achievement_id) const;
    bool is_tier_unlocked(const std::string& achievement_id, int tier) const;

    const AchievementProgress* get_achievement_progress(const std::string& achievement_id) const;

    // ========================================================================
    // Bulk Queries
    // ========================================================================

    std::vector<std::string> get_all_unlocked() const;
    std::vector<std::string> get_all_locked() const;
    std::vector<std::string> get_in_progress() const;  // Started but not complete
    std::vector<std::string> get_by_category(AchievementCategory category) const;

    // ========================================================================
    // Statistics
    // ========================================================================

    int get_unlocked_count() const;
    int get_total_count() const;
    int get_earned_points() const;
    int get_total_points() const;
    float get_completion_percent() const;

    // Category-specific stats
    int get_unlocked_in_category(AchievementCategory category) const;
    int get_total_in_category(AchievementCategory category) const;

    // ========================================================================
    // Persistence
    // ========================================================================

    bool load_progress(const std::string& path = "achievements.json");
    bool save_progress(const std::string& path = "achievements.json") const;
    void reset_progress();
    void reset_achievement(const std::string& achievement_id);

    // ========================================================================
    // Platform Integration
    // ========================================================================

    // Callback when achievement is unlocked (for Steam, Xbox, etc.)
    using PlatformUnlockCallback = std::function<void(const std::string& achievement_id, const std::string& platform_id)>;
    void set_platform_callback(PlatformUnlockCallback callback);

    // Sync with platform (pull from Steam/Xbox)
    void sync_with_platform();

    // ========================================================================
    // Notifications
    // ========================================================================

    // Get pending notifications (for UI display)
    std::vector<AchievementNotification> get_pending_notifications();
    void clear_notifications();
    bool has_pending_notifications() const;

    // ========================================================================
    // Callbacks
    // ========================================================================

    using UnlockCallback = std::function<void(const AchievementDefinition&)>;
    using ProgressCallback = std::function<void(const std::string&, int current, int target)>;
    using TierCallback = std::function<void(const AchievementDefinition&, int tier)>;

    void set_on_unlock(UnlockCallback callback);
    void set_on_progress(ProgressCallback callback);
    void set_on_tier_unlock(TierCallback callback);

    // ========================================================================
    // Debug
    // ========================================================================

    void unlock_all();  // Debug: unlock everything
    void lock_all();    // Debug: reset all progress

private:
    AchievementManager() = default;
    ~AchievementManager() = default;

    void internal_unlock(const std::string& achievement_id);
    void internal_unlock_tier(const std::string& achievement_id, int tier);
    bool check_prerequisites(const std::string& achievement_id) const;
    void create_notification(const std::string& achievement_id, bool is_tier = false, int tier = 0);

    std::unordered_map<std::string, AchievementProgress> m_progress;
    std::vector<AchievementNotification> m_pending_notifications;

    PlatformUnlockCallback m_platform_callback;
    UnlockCallback m_on_unlock;
    ProgressCallback m_on_progress;
    TierCallback m_on_tier_unlock;
};

// ============================================================================
// Global Access
// ============================================================================

inline AchievementManager& achievements() { return AchievementManager::instance(); }

} // namespace engine::achievements
