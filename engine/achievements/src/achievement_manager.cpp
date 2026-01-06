#include <engine/achievements/achievement_manager.hpp>
#include <engine/achievements/achievement_events.hpp>
#include <engine/core/log.hpp>
#include <engine/core/game_events.hpp>
#include <fstream>
#include <nlohmann/json.hpp>
#include <ctime>

namespace engine::achievements {

using json = nlohmann::json;

// ============================================================================
// AchievementManager Singleton
// ============================================================================

AchievementManager& AchievementManager::instance() {
    static AchievementManager s_instance;
    return s_instance;
}

// ============================================================================
// Progress Tracking
// ============================================================================

void AchievementManager::increment(const std::string& achievement_id, int amount) {
    const auto* def = achievement_registry().get(achievement_id);
    if (!def) {
        core::log_warning("achievements", "Unknown achievement: {}", achievement_id);
        return;
    }

    if (is_unlocked(achievement_id)) {
        return;  // Already fully unlocked
    }

    auto& progress = m_progress[achievement_id];
    progress.achievement_id = achievement_id;

    bool first_progress = (progress.current_count == 0);
    if (first_progress) {
        progress.first_progress_timestamp = static_cast<uint64_t>(std::time(nullptr));
    }

    progress.current_count += amount;

    // Fire progress event
    if (m_on_progress) {
        m_on_progress(achievement_id, progress.current_count, def->target_count);
    }

    AchievementProgressEvent event;
    event.achievement_id = achievement_id;
    event.current_count = progress.current_count;
    event.target_count = def->target_count;
    event.progress_percent = get_progress_percent(achievement_id);
    event.newly_started = first_progress;
    core::game_events().publish(event);

    // Check for unlock/tier unlock
    if (def->is_tiered()) {
        // Check each tier
        for (int i = 0; i < def->get_tier_count(); ++i) {
            const auto* tier = def->get_tier(i);
            if (tier && progress.current_count >= tier->target_count) {
                if (progress.tiers_unlocked.size() <= static_cast<size_t>(i)) {
                    progress.tiers_unlocked.resize(static_cast<size_t>(i) + 1, false);
                }
                if (!progress.tiers_unlocked[static_cast<size_t>(i)]) {
                    internal_unlock_tier(achievement_id, i);
                }
            }
        }

        // Check if all tiers complete
        if (progress.tiers_unlocked.size() == def->tiers.size() &&
            std::all_of(progress.tiers_unlocked.begin(), progress.tiers_unlocked.end(), [](bool v) { return v; })) {
            progress.unlocked = true;
            progress.unlock_timestamp = static_cast<uint64_t>(std::time(nullptr));
        }
    } else {
        // Standard counter
        if (progress.current_count >= def->target_count) {
            internal_unlock(achievement_id);
        }
    }
}

void AchievementManager::set_progress(const std::string& achievement_id, int value) {
    auto& progress = m_progress[achievement_id];
    int delta = value - progress.current_count;
    if (delta > 0) {
        increment(achievement_id, delta);
    } else if (delta < 0) {
        progress.current_count = value;
    }
}

void AchievementManager::unlock(const std::string& achievement_id) {
    if (is_unlocked(achievement_id)) {
        return;
    }

    const auto* def = achievement_registry().get(achievement_id);
    if (!def) {
        core::log_warning("achievements", "Unknown achievement: {}", achievement_id);
        return;
    }

    if (!check_prerequisites(achievement_id)) {
        core::log_warning("achievements", "Prerequisites not met for: {}", achievement_id);
        return;
    }

    internal_unlock(achievement_id);
}

void AchievementManager::check_unlock(const std::string& achievement_id) {
    const auto* def = achievement_registry().get(achievement_id);
    if (!def) return;

    const auto& progress = m_progress[achievement_id];

    if (def->type == AchievementType::Counter || def->type == AchievementType::Progress) {
        if (progress.current_count >= def->target_count) {
            unlock(achievement_id);
        }
    }
}

// ============================================================================
// Queries
// ============================================================================

bool AchievementManager::is_unlocked(const std::string& achievement_id) const {
    auto it = m_progress.find(achievement_id);
    if (it != m_progress.end()) {
        return it->second.unlocked;
    }
    return false;
}

int AchievementManager::get_progress(const std::string& achievement_id) const {
    auto it = m_progress.find(achievement_id);
    if (it != m_progress.end()) {
        return it->second.current_count;
    }
    return 0;
}

float AchievementManager::get_progress_percent(const std::string& achievement_id) const {
    const auto* def = achievement_registry().get(achievement_id);
    if (!def || def->target_count <= 0) {
        return is_unlocked(achievement_id) ? 1.0f : 0.0f;
    }

    int current = get_progress(achievement_id);
    return std::min(1.0f, static_cast<float>(current) / static_cast<float>(def->target_count));
}

int AchievementManager::get_current_tier(const std::string& achievement_id) const {
    auto it = m_progress.find(achievement_id);
    if (it != m_progress.end()) {
        return it->second.current_tier;
    }
    return 0;
}

bool AchievementManager::is_tier_unlocked(const std::string& achievement_id, int tier) const {
    auto it = m_progress.find(achievement_id);
    if (it != m_progress.end() && tier >= 0 && static_cast<size_t>(tier) < it->second.tiers_unlocked.size()) {
        return it->second.tiers_unlocked[static_cast<size_t>(tier)];
    }
    return false;
}

const AchievementProgress* AchievementManager::get_achievement_progress(const std::string& achievement_id) const {
    auto it = m_progress.find(achievement_id);
    if (it != m_progress.end()) {
        return &it->second;
    }
    return nullptr;
}

// ============================================================================
// Bulk Queries
// ============================================================================

std::vector<std::string> AchievementManager::get_all_unlocked() const {
    std::vector<std::string> result;
    for (const auto& [id, progress] : m_progress) {
        if (progress.unlocked) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<std::string> AchievementManager::get_all_locked() const {
    std::vector<std::string> result;
    auto all = achievement_registry().get_all_achievement_ids();
    for (const auto& id : all) {
        if (!is_unlocked(id)) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<std::string> AchievementManager::get_in_progress() const {
    std::vector<std::string> result;
    for (const auto& [id, progress] : m_progress) {
        if (!progress.unlocked && progress.current_count > 0) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<std::string> AchievementManager::get_by_category(AchievementCategory category) const {
    return achievement_registry().get_by_category(category);
}

// ============================================================================
// Statistics
// ============================================================================

int AchievementManager::get_unlocked_count() const {
    int count = 0;
    for (const auto& [id, progress] : m_progress) {
        if (progress.unlocked) {
            ++count;
        }
    }
    return count;
}

int AchievementManager::get_total_count() const {
    return achievement_registry().get_total_achievements();
}

int AchievementManager::get_earned_points() const {
    int total = 0;
    for (const auto& [id, progress] : m_progress) {
        if (progress.unlocked) {
            const auto* def = achievement_registry().get(id);
            if (def) {
                if (def->is_tiered()) {
                    for (size_t i = 0; i < progress.tiers_unlocked.size() && i < def->tiers.size(); ++i) {
                        if (progress.tiers_unlocked[i]) {
                            total += def->tiers[i].points;
                        }
                    }
                } else {
                    total += def->points;
                }
            }
        }
    }
    return total;
}

int AchievementManager::get_total_points() const {
    return achievement_registry().get_total_points();
}

float AchievementManager::get_completion_percent() const {
    int total = get_total_count();
    if (total <= 0) return 0.0f;
    return static_cast<float>(get_unlocked_count()) / static_cast<float>(total);
}

int AchievementManager::get_unlocked_in_category(AchievementCategory category) const {
    int count = 0;
    auto in_category = achievement_registry().get_by_category(category);
    for (const auto& id : in_category) {
        if (is_unlocked(id)) {
            ++count;
        }
    }
    return count;
}

int AchievementManager::get_total_in_category(AchievementCategory category) const {
    return static_cast<int>(achievement_registry().get_by_category(category).size());
}

// ============================================================================
// Persistence
// ============================================================================

bool AchievementManager::load_progress(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            core::log_warning("achievements", "Could not open progress file: {}", path);
            return false;
        }

        json j = json::parse(file);

        m_progress.clear();

        for (auto& [id, data] : j.items()) {
            AchievementProgress progress;
            progress.achievement_id = id;
            if (data.contains("current_count")) progress.current_count = data["current_count"];
            if (data.contains("current_tier")) progress.current_tier = data["current_tier"];
            if (data.contains("unlocked")) progress.unlocked = data["unlocked"];
            if (data.contains("unlock_timestamp")) progress.unlock_timestamp = data["unlock_timestamp"];
            if (data.contains("first_progress_timestamp")) progress.first_progress_timestamp = data["first_progress_timestamp"];
            if (data.contains("tiers_unlocked")) {
                for (const auto& t : data["tiers_unlocked"]) {
                    progress.tiers_unlocked.push_back(t.get<bool>());
                }
            }
            m_progress[id] = progress;
        }

        core::log_info("achievements", "Loaded {} achievement progress entries", m_progress.size());
        return true;

    } catch (const std::exception& e) {
        core::log_error("achievements", "Failed to load progress: {}", e.what());
        return false;
    }
}

bool AchievementManager::save_progress(const std::string& path) const {
    try {
        json j;

        for (const auto& [id, progress] : m_progress) {
            j[id] = {
                {"current_count", progress.current_count},
                {"current_tier", progress.current_tier},
                {"unlocked", progress.unlocked},
                {"unlock_timestamp", progress.unlock_timestamp},
                {"first_progress_timestamp", progress.first_progress_timestamp},
                {"tiers_unlocked", progress.tiers_unlocked}
            };
        }

        std::ofstream file(path);
        if (!file.is_open()) {
            core::log_error("achievements", "Could not open progress file for writing: {}", path);
            return false;
        }

        file << j.dump(4);
        core::log_info("achievements", "Saved achievement progress to: {}", path);
        return true;

    } catch (const std::exception& e) {
        core::log_error("achievements", "Failed to save progress: {}", e.what());
        return false;
    }
}

void AchievementManager::reset_progress() {
    m_progress.clear();
    m_pending_notifications.clear();

    AchievementResetEvent event;
    event.all_reset = true;
    core::game_events().publish(event);

    core::log_info("achievements", "Reset all achievement progress");
}

void AchievementManager::reset_achievement(const std::string& achievement_id) {
    m_progress.erase(achievement_id);

    AchievementResetEvent event;
    event.achievement_id = achievement_id;
    event.all_reset = false;
    core::game_events().publish(event);
}

// ============================================================================
// Platform Integration
// ============================================================================

void AchievementManager::set_platform_callback(PlatformUnlockCallback callback) {
    m_platform_callback = std::move(callback);
}

void AchievementManager::sync_with_platform() {
    // TODO: Implement platform-specific sync
    core::log_info("achievements", "Syncing achievements with platform...");

    AchievementSyncEvent event;
    event.synced_count = static_cast<int>(m_progress.size());
    event.new_unlocks = 0;
    event.success = true;
    core::game_events().publish(event);
}

// ============================================================================
// Notifications
// ============================================================================

std::vector<AchievementNotification> AchievementManager::get_pending_notifications() {
    auto result = std::move(m_pending_notifications);
    m_pending_notifications.clear();
    return result;
}

void AchievementManager::clear_notifications() {
    m_pending_notifications.clear();
}

bool AchievementManager::has_pending_notifications() const {
    return !m_pending_notifications.empty();
}

// ============================================================================
// Callbacks
// ============================================================================

void AchievementManager::set_on_unlock(UnlockCallback callback) {
    m_on_unlock = std::move(callback);
}

void AchievementManager::set_on_progress(ProgressCallback callback) {
    m_on_progress = std::move(callback);
}

void AchievementManager::set_on_tier_unlock(TierCallback callback) {
    m_on_tier_unlock = std::move(callback);
}

// ============================================================================
// Debug
// ============================================================================

void AchievementManager::unlock_all() {
    auto all = achievement_registry().get_all_achievement_ids();
    for (const auto& id : all) {
        internal_unlock(id);
    }
    core::log_info("achievements", "Debug: Unlocked all achievements");
}

void AchievementManager::lock_all() {
    reset_progress();
    core::log_info("achievements", "Debug: Locked all achievements");
}

// ============================================================================
// Internal
// ============================================================================

void AchievementManager::internal_unlock(const std::string& achievement_id) {
    const auto* def = achievement_registry().get(achievement_id);
    if (!def) return;

    auto& progress = m_progress[achievement_id];
    progress.achievement_id = achievement_id;
    progress.unlocked = true;
    progress.unlock_timestamp = static_cast<uint64_t>(std::time(nullptr));
    progress.current_count = def->target_count;

    core::log_info("achievements", "Achievement unlocked: {} ({})", achievement_id, def->display_name);

    // Create notification
    create_notification(achievement_id);

    // Fire callback
    if (m_on_unlock) {
        m_on_unlock(*def);
    }

    // Fire event
    AchievementUnlockedEvent event;
    event.achievement_id = achievement_id;
    event.display_name = def->display_name;
    event.description = def->description;
    event.icon_path = def->icon_path;
    event.points = def->points;
    event.timestamp = progress.unlock_timestamp;
    core::game_events().publish(event);

    // Platform callback
    if (m_platform_callback && !def->platform_id.empty()) {
        m_platform_callback(achievement_id, def->platform_id);
    }
}

void AchievementManager::internal_unlock_tier(const std::string& achievement_id, int tier) {
    const auto* def = achievement_registry().get(achievement_id);
    if (!def || tier < 0 || tier >= def->get_tier_count()) return;

    auto& progress = m_progress[achievement_id];

    if (progress.tiers_unlocked.size() <= static_cast<size_t>(tier)) {
        progress.tiers_unlocked.resize(static_cast<size_t>(tier) + 1, false);
    }

    if (progress.tiers_unlocked[static_cast<size_t>(tier)]) {
        return;  // Already unlocked
    }

    progress.tiers_unlocked[static_cast<size_t>(tier)] = true;
    progress.current_tier = tier + 1;

    const auto* tier_def = def->get_tier(tier);
    if (!tier_def) return;

    core::log_info("achievements", "Achievement tier unlocked: {} - {} (tier {})",
                   achievement_id, tier_def->display_name, tier + 1);

    // Create notification
    create_notification(achievement_id, true, tier);

    // Fire callback
    if (m_on_tier_unlock) {
        m_on_tier_unlock(*def, tier);
    }

    // Fire event
    AchievementTierUnlockedEvent event;
    event.achievement_id = achievement_id;
    event.tier_index = tier;
    event.tier_name = tier_def->display_name;
    event.tier_points = tier_def->points;
    event.total_tiers = def->get_tier_count();
    event.is_final_tier = (tier == def->get_tier_count() - 1);
    core::game_events().publish(event);
}

bool AchievementManager::check_prerequisites(const std::string& achievement_id) const {
    const auto* def = achievement_registry().get(achievement_id);
    if (!def) return false;

    for (const auto& prereq : def->prerequisites) {
        if (!is_unlocked(prereq)) {
            return false;
        }
    }
    return true;
}

void AchievementManager::create_notification(const std::string& achievement_id, bool is_tier, int tier) {
    const auto* def = achievement_registry().get(achievement_id);
    if (!def) return;

    AchievementNotification notif;
    notif.achievement_id = achievement_id;
    notif.timestamp = static_cast<uint64_t>(std::time(nullptr));
    notif.is_tier_unlock = is_tier;
    notif.tier_index = tier;

    if (is_tier && tier >= 0 && tier < def->get_tier_count()) {
        const auto* tier_def = def->get_tier(tier);
        if (tier_def) {
            notif.display_name = def->display_name + " - " + tier_def->display_name;
            notif.points = tier_def->points;
        }
    } else {
        notif.display_name = def->display_name;
        notif.points = def->points;
    }

    notif.description = def->description;
    notif.icon_path = def->icon_path;

    m_pending_notifications.push_back(notif);
}

// ============================================================================
// Event Registration
// ============================================================================

void register_achievement_events() {
    core::log_info("achievements", "Registered achievement events");
}

} // namespace engine::achievements
