#pragma once

#include <engine/scene/entity.hpp>
#include <engine/core/math.hpp>
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <cstdint>

namespace engine::scene {

using engine::core::Vec3;

class World;

// Type of interaction (hints for UI and game logic)
enum class InteractionType : uint8_t {
    Generic,        // General interaction
    Pickup,         // Item collection
    Door,           // Open/close doors
    Lever,          // Pull/push switches
    Talk,           // NPC dialogue
    Examine,        // Look at/read objects
    Use,            // Use an object
    Climb,          // Climbing point
    Vehicle,        // Enter vehicle
    Custom          // Game-defined interaction
};

// Marks an entity as interactable
struct InteractableComponent {
    std::string interaction_id;             // Unique identifier for game logic
    std::string display_name;               // UI display text ("Open Door", "Pick Up Sword")
    std::string interaction_prompt = "E";   // Key/button hint shown to player

    InteractionType type = InteractionType::Generic;

    float interaction_radius = 2.0f;        // Detection distance in world units
    float interaction_angle = 180.0f;       // Cone in front of interactor (degrees, 360 = all around)
    bool requires_line_of_sight = true;     // Raycast check for visibility
    bool one_shot = false;                  // Disable after first interaction
    bool enabled = true;                    // Can be disabled at runtime

    // Hold-to-interact settings
    bool hold_to_interact = false;
    float hold_duration = 0.0f;             // Seconds to hold

    // Priority for selecting between multiple interactables
    int priority = 0;                       // Higher = preferred when multiple in range

    // Interaction point offset from entity origin
    Vec3 interaction_offset{0.0f};          // Local space offset for interaction point
};

// Optional component for visual feedback when in range
struct InteractionHighlightComponent {
    bool highlight_enabled = true;
    Vec3 outline_color{1.0f, 0.9f, 0.2f};   // Yellow outline
    float outline_width = 2.0f;
    bool show_prompt = true;                // Show interaction prompt UI
    bool pulse_effect = true;               // Subtle pulsing animation
};

// Interaction detection result
struct InteractionCandidate {
    Entity entity = NullEntity;
    float distance = 0.0f;                  // Distance to interactor
    float angle = 0.0f;                     // Angle from interactor forward
    float dot_product = 0.0f;               // Facing alignment (-1 to 1)
    bool in_line_of_sight = true;           // Passed raycast check
    float score = 0.0f;                     // Combined ranking score

    // Interactable data (copied for convenience)
    std::string interaction_id;
    std::string display_name;
    InteractionType type = InteractionType::Generic;
    bool hold_to_interact = false;
    float hold_duration = 0.0f;
};

// Hold interaction progress tracking
struct InteractionHoldState {
    Entity target = NullEntity;
    float hold_time = 0.0f;
    bool holding = false;
};

// Callbacks for interaction events
using InteractionCallback = std::function<void(Entity interactor, Entity target, const std::string& interaction_id)>;

// Interaction query and management system
class InteractionSystem {
public:
    static InteractionSystem& instance();

    // Delete copy/move
    InteractionSystem(const InteractionSystem&) = delete;
    InteractionSystem& operator=(const InteractionSystem&) = delete;

    // Find the best interactable near a position
    std::optional<InteractionCandidate> find_best_interactable(
        World& world,
        const Vec3& position,
        const Vec3& forward,
        float max_distance = 3.0f
    );

    // Find all interactables in range (sorted by score)
    std::vector<InteractionCandidate> find_all_interactables(
        World& world,
        const Vec3& position,
        const Vec3& forward,
        float max_distance = 5.0f
    );

    // Check if a specific entity can be interacted with
    std::optional<InteractionCandidate> can_interact_with(
        World& world,
        Entity target,
        const Vec3& position,
        const Vec3& forward
    );

    // Hold interaction management
    void begin_hold(Entity interactor, Entity target);
    bool update_hold(float dt);  // Returns true when hold completes
    void cancel_hold();
    const InteractionHoldState& get_hold_state() const { return m_hold_state; }
    float get_hold_progress() const;  // 0.0 to 1.0

    // Perform an interaction
    void interact(World& world, Entity interactor, Entity target);

    // Callbacks
    void set_on_interaction(InteractionCallback callback);
    void set_line_of_sight_check(std::function<bool(World&, const Vec3&, const Vec3&)> check);

    // Configuration
    void set_default_max_distance(float distance) { m_default_max_distance = distance; }
    float get_default_max_distance() const { return m_default_max_distance; }

private:
    InteractionSystem();
    ~InteractionSystem() = default;

    InteractionCandidate evaluate_interactable(
        World& world,
        Entity entity,
        const InteractableComponent& interactable,
        const Vec3& position,
        const Vec3& forward,
        float max_distance
    );

    float calculate_score(const InteractionCandidate& candidate, const InteractableComponent& interactable);
    bool default_line_of_sight_check(World& world, const Vec3& from, const Vec3& to);

    InteractionCallback m_on_interaction;
    std::function<bool(World&, const Vec3&, const Vec3&)> m_line_of_sight_check;
    InteractionHoldState m_hold_state;
    float m_default_max_distance = 3.0f;
};

// Convenience function
inline InteractionSystem& interactions() {
    return InteractionSystem::instance();
}

// ECS system function for highlighting interactables
void interaction_highlight_system(World& world, double dt);

} // namespace engine::scene
