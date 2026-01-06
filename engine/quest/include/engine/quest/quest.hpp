#pragma once

#include <engine/quest/objective.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <functional>

namespace engine::quest {

// ============================================================================
// Quest State
// ============================================================================

enum class QuestState {
    Unavailable,    // Prerequisites not met
    Available,      // Can be started
    Active,         // In progress
    Completed,      // Successfully finished
    Failed,         // Failed
    Abandoned       // Player abandoned
};

// ============================================================================
// Quest Category
// ============================================================================

enum class QuestCategory {
    Main,           // Main storyline
    Side,           // Side quests
    Faction,        // Faction-specific
    Bounty,         // Repeatable bounties
    Collection,     // Collection quests
    Exploration,    // Exploration/discovery
    Tutorial        // Tutorial quests
};

// ============================================================================
// Quest Rewards
// ============================================================================

struct QuestReward {
    std::string type;       // "experience", "gold", "item", "reputation", etc.
    std::string value;      // Item ID, amount as string, etc.
    int32_t amount = 1;

    // For display
    std::string display_name;
    std::string icon;
};

// ============================================================================
// Quest Prerequisites
// ============================================================================

struct QuestPrerequisite {
    enum class Type {
        QuestCompleted,     // Another quest must be complete
        QuestActive,        // Another quest must be active
        Level,              // Player level requirement
        Reputation,         // Faction reputation
        Item,               // Must have item
        Flag,               // Game flag set
        Custom              // Custom condition
    };

    Type type = Type::QuestCompleted;
    std::string key;        // Quest ID, flag name, etc.
    int32_t value = 0;      // Level, reputation amount, item count
    std::function<bool()> custom_check;
};

// ============================================================================
// Quest Definition
// ============================================================================

struct Quest {
    std::string id;
    std::string title_key;          // Localization key
    std::string description_key;    // Localization key
    std::string summary_key;        // Short summary for quest log

    QuestCategory category = QuestCategory::Side;
    QuestState state = QuestState::Unavailable;

    std::vector<Objective> objectives;
    std::vector<QuestReward> rewards;
    std::vector<QuestPrerequisite> prerequisites;

    // Quest giver info
    scene::Entity quest_giver = scene::NullEntity;
    std::string quest_giver_name;

    // Optional turn-in location (if different from giver)
    scene::Entity turn_in_entity = scene::NullEntity;
    std::optional<Vec3> turn_in_location;

    // Display
    std::string icon;
    int32_t display_order = 0;
    bool is_tracked = false;        // Shown in HUD

    // Flags
    bool is_repeatable = false;
    bool auto_track_on_accept = true;
    bool fail_on_objective_fail = true;
    int32_t repeat_count = 0;       // Times completed

    // Time tracking
    float time_started = 0.0f;
    float time_completed = 0.0f;

    // ========================================================================
    // Helper Methods
    // ========================================================================

    bool is_available() const { return state == QuestState::Available; }
    bool is_active() const { return state == QuestState::Active; }
    bool is_completed() const { return state == QuestState::Completed; }
    bool is_failed() const { return state == QuestState::Failed; }

    Objective* find_objective(const std::string& obj_id) {
        for (auto& obj : objectives) {
            if (obj.id == obj_id) return &obj;
        }
        return nullptr;
    }

    const Objective* find_objective(const std::string& obj_id) const {
        for (const auto& obj : objectives) {
            if (obj.id == obj_id) return &obj;
        }
        return nullptr;
    }

    bool all_required_complete() const {
        for (const auto& obj : objectives) {
            if (!obj.is_optional && !obj.is_completed()) {
                return false;
            }
        }
        return true;
    }

    bool any_failed() const {
        for (const auto& obj : objectives) {
            if (!obj.is_optional && obj.is_failed()) {
                return true;
            }
        }
        return false;
    }

    int32_t get_active_objective_count() const {
        int32_t count = 0;
        for (const auto& obj : objectives) {
            if (obj.is_active()) ++count;
        }
        return count;
    }

