#include <engine/achievements/achievement_definition.hpp>
#include <engine/data/json_loader.hpp>
#include <engine/core/log.hpp>
#include <algorithm>

namespace engine::achievements {

// ============================================================================
// JSON Deserialization
// ============================================================================

namespace {

// Deserialize a single AchievementDefinition from JSON
std::optional<AchievementDefinition> deserialize_achievement(const nlohmann::json& j, std::string& error) {
    using namespace data::json_helpers;

    // Required: achievement_id
    if (!require_string(j, "achievement_id", error)) {
        return std::nullopt;
    }

    AchievementDefinition def;
    def.achievement_id = j["achievement_id"].get<std::string>();

    // Basic strings
    def.display_name = get_string(j, "display_name", def.achievement_id);
    def.description = get_string(j, "description");
    def.hidden_description = get_string(j, "hidden_description");
    def.icon_path = get_string(j, "icon_path");
    def.icon_locked_path = get_string(j, "icon_locked_path");
    def.platform_id = get_string(j, "platform_id");

    // Enums
    def.type = get_enum<AchievementType>(j, "type", AchievementType::Binary);
    def.category = get_enum<AchievementCategory>(j, "category", AchievementCategory::Misc);

    // Numeric values
    def.target_count = get_int(j, "target_count", 1);
    def.points = get_int(j, "points", 0);
    def.display_order = get_int(j, "display_order", 0);

    // Hidden settings
    def.is_hidden = get_bool(j, "is_hidden", false);
    def.is_hidden_until_progress = get_bool(j, "is_hidden_until_progress", false);
    def.hidden_progress_threshold = get_float(j, "hidden_progress_threshold", 0.5f);

    // String arrays
    def.prerequisites = get_string_array(j, "prerequisites");
    def.unlock_rewards = get_string_array(j, "unlock_rewards");

    // Tiers: array of {tier_id, display_name, target_count, points, rewards[]}
    if (j.contains("tiers") && j["tiers"].is_array()) {
        def.type = AchievementType::Tiered;
        for (const auto& tier_json : j["tiers"]) {
            if (!tier_json.is_object()) continue;

            AchievementTier tier;
            tier.tier_id = get_string(tier_json, "tier_id");
            tier.display_name = get_string(tier_json, "display_name");
            tier.target_count = get_int(tier_json, "target_count", 1);
            tier.points = get_int(tier_json, "points", 0);
            tier.rewards = get_string_array(tier_json, "rewards");
            def.tiers.push_back(tier);
        }
    }

    return def;
}

} // anonymous namespace

// ============================================================================
// AchievementDefinition
// ============================================================================

const AchievementTier* AchievementDefinition::get_tier(int index) const {
    if (index < 0 || index >= static_cast<int>(tiers.size())) {
        return nullptr;
    }
    return &tiers[static_cast<size_t>(index)];
}

int AchievementDefinition::get_total_points() const {
    if (is_tiered()) {
        int total = 0;
        for (const auto& tier : tiers) {
            total += tier.points;
        }
        return total;
    }
    return points;
}

// ============================================================================
// AchievementRegistry
// ============================================================================

AchievementRegistry& AchievementRegistry::instance() {
    static AchievementRegistry s_instance;
    return s_instance;
}

void AchievementRegistry::register_achievement(const AchievementDefinition& def) {
    if (def.achievement_id.empty()) {
        core::log(core::LogLevel::Error, "[Achievements] Cannot register achievement with empty ID");
        return;
    }

    if (m_achievements.contains(def.achievement_id)) {
        core::log(core::LogLevel::Warn, "[Achievements] Overwriting existing achievement: {}", def.achievement_id);
    }

    m_achievements[def.achievement_id] = def;
    core::log(core::LogLevel::Debug, "[Achievements] Registered achievement: {} ({})", def.achievement_id, def.display_name);
}

void AchievementRegistry::load_achievements(const std::string& path) {
    core::log(core::LogLevel::Info, "[Achievements] Loading achievements from: {}", path);

    auto result = data::load_json_array<AchievementDefinition>(path, deserialize_achievement, "achievements");

    // Log warnings
    for (const auto& warn : result.warnings) {
        core::log(core::LogLevel::Warn, "[Achievements] {}", warn);
    }

    // Log errors
    for (const auto& err : result.errors) {
        core::log(core::LogLevel::Error, "[Achievements] {}", err);
    }

    // Register successfully loaded achievements
    for (const auto& achievement : result.items) {
        register_achievement(achievement);
    }

    core::log(core::LogLevel::Info, "[Achievements] Loaded {} achievements ({} errors)",
              result.loaded_count(), result.error_count());
}

const AchievementDefinition* AchievementRegistry::get(const std::string& achievement_id) const {
    auto it = m_achievements.find(achievement_id);
    if (it != m_achievements.end()) {
        return &it->second;
    }
    return nullptr;
}

bool AchievementRegistry::exists(const std::string& achievement_id) const {
    return m_achievements.contains(achievement_id);
}

std::vector<std::string> AchievementRegistry::get_all_achievement_ids() const {
    std::vector<std::string> result;
    result.reserve(m_achievements.size());
    for (const auto& [id, def] : m_achievements) {
        result.push_back(id);
    }
    return result;
}

std::vector<std::string> AchievementRegistry::get_by_category(AchievementCategory category) const {
    std::vector<std::string> result;
    for (const auto& [id, def] : m_achievements) {
        if (def.category == category) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<std::string> AchievementRegistry::get_visible_achievements() const {
    std::vector<std::string> result;
    for (const auto& [id, def] : m_achievements) {
        if (!def.is_hidden) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<std::string> AchievementRegistry::get_hidden_achievements() const {
    std::vector<std::string> result;
    for (const auto& [id, def] : m_achievements) {
        if (def.is_hidden) {
            result.push_back(id);
        }
    }
    return result;
}

int AchievementRegistry::get_total_achievements() const {
    return static_cast<int>(m_achievements.size());
}

int AchievementRegistry::get_total_points() const {
    int total = 0;
    for (const auto& [id, def] : m_achievements) {
        total += def.get_total_points();
    }
    return total;
}

void AchievementRegistry::clear() {
    m_achievements.clear();
    core::log(core::LogLevel::Info, "[Achievements] Cleared achievement registry");
}

// ============================================================================
// AchievementBuilder
// ============================================================================

AchievementBuilder& AchievementBuilder::id(const std::string& achievement_id) {
    m_def.achievement_id = achievement_id;
    return *this;
}

AchievementBuilder& AchievementBuilder::name(const std::string& display_name) {
    m_def.display_name = display_name;
    return *this;
}

AchievementBuilder& AchievementBuilder::description(const std::string& desc) {
    m_def.description = desc;
    return *this;
}

AchievementBuilder& AchievementBuilder::hidden_description(const std::string& desc) {
    m_def.hidden_description = desc;
    return *this;
}

AchievementBuilder& AchievementBuilder::icon(const std::string& path) {
    m_def.icon_path = path;
    return *this;
}

AchievementBuilder& AchievementBuilder::locked_icon(const std::string& path) {
    m_def.icon_locked_path = path;
    return *this;
}

AchievementBuilder& AchievementBuilder::type(AchievementType t) {
    m_def.type = t;
    return *this;
}

AchievementBuilder& AchievementBuilder::category(AchievementCategory cat) {
    m_def.category = cat;
    return *this;
}

AchievementBuilder& AchievementBuilder::target(int count) {
    m_def.target_count = count;
    return *this;
}

AchievementBuilder& AchievementBuilder::tier(const std::string& tier_id, const std::string& tier_name,
                                              int target, int pts) {
    m_def.type = AchievementType::Tiered;
    AchievementTier t;
    t.tier_id = tier_id;
    t.display_name = tier_name;
    t.target_count = target;
    t.points = pts;
    m_def.tiers.push_back(t);
    return *this;
}

AchievementBuilder& AchievementBuilder::hidden(bool hide_until_progress, float threshold) {
    m_def.is_hidden = true;
    m_def.is_hidden_until_progress = hide_until_progress;
    m_def.hidden_progress_threshold = threshold;
    return *this;
}

AchievementBuilder& AchievementBuilder::prerequisite(const std::string& achievement_id) {
    m_def.prerequisites.push_back(achievement_id);
    return *this;
}

AchievementBuilder& AchievementBuilder::points(int pts) {
    m_def.points = pts;
    return *this;
}

AchievementBuilder& AchievementBuilder::reward(const std::string& reward_id) {
    m_def.unlock_rewards.push_back(reward_id);
    return *this;
}

AchievementBuilder& AchievementBuilder::platform_id(const std::string& pid) {
    m_def.platform_id = pid;
    return *this;
}

AchievementBuilder& AchievementBuilder::order(int display_order) {
    m_def.display_order = display_order;
    return *this;
}

AchievementDefinition AchievementBuilder::build() const {
    return m_def;
}

void AchievementBuilder::register_achievement() const {
    achievement_registry().register_achievement(m_def);
}

} // namespace engine::achievements
