#include <engine/quest/quest_manager.hpp>
#include <engine/core/event_dispatcher.hpp>
#include <engine/core/log.hpp>
#include <engine/scene/transform.hpp>
#include <algorithm>

namespace engine::quest {

namespace {

Vec3 get_entity_position(scene::World& world, scene::Entity entity) {
    auto* world_transform = world.try_get<scene::WorldTransform>(entity);
    if (world_transform) {
        return world_transform->get_position();
    }
    auto* local_transform = world.try_get<scene::LocalTransform>(entity);
    if (local_transform) {
        return local_transform->position;
    }
    return Vec3(0.0f);
}

} // anonymous namespace

// ============================================================================
// Singleton Instance
// ============================================================================

QuestManager& QuestManager::instance() {
    static QuestManager s_instance;
    return s_instance;
}

// ============================================================================
// Quest Registration
// ============================================================================

void QuestManager::register_quest(Quest quest) {
    std::string id = quest.id;
    m_quests[id] = std::move(quest);
    core::log(core::LogLevel::Info, "Quest registered: {}", id);
}

void QuestManager::unregister_quest(const std::string& quest_id) {
    m_quests.erase(quest_id);
}

Quest* QuestManager::get_quest(const std::string& quest_id) {
    auto it = m_quests.find(quest_id);
    return it != m_quests.end() ? &it->second : nullptr;
}

const Quest* QuestManager::get_quest(const std::string& quest_id) const {
    auto it = m_quests.find(quest_id);
    return it != m_quests.end() ? &it->second : nullptr;
}

bool QuestManager::has_quest(const std::string& quest_id) const {
    return m_quests.count(quest_id) > 0;
}

// ============================================================================
// Quest Lifecycle
// ============================================================================

bool QuestManager::start_quest(const std::string& quest_id) {
    Quest* quest = get_quest(quest_id);
    if (!quest) {
        core::log(core::LogLevel::Warning, "Cannot start unknown quest: {}", quest_id);
        return false;
    }

    if (quest->state == QuestState::Active) {
        return false;  // Already active
    }

    if (!can_start_quest(quest_id)) {
        core::log(core::LogLevel::Warning, "Quest prerequisites not met: {}", quest_id);
        return false;
    }

    quest->state = QuestState::Active;
    quest->time_started = 0.0f;  // Would use game time

    // Start first objective(s)
    for (auto& obj : quest->objectives) {
        if (obj.state == ObjectiveState::Inactive) {
            obj.state = ObjectiveState::Active;
            dispatch_event(ObjectiveStartedEvent{quest_id, obj.id});
            break;  // Start one at a time by default
        }
    }

    if (quest->auto_track_on_accept) {
        track_quest(quest_id);
    }

    dispatch_event(QuestStartedEvent{quest_id});
    core::log(core::LogLevel::Info, "Quest started: {}", quest_id);
    return true;
}

bool QuestManager::complete_quest(const std::string& quest_id) {
    Quest* quest = get_quest(quest_id);
    if (!quest || quest->state != QuestState::Active) {
        return false;
    }

    quest->state = QuestState::Completed;
    quest->time_completed = 0.0f;  // Would use game time

    if (quest->is_repeatable) {
        quest->repeat_count++;
    }

    // Give rewards
    give_rewards(*quest);

    // Untrack
    untrack_quest(quest_id);

    dispatch_event(QuestCompletedEvent{quest_id, quest->rewards});
    core::log(core::LogLevel::Info, "Quest completed: {}", quest_id);

    // Check if other quests became available
    update_quest_availability();

    return true;
}

bool QuestManager::fail_quest(const std::string& quest_id, const std::string& reason) {
    Quest* quest = get_quest(quest_id);
    if (!quest || quest->state != QuestState::Active) {
        return false;
    }

    quest->state = QuestState::Failed;
    untrack_quest(quest_id);

    dispatch_event(QuestFailedEvent{quest_id, reason});
    core::log(core::LogLevel::Info, "Quest failed: {} - {}", quest_id, reason);
    return true;
}

bool QuestManager::abandon_quest(const std::string& quest_id) {
    Quest* quest = get_quest(quest_id);
    if (!quest || quest->state != QuestState::Active) {
        return false;
    }

    quest->state = QuestState::Abandoned;
    untrack_quest(quest_id);

    // Reset objectives
    for (auto& obj : quest->objectives) {
        obj.state = ObjectiveState::Inactive;
    }

    dispatch_event(QuestAbandonedEvent{quest_id});
    core::log(core::LogLevel::Info, "Quest abandoned: {}", quest_id);
    return true;
}

bool QuestManager::can_start_quest(const std::string& quest_id) const {
    const Quest* quest = get_quest(quest_id);
    if (!quest) return false;

    if (quest->state != QuestState::Available) return false;

    return check_prerequisites(quest_id);
}

bool QuestManager::check_prerequisites(const std::string& quest_id) const {
    const Quest* quest = get_quest(quest_id);
    if (!quest) return false;

    for (const auto& prereq : quest->prerequisites) {
        // Check custom checker first
        auto it = m_prerequisite_checkers.find(prereq.type);
        if (it != m_prerequisite_checkers.end()) {
            if (!it->second(prereq)) return false;
            continue;
        }

        // Default checks
        switch (prereq.type) {
            case QuestPrerequisite::Type::QuestCompleted: {
                const Quest* required = get_quest(prereq.key);
                if (!required || required->state != QuestState::Completed) {
                    return false;
                }
                break;
            }
            case QuestPrerequisite::Type::QuestActive: {
                const Quest* required = get_quest(prereq.key);
                if (!required || required->state != QuestState::Active) {
                    return false;
                }
                break;
            }
            case QuestPrerequisite::Type::Level: {
                if (m_player_level_getter) {
                    if (m_player_level_getter() < prereq.value) {
                        return false;
                    }
                }
                break;
            }
            case QuestPrerequisite::Type::Flag: {
                if (!get_flag(prereq.key)) {
                    return false;
                }
                break;
            }
            case QuestPrerequisite::Type::Custom: {
                if (prereq.custom_check && !prereq.custom_check()) {
                    return false;
                }
                break;
            }
            default:
                break;
        }
    }

    return true;
}

// ============================================================================
// Objective Management
// ============================================================================

bool QuestManager::start_objective(const std::string& quest_id, const std::string& objective_id) {
    Quest* quest = get_quest(quest_id);
    if (!quest || quest->state != QuestState::Active) return false;

    Objective* obj = quest->find_objective(objective_id);
    if (!obj || obj->state != ObjectiveState::Inactive) return false;

    obj->state = ObjectiveState::Active;
    dispatch_event(ObjectiveStartedEvent{quest_id, objective_id});
    return true;
}

bool QuestManager::complete_objective(const std::string& quest_id, const std::string& objective_id) {
    Quest* quest = get_quest(quest_id);
    if (!quest || quest->state != QuestState::Active) return false;

    Objective* obj = quest->find_objective(objective_id);
    if (!obj || obj->state != ObjectiveState::Active) return false;

    obj->state = ObjectiveState::Completed;
    dispatch_event(ObjectiveCompletedEvent{quest_id, objective_id});

    // Start next objective if any
    bool found_current = false;
    for (auto& next_obj : quest->objectives) {
        if (&next_obj == obj) {
            found_current = true;
            continue;
        }
        if (found_current && next_obj.state == ObjectiveState::Inactive) {
            next_obj.state = ObjectiveState::Active;
            dispatch_event(ObjectiveStartedEvent{quest_id, next_obj.id});
            break;
        }
    }

    // Check if quest is complete
    auto_complete_check(*quest);

    return true;
}

bool QuestManager::fail_objective(const std::string& quest_id, const std::string& objective_id) {
    Quest* quest = get_quest(quest_id);
    if (!quest || quest->state != QuestState::Active) return false;

    Objective* obj = quest->find_objective(objective_id);
    if (!obj) return false;

    obj->state = ObjectiveState::Failed;
    dispatch_event(ObjectiveFailedEvent{quest_id, objective_id});

    // Fail quest if required objective failed
    if (!obj->is_optional && quest->fail_on_objective_fail) {
        fail_quest(quest_id, "Required objective failed: " + objective_id);
    }

    return true;
}

void QuestManager::set_objective_progress(const std::string& quest_id, const std::string& objective_id,
                                           int32_t current, int32_t target) {
    Quest* quest = get_quest(quest_id);
    if (!quest) return;

    Objective* obj = quest->find_objective(objective_id);
    if (!obj) return;

    if (obj->type == ObjectiveType::Counter) {
        if (auto* data = std::get_if<CounterData>(&obj->data)) {
            data->current = current;
            data->target = target;
        }
    } else if (obj->type == ObjectiveType::Kill) {
        if (auto* data = std::get_if<KillData>(&obj->data)) {
            data->current = current;
            data->target = target;
        }
    }

    dispatch_event(ObjectiveProgressEvent{quest_id, objective_id, obj->get_progress(), obj->get_progress_text()});

    // Auto-complete if target reached
    if (obj->auto_complete && current >= target) {
        complete_objective(quest_id, objective_id);
    }
}

// ============================================================================
// Global Counters
// ============================================================================

void QuestManager::increment_counter(const std::string& key, int32_t amount) {
    m_counters[key] += amount;
    process_counter_objectives(key);
}

void QuestManager::decrement_counter(const std::string& key, int32_t amount) {
    m_counters[key] = std::max(0, m_counters[key] - amount);
}

void QuestManager::set_counter(const std::string& key, int32_t value) {
    m_counters[key] = value;
    process_counter_objectives(key);
}

int32_t QuestManager::get_counter(const std::string& key) const {
    auto it = m_counters.find(key);
    return it != m_counters.end() ? it->second : 0;
}

void QuestManager::report_kill(const std::string& enemy_type, const std::string& faction) {
    process_kill_objectives(enemy_type, faction);
}

// ============================================================================
// Location Tracking
// ============================================================================

void QuestManager::check_location_objectives(scene::World& world, scene::Entity player, const Vec3& position) {
    m_player_entity = player;

    for (auto& [quest_id, quest] : m_quests) {
        if (quest.state != QuestState::Active) continue;

        for (auto& obj : quest.objectives) {
            if (obj.state != ObjectiveState::Active) continue;
            if (obj.type != ObjectiveType::Location) continue;

            auto* loc_data = std::get_if<LocationData>(&obj.data);
            if (!loc_data) continue;

            float distance = glm::length(position - loc_data->target_position);
            if (distance <= loc_data->radius) {
                if (obj.auto_complete) {
                    complete_objective(quest_id, obj.id);
                }
            }
        }
    }
}

// ============================================================================
// Interaction Tracking
// ============================================================================

void QuestManager::report_interaction(scene::Entity target, const std::string& interaction_type) {
    for (auto& [quest_id, quest] : m_quests) {
        if (quest.state != QuestState::Active) continue;

        for (auto& obj : quest.objectives) {
            if (obj.state != ObjectiveState::Active) continue;
            if (obj.type != ObjectiveType::Interact) continue;

            auto* interact_data = std::get_if<InteractData>(&obj.data);
            if (!interact_data) continue;

            // Check if this is the target entity
            if (interact_data->target_entity != scene::NullEntity &&
                interact_data->target_entity == target) {
                if (interact_data->interaction_type.empty() ||
                    interact_data->interaction_type == interaction_type) {
                    if (obj.auto_complete) {
                        complete_objective(quest_id, obj.id);
                    }
                }
            }
        }
    }
}

// ============================================================================
// Game Flags
// ============================================================================

void QuestManager::set_flag(const std::string& flag, bool value) {
    if (value) {
        m_flags.insert(flag);
    } else {
        m_flags.erase(flag);
    }
    update_quest_availability();
}

bool QuestManager::get_flag(const std::string& flag) const {
    return m_flags.count(flag) > 0;
}

void QuestManager::clear_flag(const std::string& flag) {
    m_flags.erase(flag);
}

// ============================================================================
// Queries
// ============================================================================

std::vector<Quest*> QuestManager::get_active_quests() {
    std::vector<Quest*> result;
    for (auto& [id, quest] : m_quests) {
        if (quest.state == QuestState::Active) {
            result.push_back(&quest);
        }
    }
    return result;
}

std::vector<const Quest*> QuestManager::get_active_quests() const {
    std::vector<const Quest*> result;
    for (const auto& [id, quest] : m_quests) {
        if (quest.state == QuestState::Active) {
            result.push_back(&quest);
        }
    }
    return result;
}

std::vector<Quest*> QuestManager::get_available_quests() {
    std::vector<Quest*> result;
    for (auto& [id, quest] : m_quests) {
        if (quest.state == QuestState::Available) {
            result.push_back(&quest);
        }
    }
    return result;
}

std::vector<Quest*> QuestManager::get_completed_quests() {
    std::vector<Quest*> result;
    for (auto& [id, quest] : m_quests) {
        if (quest.state == QuestState::Completed) {
            result.push_back(&quest);
        }
    }
    return result;
}

std::vector<Quest*> QuestManager::get_quests_by_category(QuestCategory category) {
    std::vector<Quest*> result;
    for (auto& [id, quest] : m_quests) {
        if (quest.category == category) {
            result.push_back(&quest);
        }
    }
    return result;
}

std::vector<Quest*> QuestManager::get_tracked_quests() {
    std::vector<Quest*> result;
    for (const auto& id : m_tracked_quests) {
        if (Quest* q = get_quest(id)) {
            result.push_back(q);
        }
    }
    return result;
}

Quest* QuestManager::get_tracked_main_quest() {
    for (const auto& id : m_tracked_quests) {
        if (Quest* q = get_quest(id)) {
            if (q->category == QuestCategory::Main && q->state == QuestState::Active) {
                return q;
            }
        }
    }
    return nullptr;
}

const Quest* QuestManager::get_active_objective_quest() const {
    for (const auto& [id, quest] : m_quests) {
        if (quest.state == QuestState::Active) {
            for (const auto& obj : quest.objectives) {
                if (obj.state == ObjectiveState::Active) {
                    return &quest;
                }
            }
        }
    }
    return nullptr;
}

bool QuestManager::is_quest_active(const std::string& quest_id) const {
    const Quest* q = get_quest(quest_id);
    return q && q->state == QuestState::Active;
}

bool QuestManager::is_quest_completed(const std::string& quest_id) const {
    const Quest* q = get_quest(quest_id);
    return q && q->state == QuestState::Completed;
}

bool QuestManager::is_quest_failed(const std::string& quest_id) const {
    const Quest* q = get_quest(quest_id);
    return q && q->state == QuestState::Failed;
}

// ============================================================================
// Tracking
// ============================================================================

void QuestManager::track_quest(const std::string& quest_id) {
    if (m_tracked_quests.size() >= static_cast<size_t>(m_max_tracked)) {
        // Remove oldest tracked quest
        if (!m_tracked_quests.empty()) {
            m_tracked_quests.erase(m_tracked_quests.begin());
        }
    }
    m_tracked_quests.insert(quest_id);

    if (Quest* q = get_quest(quest_id)) {
        q->is_tracked = true;
    }
}

void QuestManager::untrack_quest(const std::string& quest_id) {
    m_tracked_quests.erase(quest_id);

    if (Quest* q = get_quest(quest_id)) {
        q->is_tracked = false;
    }
}

bool QuestManager::is_tracked(const std::string& quest_id) const {
    return m_tracked_quests.count(quest_id) > 0;
}

void QuestManager::set_max_tracked(int32_t max) {
    m_max_tracked = max;
}

// ============================================================================
// Update
// ============================================================================

void QuestManager::update(scene::World& world, float dt) {
    update_objective_timers(dt);
    update_quest_availability();

    // Update escort objectives
    for (auto& [quest_id, quest] : m_quests) {
        if (quest.state != QuestState::Active) continue;

        for (auto& obj : quest.objectives) {
            if (obj.state != ObjectiveState::Active) continue;

            if (obj.type == ObjectiveType::Escort) {
                auto* escort_data = std::get_if<EscortData>(&obj.data);
                if (!escort_data) continue;

                if (escort_data->escort_target == scene::NullEntity) continue;
                if (!world.valid(escort_data->escort_target)) {
                    fail_objective(quest_id, obj.id);
                    continue;
                }

                Vec3 escort_pos = get_entity_position(world, escort_data->escort_target);
                float dist_to_dest = glm::length(escort_pos - escort_data->destination);

                if (dist_to_dest <= escort_data->destination_radius) {
                    complete_objective(quest_id, obj.id);
                }
            }
        }
    }
}

// ============================================================================
// Save/Load
// ============================================================================

void QuestManager::save_state(nlohmann::json& data) const {
    // Save quest states
    nlohmann::json quests_json = nlohmann::json::array();
    for (const auto& [id, quest] : m_quests) {
        nlohmann::json quest_json;
        quest_json["id"] = id;
        quest_json["state"] = static_cast<int>(quest.state);
        quest_json["repeat_count"] = quest.repeat_count;
        quest_json["is_tracked"] = quest.is_tracked;

        // Save objectives
        nlohmann::json objectives_json = nlohmann::json::array();
        for (const auto& obj : quest.objectives) {
            nlohmann::json obj_json;
            obj_json["id"] = obj.id;
            obj_json["state"] = static_cast<int>(obj.state);

            // Save progress data
            if (obj.type == ObjectiveType::Counter) {
                if (auto* d = std::get_if<CounterData>(&obj.data)) {
                    obj_json["current"] = d->current;
                }
            } else if (obj.type == ObjectiveType::Kill) {
                if (auto* d = std::get_if<KillData>(&obj.data)) {
                    obj_json["current"] = d->current;
                }
            } else if (obj.type == ObjectiveType::Timer) {
                if (auto* d = std::get_if<TimerData>(&obj.data)) {
                    obj_json["elapsed"] = d->elapsed;
                }
            }

            objectives_json.push_back(obj_json);
        }
        quest_json["objectives"] = objectives_json;
        quests_json.push_back(quest_json);
    }
    data["quests"] = quests_json;

    // Save counters
    data["counters"] = m_counters;

    // Save flags
    data["flags"] = nlohmann::json::array();
    for (const auto& flag : m_flags) {
        data["flags"].push_back(flag);
    }

    // Save tracked quests
    data["tracked"] = nlohmann::json::array();
    for (const auto& id : m_tracked_quests) {
        data["tracked"].push_back(id);
    }
}

void QuestManager::load_state(const nlohmann::json& data) {
    // Load quest states
    if (data.contains("quests")) {
        for (const auto& quest_json : data["quests"]) {
            std::string id = quest_json["id"];
            Quest* quest = get_quest(id);
            if (!quest) continue;

            quest->state = static_cast<QuestState>(quest_json["state"].get<int>());
            quest->repeat_count = quest_json.value("repeat_count", 0);
            quest->is_tracked = quest_json.value("is_tracked", false);

            // Load objectives
            if (quest_json.contains("objectives")) {
                for (const auto& obj_json : quest_json["objectives"]) {
                    std::string obj_id = obj_json["id"];
                    Objective* obj = quest->find_objective(obj_id);
                    if (!obj) continue;

                    obj->state = static_cast<ObjectiveState>(obj_json["state"].get<int>());

                    if (obj_json.contains("current")) {
                        if (obj->type == ObjectiveType::Counter) {
                            if (auto* d = std::get_if<CounterData>(&obj->data)) {
                                d->current = obj_json["current"];
                            }
                        } else if (obj->type == ObjectiveType::Kill) {
                            if (auto* d = std::get_if<KillData>(&obj->data)) {
                                d->current = obj_json["current"];
                            }
                        }
                    }
                    if (obj_json.contains("elapsed")) {
                        if (auto* d = std::get_if<TimerData>(&obj->data)) {
                            d->elapsed = obj_json["elapsed"];
                        }
                    }
                }
            }
        }
    }

    // Load counters
    if (data.contains("counters")) {
        m_counters = data["counters"].get<std::unordered_map<std::string, int32_t>>();
    }

    // Load flags
    m_flags.clear();
    if (data.contains("flags")) {
        for (const auto& flag : data["flags"]) {
            m_flags.insert(flag.get<std::string>());
        }
    }

    // Load tracked quests
    m_tracked_quests.clear();
    if (data.contains("tracked")) {
        for (const auto& id : data["tracked"]) {
            m_tracked_quests.insert(id.get<std::string>());
        }
    }
}

void QuestManager::reset() {
    for (auto& [id, quest] : m_quests) {
        quest.state = QuestState::Unavailable;
        quest.repeat_count = 0;
        quest.is_tracked = false;
        for (auto& obj : quest.objectives) {
            obj.state = ObjectiveState::Inactive;
            // Reset progress
            if (auto* d = std::get_if<CounterData>(&obj.data)) d->current = 0;
            if (auto* d = std::get_if<KillData>(&obj.data)) d->current = 0;
            if (auto* d = std::get_if<TimerData>(&obj.data)) d->elapsed = 0.0f;
        }
    }
    m_counters.clear();
    m_flags.clear();
    m_tracked_quests.clear();
}

// ============================================================================
// Callbacks
// ============================================================================

void QuestManager::set_prerequisite_checker(QuestPrerequisite::Type type, PrerequisiteCheck check) {
    m_prerequisite_checkers[type] = std::move(check);
}

void QuestManager::set_reward_handler(const std::string& reward_type, RewardHandler handler) {
    m_reward_handlers[reward_type] = std::move(handler);
}

void QuestManager::set_player_level_getter(std::function<int32_t()> getter) {
    m_player_level_getter = std::move(getter);
}

// ============================================================================
// Private Helpers
// ============================================================================

void QuestManager::dispatch_event(const QuestStartedEvent& event) {
    core::EventDispatcher::instance().dispatch(event);
}

void QuestManager::dispatch_event(const QuestCompletedEvent& event) {
    core::EventDispatcher::instance().dispatch(event);
}

void QuestManager::dispatch_event(const QuestFailedEvent& event) {
    core::EventDispatcher::instance().dispatch(event);
}

void QuestManager::dispatch_event(const QuestAbandonedEvent& event) {
    core::EventDispatcher::instance().dispatch(event);
}

void QuestManager::dispatch_event(const ObjectiveStartedEvent& event) {
    core::EventDispatcher::instance().dispatch(event);
}

void QuestManager::dispatch_event(const ObjectiveCompletedEvent& event) {
    core::EventDispatcher::instance().dispatch(event);
}

void QuestManager::dispatch_event(const ObjectiveFailedEvent& event) {
    core::EventDispatcher::instance().dispatch(event);
}

void QuestManager::dispatch_event(const ObjectiveProgressEvent& event) {
    core::EventDispatcher::instance().dispatch(event);
}

void QuestManager::dispatch_event(const QuestAvailableEvent& event) {
    core::EventDispatcher::instance().dispatch(event);
}

void QuestManager::update_quest_availability() {
    for (auto& [id, quest] : m_quests) {
        if (quest.state == QuestState::Unavailable) {
            if (check_prerequisites(id)) {
                quest.state = QuestState::Available;
                dispatch_event(QuestAvailableEvent{id, quest.quest_giver});
            }
        }
    }
}

void QuestManager::update_objective_timers(float dt) {
    for (auto& [quest_id, quest] : m_quests) {
        if (quest.state != QuestState::Active) continue;

        for (auto& obj : quest.objectives) {
            if (obj.state != ObjectiveState::Active) continue;
            if (obj.type != ObjectiveType::Timer) continue;

            auto* timer_data = std::get_if<TimerData>(&obj.data);
            if (!timer_data) continue;

            timer_data->elapsed += dt;

            // Dispatch progress event
            dispatch_event(ObjectiveProgressEvent{
                quest_id, obj.id, obj.get_progress(), obj.get_progress_text()
            });

            if (timer_data->elapsed >= timer_data->time_limit) {
                if (timer_data->fail_on_timeout) {
                    fail_objective(quest_id, obj.id);
                } else {
                    complete_objective(quest_id, obj.id);
                }
            }
        }
    }
}

void QuestManager::process_counter_objectives(const std::string& counter_key) {
    int32_t value = get_counter(counter_key);

    for (auto& [quest_id, quest] : m_quests) {
        if (quest.state != QuestState::Active) continue;

        for (auto& obj : quest.objectives) {
            if (obj.state != ObjectiveState::Active) continue;
            if (obj.type != ObjectiveType::Counter) continue;

            auto* counter_data = std::get_if<CounterData>(&obj.data);
            if (!counter_data || counter_data->counter_key != counter_key) continue;

            counter_data->current = value;
            dispatch_event(ObjectiveProgressEvent{
                quest_id, obj.id, obj.get_progress(), obj.get_progress_text()
            });

            if (obj.auto_complete && counter_data->current >= counter_data->target) {
                complete_objective(quest_id, obj.id);
            }
        }
    }
}

void QuestManager::process_kill_objectives(const std::string& enemy_type, const std::string& faction) {
    for (auto& [quest_id, quest] : m_quests) {
        if (quest.state != QuestState::Active) continue;

        for (auto& obj : quest.objectives) {
            if (obj.state != ObjectiveState::Active) continue;
            if (obj.type != ObjectiveType::Kill) continue;

            auto* kill_data = std::get_if<KillData>(&obj.data);
            if (!kill_data) continue;

            // Check if this kill counts
            bool matches = false;
            if (!kill_data->enemy_type.empty() && kill_data->enemy_type == enemy_type) {
                matches = true;
            }
            if (!kill_data->enemy_faction.empty() && kill_data->enemy_faction == faction) {
                matches = true;
            }

            if (matches) {
                kill_data->current++;
                dispatch_event(ObjectiveProgressEvent{
                    quest_id, obj.id, obj.get_progress(), obj.get_progress_text()
                });

                if (obj.auto_complete && kill_data->current >= kill_data->target) {
                    complete_objective(quest_id, obj.id);
                }
            }
        }
    }
}

void QuestManager::give_rewards(const Quest& quest) {
    for (const auto& reward : quest.rewards) {
        auto it = m_reward_handlers.find(reward.type);
        if (it != m_reward_handlers.end()) {
            it->second(reward);
        }
    }
}

void QuestManager::auto_complete_check(Quest& quest) {
    if (quest.all_required_complete()) {
        complete_quest(quest.id);
    } else if (quest.any_failed() && quest.fail_on_objective_fail) {
        fail_quest(quest.id, "Required objective failed");
    }
}

// ============================================================================
// Quest Save Handler
// ============================================================================

void QuestSaveHandler::save(nlohmann::json& data) {
    QuestManager::instance().save_state(data);
}

void QuestSaveHandler::load(const nlohmann::json& data) {
    QuestManager::instance().load_state(data);
}

void QuestSaveHandler::reset() {
    QuestManager::instance().reset();
}

void QuestSaveHandler::migrate(nlohmann::json& data, int32_t from_version) {
    // Future migration logic
}

} // namespace engine::quest
