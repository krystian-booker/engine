#pragma once

#include <engine/spline/spline.hpp>
#include <engine/scene/entity.hpp>
#include <functional>
#include <string>

namespace engine::scene { class World; }

namespace engine::spline {

// Movement mode for following a spline
enum class FollowMode : uint8_t {
    Distance,       // Move by distance along spline (constant speed)
    Parameter,      // Move by parameter t (variable speed based on curve)
    Time            // Complete spline in set time (normalized)
};

// What to do when reaching the end of the spline
enum class FollowEndBehavior : uint8_t {
    Stop,           // Stop at the end
    Loop,           // Jump back to start
    PingPong,       // Reverse direction
    Destroy,        // Destroy the entity
    Custom          // Call custom callback
};

// How the entity orients itself on the spline
enum class FollowOrientation : uint8_t {
    None,           // Don't change orientation
    FollowTangent,  // Face forward along spline
    FollowPath,     // Face forward with up vector from spline normal
    LookAt,         // Look at a target entity/position
    Custom          // Use custom rotation callback
};

// Callback types
using SplineReachedEndCallback = std::function<void(scene::Entity entity)>;
using SplineOrientationCallback = std::function<Quat(const SplineEvalResult& eval)>;
using SplinePositionCallback = std::function<Vec3(const SplineEvalResult& eval, const Vec3& base_position)>;

// Component for entities that follow a spline
struct SplineFollowerComponent {
    // Reference to spline entity
    scene::Entity spline_entity;    // Entity with SplineComponent

    // Current state
    float current_distance = 0.0f;  // Distance along spline
    float current_t = 0.0f;         // Parameter t [0, 1]
    bool is_moving = true;
    bool is_reversed = false;       // Moving backwards

    // Movement settings
    FollowMode follow_mode = FollowMode::Distance;
    float speed = 5.0f;             // Units per second (Distance mode)
    float duration = 5.0f;          // Total time to traverse (Time mode)
    float parameter_speed = 0.2f;   // t change per second (Parameter mode)

    // End behavior
    FollowEndBehavior end_behavior = FollowEndBehavior::Stop;
    int max_loops = -1;             // -1 = infinite
    int current_loop = 0;

    // Orientation
    FollowOrientation orientation = FollowOrientation::FollowTangent;
    Vec3 up_vector{0.0f, 1.0f, 0.0f};   // Reference up for FollowTangent
    scene::Entity look_at_entity;        // Target for LookAt mode
    Vec3 look_at_offset{0.0f};          // Offset from look-at target
    float rotation_smoothing = 0.0f;     // 0 = instant, higher = smoother

    // Position offset from spline
    Vec3 offset{0.0f};              // Local offset from spline position
    bool offset_in_spline_space = true; // Offset relative to spline tangent frame

    // Easing
    enum class EaseType : uint8_t {
        None,
        EaseIn,
        EaseOut,
        EaseInOut,
        Custom
    };
    EaseType ease_in = EaseType::None;
    EaseType ease_out = EaseType::None;
    float ease_distance = 1.0f;     // Distance over which to ease

    // Events
    bool fire_started_event = true;
    bool fire_ended_event = true;
    bool fire_loop_event = true;
    std::string started_event_name = "spline_started";
    std::string ended_event_name = "spline_ended";
    std::string loop_event_name = "spline_loop";

    // Runtime state (not serialized)
    bool has_started = false;
    Quat target_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Quat current_rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

// Attach an entity to a point on a spline (doesn't move, just positioned)
struct SplineAttachmentComponent {
    scene::Entity spline_entity;
    float t = 0.0f;                 // Position on spline [0, 1]
    float distance = 0.0f;          // Alternative: position by distance
    bool use_distance = false;      // Use distance instead of t

    Vec3 offset{0.0f};
    bool offset_in_spline_space = true;

    bool match_rotation = true;     // Match spline tangent rotation
    Vec3 rotation_offset{0.0f};     // Additional rotation (euler angles)
};

// Control spline follower playback
struct SplineFollowerController {
    // Play/pause control
    static void play(SplineFollowerComponent& follower);
    static void pause(SplineFollowerComponent& follower);
    static void stop(SplineFollowerComponent& follower);  // Reset to start
    static void toggle(SplineFollowerComponent& follower);

    // Position control
    static void set_position(SplineFollowerComponent& follower, float t);
    static void set_distance(SplineFollowerComponent& follower, float distance);
    static void jump_to_start(SplineFollowerComponent& follower);
    static void jump_to_end(SplineFollowerComponent& follower);

    // Direction control
    static void reverse(SplineFollowerComponent& follower);
    static void set_reversed(SplineFollowerComponent& follower, bool reversed);

    // Speed control
    static void set_speed(SplineFollowerComponent& follower, float speed);
    static void multiply_speed(SplineFollowerComponent& follower, float multiplier);
};

// Systems

// Updates SplineFollowerComponent - moves entities along their splines
void spline_follower_system(engine::scene::World& world, double dt);

// Updates SplineAttachmentComponent - positions attached entities
void spline_attachment_system(engine::scene::World& world, double dt);

// Utility: Create a follower that moves an entity along a spline
void setup_spline_follower(engine::scene::World& world, scene::Entity follower_entity,
                           scene::Entity spline_entity, float speed = 5.0f,
                           FollowEndBehavior end_behavior = FollowEndBehavior::Stop);

// Utility: Attach an entity to a fixed point on a spline
void attach_to_spline(engine::scene::World& world, scene::Entity entity,
                      scene::Entity spline_entity, float t);

} // namespace engine::spline
