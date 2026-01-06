#pragma once

#include <engine/core/types.hpp>
#include <engine/scene/entity.hpp>
#include <string>
#include <variant>
#include <optional>
#include <functional>

namespace engine::quest {

// ============================================================================
// Objective State
// ============================================================================

enum class ObjectiveState {
    Inactive,       // Not yet started
    Active,         // In progress
    Completed,      // Successfully finished
    Failed          // Failed (if failible)
};

// ============================================================================
// Objective Types
// ============================================================================

enum class ObjectiveType {
    Simple,         // Just needs to be marked complete
    Counter,        // Collect X of Y
    Location,       // Reach a specific area
    Interact,       // Interact with specific entity
    Kill,           // Defeat X enemies of type
    Timer,          // Complete within time limit
    Escort,         // Keep entity alive until destination
    Custom          // Custom condition via callback
};

// ============================================================================
// Objective Data Structures
// ============================================================================

struct CounterData {
    int32_t current = 0;
    int32_t target = 1;
    std::string counter_key;    // Global counter key to track
};

struct LocationData {
    Vec3 target_position{0.0f};
    float radius = 5.0f;
    std::string location_name;
    bool any_party_member = false;  // Any party member can trigger
};

struct InteractData {
    scene::Entity target_entity = scene::NullEntity;
    std::string target_tag;         // Alternative: find by tag
    std::string interaction_type;   // Optional specific interaction
};

struct KillData {
    int32_t current = 0;
    int32_t target = 1;
    std::string enemy_type;         // Enemy type/tag to track
    std::string enemy_faction;      // Or faction
};

struct TimerData {
    float time_limit = 60.0f;
    float elapsed = 0.0f;
    bool fail_on_timeout = true;
};

struct EscortData {
    scene::Entity escort_target = scene::NullEntity;
    Vec3 destination{0.0f};
    float destination_radius = 5.0f;
    float max_distance = 20.0f;     // Max distance before fail
};

using ObjectiveData = std::variant<
    std::monostate,     // Simple
    CounterData,
    LocationData,
    InteractData,
    KillData,
    TimerData,
    EscortData
>;

// ============================================================================
// Objective Definition
// ============================================================================

using ObjectiveCondition = std::function<bool()>;

struct Objective {
    std::string id;
    std::string title_key;          // Localization key
    std::string description_key;    // Localization key

    ObjectiveType type = ObjectiveType::Simple;
    ObjectiveState state = ObjectiveState::Inactive;
    ObjectiveData data;

    // Display settings
    bool show_in_hud = true;
    bool show_waypoint = true;
    int32_t display_order = 0;

    // Optional waypoint position (can be updated dynamically)
    std::optional<Vec3> waypoint_position;
    scene::Entity waypoint_entity = scene::NullEntity;

    // Custom condition for ObjectiveType::Custom
    ObjectiveCondition custom_condition;

    // Flags
    bool is_optional = false;
    bool is_hidden = false;         // Secret objective
    bool auto_complete = true;      // Auto-complete when conditions met

    // ========================================================================
    // Helper Methods
    // ========================================================================

    bool is_active() const { return state == ObjectiveState::Active; }
    bool is_completed() const { return state == ObjectiveState::Completed; }
    bool is_failed() const { return state == ObjectiveState::Failed; }

    float get_progress() const {
        switch (type) {
            case ObjectiveType::Counter:
                if (auto* d = std::get_if<CounterData>(&data)) {
                    return d->target > 0 ? static_cast<float>(d->current) / d->target : 0.0f;
                }
                break;
            case ObjectiveType::Kill:
                if (auto* d = std::get_if<KillData>(&data)) {
                    return d->target > 0 ? static_cast<float>(d->current) / d->target : 0.0f;
                }
                break;
            case ObjectiveType::Timer:
                if (auto* d = std::get_if<TimerData>(&data)) {
                    return d->time_limit > 0 ? d->elapsed / d->time_limit : 0.0f;
                }
                break;
            default:
                break;
        }
        return is_completed() ? 1.0f : 0.0f;
    }

