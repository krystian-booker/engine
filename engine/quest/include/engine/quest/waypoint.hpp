#pragma once

#include <engine/core/math.hpp>
#include <cstdint>
#include <engine/scene/entity.hpp>
#include <string>
#include <optional>
#include <vector>

// Forward declaration
namespace engine::scene { class World; }

namespace engine::quest {

using core::Vec3;
using core::Vec4;

// ============================================================================
// Waypoint Type
// ============================================================================

enum class WaypointType {
    Objective,          // Quest objective marker
    QuestGiver,         // NPC with available quest
    QuestTurnIn,        // Quest turn-in location
    PointOfInterest,    // General POI
    Custom              // Custom marker
};

// ============================================================================
// Waypoint Priority
// ============================================================================

enum class WaypointPriority {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

// ============================================================================
// Waypoint Component
// ============================================================================

struct WaypointComponent {
    WaypointType type = WaypointType::PointOfInterest;
    WaypointPriority priority = WaypointPriority::Normal;

    // Display
    std::string icon;               // Icon to show
    std::string label;              // Text label
    Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};  // Marker color

    // Position override (if not using entity position)
    std::optional<Vec3> position_override;
    float height_offset = 2.0f;     // Height above entity/position

    // Visibility
    float max_distance = 0.0f;      // 0 = always visible
    float min_distance = 0.0f;      // Don't show when closer
    bool show_distance = true;
    bool show_on_screen_edge = true; // Show at screen edge when off-screen
    bool clamp_to_screen = true;

    // Animation
    bool animate = true;
    float pulse_speed = 1.0f;
    float bob_amount = 0.2f;

    // Quest linking
    std::string quest_id;
    std::string objective_id;

    // State
    bool enabled = true;
    bool visible = true;
};

// ============================================================================
// Quest Trigger Component
// ============================================================================

struct QuestTriggerComponent {
    enum class TriggerType {
        StartQuest,         // Start a quest when entered
        CompleteObjective,  // Complete objective when entered
        FailObjective,      // Fail objective when entered
        Custom              // Custom callback
    };

    TriggerType type = TriggerType::CompleteObjective;

    std::string quest_id;
    std::string objective_id;

    // Trigger shape
    enum class Shape {
        Sphere,
        Box
    };
    Shape shape = Shape::Sphere;
    float radius = 5.0f;            // For sphere
    Vec3 extents{5.0f, 5.0f, 5.0f}; // For box (half-extents)

    // Activation
    bool require_player = true;     // Only player triggers
    bool one_shot = true;           // Trigger only once
    bool triggered = false;         // Has been triggered

    // Requirements
    std::string required_flag;      // Optional flag requirement
    std::string required_item;      // Optional item requirement

    // Feedback
    bool show_feedback = true;
    std::string feedback_message;
};

// ============================================================================
// Quest Giver Component
// ============================================================================

struct QuestGiverComponent {
    std::vector<std::string> available_quests;  // Quest IDs this NPC can give
    std::vector<std::string> turn_in_quests;    // Quest IDs this NPC accepts

    // Display
    std::string npc_name;
    bool show_indicator = true;     // Show quest indicator above head

    // Indicator states
    std::string available_icon;     // Icon when has available quest
    std::string active_icon;        // Icon when has active quest
    std::string complete_icon;      // Icon when quest ready to turn in

    // Interaction
    float interaction_range = 3.0f;
    bool face_player = true;
};

// ============================================================================
// Waypoint System
// ============================================================================

class WaypointSystem {
public:
    static WaypointSystem& instance();

    void update(scene::World& world, float dt);

    // Create waypoints
    scene::Entity create_waypoint(scene::World& world, const Vec3& position,
                                   WaypointType type, const std::string& label = "");

    scene::Entity create_objective_waypoint(scene::World& world,
                                             const std::string& quest_id,
                                             const std::string& objective_id,
                                             const Vec3& position);

    scene::Entity create_objective_waypoint(scene::World& world,
                                             const std::string& quest_id,
                                             const std::string& objective_id,
                                             scene::Entity target);

    // Waypoint queries
    std::vector<scene::Entity> get_visible_waypoints(scene::World& world,
                                                      const Vec3& camera_pos) const;

    scene::Entity get_closest_waypoint(scene::World& world,
                                        const Vec3& position,
                                        WaypointType type = WaypointType::Objective) const;

    // Update waypoints for quest state
    void update_quest_waypoints(scene::World& world, const std::string& quest_id);
    void remove_quest_waypoints(scene::World& world, const std::string& quest_id);

    // Player tracking
    void set_player_entity(scene::Entity player) { m_player = player; }
    void set_camera_position(const Vec3& pos) { m_camera_position = pos; }

private:
    WaypointSystem() = default;

    scene::Entity m_player = scene::NullEntity;
    Vec3 m_camera_position{0.0f};
    float m_animation_time = 0.0f;
};

inline WaypointSystem& waypoints() { return WaypointSystem::instance(); }

} // namespace engine::quest