    int32_t get_completed_objective_count() const {
        int32_t count = 0;
        for (const auto& obj : objectives) {
            if (obj.is_completed()) ++count;
        }
        return count;
    }

    float get_progress() const {
        if (objectives.empty()) return is_completed() ? 1.0f : 0.0f;

        int32_t required = 0;
        int32_t completed = 0;
        for (const auto& obj : objectives) {
            if (!obj.is_optional) {
                ++required;
                if (obj.is_completed()) ++completed;
            }
        }
        return required > 0 ? static_cast<float>(completed) / required : 1.0f;
    }

    std::vector<const Objective*> get_active_objectives() const {
        std::vector<const Objective*> result;
        for (const auto& obj : objectives) {
            if (obj.is_active() && !obj.is_hidden) {
                result.push_back(&obj);
            }
        }
        return result;
    }
};

// ============================================================================
// Quest Builder
// ============================================================================

class QuestBuilder {
public:
    QuestBuilder(const std::string& id) {
        m_quest.id = id;
    }

    QuestBuilder& title(const std::string& key) {
        m_quest.title_key = key;
        return *this;
    }

    QuestBuilder& description(const std::string& key) {
        m_quest.description_key = key;
        return *this;
    }

    QuestBuilder& summary(const std::string& key) {
        m_quest.summary_key = key;
        return *this;
    }

    QuestBuilder& category(QuestCategory cat) {
        m_quest.category = cat;
        return *this;
    }

    QuestBuilder& main_quest() {
        m_quest.category = QuestCategory::Main;
        return *this;
    }

    QuestBuilder& side_quest() {
        m_quest.category = QuestCategory::Side;
        return *this;
    }

    QuestBuilder& objective(Objective obj) {
        m_quest.objectives.push_back(std::move(obj));
        return *this;
    }

    QuestBuilder& reward(const std::string& type, const std::string& value, int32_t amount = 1) {
        QuestReward r;
        r.type = type;
        r.value = value;
        r.amount = amount;
        m_quest.rewards.push_back(r);
        return *this;
    }

    QuestBuilder& reward(QuestReward r) {
        m_quest.rewards.push_back(std::move(r));
        return *this;
    }

    QuestBuilder& requires_quest(const std::string& quest_id) {
        QuestPrerequisite p;
        p.type = QuestPrerequisite::Type::QuestCompleted;
        p.key = quest_id;
        m_quest.prerequisites.push_back(p);
        return *this;
    }

    QuestBuilder& requires_level(int32_t level) {
        QuestPrerequisite p;
        p.type = QuestPrerequisite::Type::Level;
        p.value = level;
        m_quest.prerequisites.push_back(p);
        return *this;
    }

    QuestBuilder& requires_flag(const std::string& flag) {
        QuestPrerequisite p;
        p.type = QuestPrerequisite::Type::Flag;
        p.key = flag;
        m_quest.prerequisites.push_back(p);
        return *this;
    }

    QuestBuilder& prerequisite(QuestPrerequisite p) {
        m_quest.prerequisites.push_back(std::move(p));
        return *this;
    }

    QuestBuilder& giver(scene::Entity entity, const std::string& name = "") {
        m_quest.quest_giver = entity;
        m_quest.quest_giver_name = name;
        return *this;
    }

    QuestBuilder& turn_in(scene::Entity entity) {
        m_quest.turn_in_entity = entity;
        return *this;
    }

    QuestBuilder& turn_in_location(const Vec3& pos) {
        m_quest.turn_in_location = pos;
        return *this;
    }

    QuestBuilder& icon(const std::string& i) {
        m_quest.icon = i;
        return *this;
    }

    QuestBuilder& repeatable(bool value = true) {
        m_quest.is_repeatable = value;
        return *this;
    }

    QuestBuilder& order(int32_t o) {
        m_quest.display_order = o;
        return *this;
    }

    Quest build() { return std::move(m_quest); }

private:
    Quest m_quest;
};

inline QuestBuilder make_quest(const std::string& id) {
    return QuestBuilder(id);
}

} // namespace engine::quest