    std::string get_progress_text() const {
        switch (type) {
            case ObjectiveType::Counter:
                if (auto* d = std::get_if<CounterData>(&data)) {
                    return std::to_string(d->current) + "/" + std::to_string(d->target);
                }
                break;
            case ObjectiveType::Kill:
                if (auto* d = std::get_if<KillData>(&data)) {
                    return std::to_string(d->current) + "/" + std::to_string(d->target);
                }
                break;
            case ObjectiveType::Timer:
                if (auto* d = std::get_if<TimerData>(&data)) {
                    float remaining = d->time_limit - d->elapsed;
                    int mins = static_cast<int>(remaining) / 60;
                    int secs = static_cast<int>(remaining) % 60;
                    return std::to_string(mins) + ":" + (secs < 10 ? "0" : "") + std::to_string(secs);
                }
                break;
            default:
                break;
        }
        return "";
    }
};

// ============================================================================
// Objective Builder
// ============================================================================

class ObjectiveBuilder {
public:
    ObjectiveBuilder(const std::string& id) {
        m_objective.id = id;
    }

    ObjectiveBuilder& title(const std::string& key) {
        m_objective.title_key = key;
        return *this;
    }

    ObjectiveBuilder& description(const std::string& key) {
        m_objective.description_key = key;
        return *this;
    }

    ObjectiveBuilder& simple() {
        m_objective.type = ObjectiveType::Simple;
        m_objective.data = std::monostate{};
        return *this;
    }

    ObjectiveBuilder& counter(const std::string& key, int32_t target) {
        m_objective.type = ObjectiveType::Counter;
        CounterData data;
        data.counter_key = key;
        data.target = target;
        m_objective.data = data;
        return *this;
    }

    ObjectiveBuilder& location(const Vec3& pos, float radius, const std::string& name = "") {
        m_objective.type = ObjectiveType::Location;
        LocationData data;
        data.target_position = pos;
        data.radius = radius;
        data.location_name = name;
        m_objective.data = data;
        m_objective.waypoint_position = pos;
        return *this;
    }

    ObjectiveBuilder& interact(scene::Entity entity) {
        m_objective.type = ObjectiveType::Interact;
        InteractData data;
        data.target_entity = entity;
        m_objective.data = data;
        m_objective.waypoint_entity = entity;
        return *this;
    }

    ObjectiveBuilder& interact_tag(const std::string& tag, const std::string& interaction = "") {
        m_objective.type = ObjectiveType::Interact;
        InteractData data;
        data.target_tag = tag;
        data.interaction_type = interaction;
        m_objective.data = data;
        return *this;
    }

    ObjectiveBuilder& kill(const std::string& enemy_type, int32_t count) {
        m_objective.type = ObjectiveType::Kill;
        KillData data;
        data.enemy_type = enemy_type;
        data.target = count;
        m_objective.data = data;
        return *this;
    }

    ObjectiveBuilder& timer(float seconds, bool fail_on_timeout = true) {
        m_objective.type = ObjectiveType::Timer;
        TimerData data;
        data.time_limit = seconds;
        data.fail_on_timeout = fail_on_timeout;
        m_objective.data = data;
        return *this;
    }

    ObjectiveBuilder& escort(scene::Entity target, const Vec3& dest, float dest_radius = 5.0f) {
        m_objective.type = ObjectiveType::Escort;
        EscortData data;
        data.escort_target = target;
        data.destination = dest;
        data.destination_radius = dest_radius;
        m_objective.data = data;
        m_objective.waypoint_position = dest;
        return *this;
    }

    ObjectiveBuilder& custom(ObjectiveCondition condition) {
        m_objective.type = ObjectiveType::Custom;
        m_objective.custom_condition = std::move(condition);
        return *this;
    }

    ObjectiveBuilder& optional(bool value = true) {
        m_objective.is_optional = value;
        return *this;
    }

    ObjectiveBuilder& hidden(bool value = true) {
        m_objective.is_hidden = value;
        return *this;
    }

    ObjectiveBuilder& no_waypoint() {
        m_objective.show_waypoint = false;
        return *this;
    }

    ObjectiveBuilder& no_hud() {
        m_objective.show_in_hud = false;
        return *this;
    }

    ObjectiveBuilder& order(int32_t o) {
        m_objective.display_order = o;
        return *this;
    }

    Objective build() { return std::move(m_objective); }

private:
    Objective m_objective;
};

inline ObjectiveBuilder make_objective(const std::string& id) {
    return ObjectiveBuilder(id);
}

} // namespace engine::quest
