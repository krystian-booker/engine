#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <functional>

namespace engine::achievements {

// ============================================================================
// Achievement Type
// ============================================================================

enum class AchievementType : uint8_t {
    Binary,         // Unlocked or not (e.g., "Complete Tutorial")
    Counter,        // Reach target count (e.g., "Kill 100 enemies")
    Progress,       // Percentage-based progress (e.g., "Complete 50% of map")
    Tiered          // Multiple tiers (e.g., "Kill 10/50/100 enemies")
};

// ============================================================================
// Achievement Category
// ============================================================================

enum class AchievementCategory : uint8_t {
    Story,          // Story/campaign progression
    Combat,         // Combat-related
    Exploration,    // Exploration and discovery
    Collection,     // Collecting items
    Challenge,      // Skill-based challenges
    Social,         // Multiplayer/social (if applicable)
    Secret,         // Hidden achievements
    Misc            // Miscellaneous
};

// ============================================================================
// Achievement Tier
// ============================================================================

struct AchievementTier {
    std::string tier_id;
    std::string display_name;
    int target_count;
    int points;
    std::vector<std::string> rewards;  // Reward IDs
};

// ============================================================================
// Achievement Definition
// ============================================================================

struct AchievementDefinition {
    std::string achievement_id;
    std::string display_name;
    std::string description;
    std::string hidden_description;     // Shown before unlock if hidden
    std::string icon_path;
    std::string icon_locked_path;

    AchievementType type = AchievementType::Binary;
    AchievementCategory category = AchievementCategory::Misc;

    int target_count = 1;               // For Counter/Progress type
    std::vector<AchievementTier> tiers; // For Tiered type

    bool is_hidden = false;             // Secret achievement
    bool is_hidden_until_progress = false; // Show after some progress
    float hidden_progress_threshold = 0.5f;

    // Prerequisites
    std::vector<std::string> prerequisites;  // Must be unlocked first

    // Rewards
    int points = 0;                     // Achievement points
    std::vector<std::string> unlock_rewards; // Item IDs, cosmetics, etc.

    // Platform-specific ID (for Steam, Xbox, PlayStation integration)
    std::string platform_id;

    // Sorting
    int display_order = 0;

    // ========================================================================
    // Helpers
    // ========================================================================

    bool is_tiered() const { return type == AchievementType::Tiered && !tiers.empty(); }
    int get_tier_count() const { return static_cast<int>(tiers.size()); }
    const AchievementTier* get_tier(int index) const;
    int get_total_points() const;
};

// ============================================================================
// Achievement Registry
// ============================================================================

class AchievementRegistry {
public:
    static AchievementRegistry& instance();

    // Delete copy/move
    AchievementRegistry(const AchievementRegistry&) = delete;
    AchievementRegistry& operator=(const AchievementRegistry&) = delete;

    // Registration
    void register_achievement(const AchievementDefinition& def);
    void load_achievements(const std::string& path);

    // Lookup
    const AchievementDefinition* get(const std::string& achievement_id) const;
    bool exists(const std::string& achievement_id) const;

    // Queries
    std::vector<std::string> get_all_achievement_ids() const;
    std::vector<std::string> get_by_category(AchievementCategory category) const;
    std::vector<std::string> get_visible_achievements() const;
    std::vector<std::string> get_hidden_achievements() const;

    // Statistics
    int get_total_achievements() const;
    int get_total_points() const;

    // Clear (for hot reload)
    void clear();

private:
    AchievementRegistry() = default;
    ~AchievementRegistry() = default;

    std::unordered_map<std::string, AchievementDefinition> m_achievements;
};

// ============================================================================
// Global Access
// ============================================================================

inline AchievementRegistry& achievement_registry() { return AchievementRegistry::instance(); }

// ============================================================================
// Achievement Builder
// ============================================================================

class AchievementBuilder {
public:
    AchievementBuilder& id(const std::string& achievement_id);
    AchievementBuilder& name(const std::string& display_name);
    AchievementBuilder& description(const std::string& desc);
    AchievementBuilder& hidden_description(const std::string& desc);
    AchievementBuilder& icon(const std::string& path);
    AchievementBuilder& locked_icon(const std::string& path);
    AchievementBuilder& type(AchievementType t);
    AchievementBuilder& category(AchievementCategory cat);
    AchievementBuilder& target(int count);
    AchievementBuilder& tier(const std::string& tier_id, const std::string& name, int target, int pts);
    AchievementBuilder& hidden(bool hide_until_progress = false, float threshold = 0.5f);
    AchievementBuilder& prerequisite(const std::string& achievement_id);
    AchievementBuilder& points(int pts);
    AchievementBuilder& reward(const std::string& reward_id);
    AchievementBuilder& platform_id(const std::string& pid);
    AchievementBuilder& order(int display_order);

    AchievementDefinition build() const;
    void register_achievement() const;

private:
    AchievementDefinition m_def;
};

// ============================================================================
// Convenience
// ============================================================================

inline AchievementBuilder achievement() { return AchievementBuilder{}; }

} // namespace engine::achievements
