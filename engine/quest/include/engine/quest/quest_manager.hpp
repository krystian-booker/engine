#pragma once

#include <engine/quest/quest.hpp>
#include <engine/scene/world.hpp>
#include <engine/save/save_handler.hpp>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <optional>

namespace engine::quest {

// ============================================================================
// Quest Events
// ============================================================================

struct QuestStartedEvent {
    std::string quest_id;
};

struct QuestCompletedEvent {
    std::string quest_id;
    std::vector<QuestReward> rewards;
};

struct QuestFailedEvent {
    std::string quest_id;
    std::string reason;
};

struct QuestAbandonedEvent {
    std::string quest_id;
};

struct ObjectiveStartedEvent {
    std::string quest_id;
    std::string objective_id;
};

struct ObjectiveCompletedEvent {
    std::string quest_id;
    std::string objective_id;
};

struct ObjectiveFailedEvent {
    std::string quest_id;
    std::string objective_id;
};

struct ObjectiveProgressEvent {
    std::string quest_id;
    std::string objective_id;
    float progress;
    std::string progress_text;
};

struct QuestAvailableEvent {
    std::string quest_id;
    scene::Entity quest_giver;
};

// ============================================================================
// Quest Manager
// ============================================================================

class QuestManager {
public:
    static QuestManager& instance();

    // ========================================================================
    // Quest Registration
    // ========================================================================

    void register_quest(Quest quest);
    void unregister_quest(const std::string& quest_id);
    Quest* get_quest(const std::string& quest_id);
    const Quest* get_quest(const std::string& quest_id) const;
    bool has_quest(const std::string& quest_id) const;

    // ========================================================================
    // Quest Lifecycle
    // ========================================================================

    bool start_quest(const std::string& quest_id);
    bool complete_quest(const std::string& quest_id);
    bool fail_quest(const std::string& quest_id, const std::string& reason = "");
    bool abandon_quest(const std::string& quest_id);

    bool can_start_quest(const std::string& quest_id) const;
    bool check_prerequisites(const std::string& quest_id) const;

    // ========================================================================
    // Objective Management
    // ========================================================================

    bool start_objective(const std::string& quest_id, const std::string& objective_id);
    bool complete_objective(const std::string& quest_id, const std::string& objective_id);
    bool fail_objective(const std::string& quest_id, const std::string& objective_id);

    void set_objective_progress(const std::string& quest_id, const std::string& objective_id,
                                 int32_t current, int32_t target);

    // ========================================================================
    // Global Counters (for Counter/Kill objectives)
    // ========================================================================

    void increment_counter(const std::string& key, int32_t amount = 1);
    void decrement_counter(const std::string& key, int32_t amount = 1);
    void set_counter(const std::string& key, int32_t value);
    int32_t get_counter(const std::string& key) const;

    // Kill tracking helper
    void report_kill(const std::string& enemy_type, const std::string& faction = "");

    // ========================================================================
    // Location Tracking
    // ========================================================================

    void check_location_objectives(scene::World& world, scene::Entity player, const Vec3& position);

    // ========================================================================
    // Interaction Tracking
    // ========================================================================

    void report_interaction(scene::Entity target, const std::string& interaction_type = "");

    // ========================================================================
    // Game Flags
    // ========================================================================

    void set_flag(const std::string& flag, bool value = true);
    bool get_flag(const std::string& flag) const;
    void clear_flag(const std::string& flag);

    // ========================================================================
    // Queries
    // ========================================================================

    std::vector<Quest*> get_active_quests();
    std::vector<const Quest*> get_active_quests() const;
    std::vector<Quest*> get_available_quests();
    std::vector<Quest*> get_completed_quests();
    std::vector<Quest*> get_quests_by_category(QuestCategory category);
    std::vector<Quest*> get_tracked_quests();

    Quest* get_tracked_main_quest();
    const Quest* get_active_objective_quest() const;  // First quest with active objective

    bool is_quest_active(const std::string& quest_id) const;
    bool is_quest_completed(const std::string& quest_id) const;
    bool is_quest_failed(const std::string& quest_id) const;

    // ========================================================================
    // Tracking
    // ========================================================================

    void track_quest(const std::string& quest_id);
    void untrack_quest(const std::string& quest_id);
    bool is_tracked(const std::string& quest_id) const;
    void set_max_tracked(int32_t max);

    // ========================================================================
    // Update
    // ========================================================================

    void update(scene::World& world, float dt);

    // ========================================================================
    // Save/Load
    // ========================================================================

    void save_state(nlohmann::json& data) const;
    void load_state(const nlohmann::json& data);
    void reset();

    // ========================================================================
    // Callbacks
    // ========================================================================

    using PrerequisiteCheck = std::function<bool(const QuestPrerequisite&)>;
    using RewardHandler = std::function<void(const QuestReward&)>;

    void set_prerequisite_checker(QuestPrerequisite::Type type, PrerequisiteCheck check);
    void set_reward_handler(const std::string& reward_type, RewardHandler handler);

    // Player level getter for level prerequisites
    void set_player_level_getter(std::function<int32_t()> getter);

private:
    QuestManager() = default;

    void dispatch_event(const QuestStartedEvent& event);
    void dispatch_event(const QuestCompletedEvent& event);
    void dispatch_event(const QuestFailedEvent& event);
    void dispatch_event(const QuestAbandonedEvent& event);
    void dispatch_event(const ObjectiveStartedEvent& event);
    void dispatch_event(const ObjectiveCompletedEvent& event);
    void dispatch_event(const ObjectiveFailedEvent& event);
    void dispatch_event(const ObjectiveProgressEvent& event);
    void dispatch_event(const QuestAvailableEvent& event);

    void update_quest_availability();
    void update_objective_timers(float dt);
    void process_counter_objectives(const std::string& counter_key);
    void process_kill_objectives(const std::string& enemy_type, const std::string& faction);
    void give_rewards(const Quest& quest);
    void auto_complete_check(Quest& quest);

    std::unordered_map<std::string, Quest> m_quests;
    std::unordered_map<std::string, int32_t> m_counters;
    std::unordered_set<std::string> m_flags;
    std::unordered_set<std::string> m_tracked_quests;

    std::unordered_map<QuestPrerequisite::Type, PrerequisiteCheck> m_prerequisite_checkers;
    std::unordered_map<std::string, RewardHandler> m_reward_handlers;

    std::function<int32_t()> m_player_level_getter;

    int32_t m_max_tracked = 3;
    scene::Entity m_player_entity = scene::NullEntity;
};

// Convenience accessor
inline QuestManager& quests() { return QuestManager::instance(); }

// ============================================================================
// Quest Save Handler
// ============================================================================

class QuestSaveHandler : public save::ISaveHandler {
public:
    std::string get_id() const override { return "quest_manager"; }
    void save(nlohmann::json& data) override;
    void load(const nlohmann::json& data) override;
    void reset() override;
    int32_t get_version() const override { return 1; }
    void migrate(nlohmann::json& data, int32_t from_version) override;
};

} // namespace engine::quest
