#pragma once

#include <engine/spline/spline.hpp>
#include <engine/spline/bezier_spline.hpp>
#include <engine/spline/catmull_rom.hpp>
#include <memory>
#include <string>

namespace engine::scene { class World; }

namespace engine::spline {

// ECS Component that holds a spline
struct SplineComponent {
    // Spline data
    SplineMode mode = SplineMode::CatmullRom;
    SplineEndMode end_mode = SplineEndMode::Clamp;
    std::vector<SplinePoint> points;

    // Catmull-Rom specific
    float catmull_rom_alpha = 0.5f;

    // Bezier specific
    bool auto_tangents = true;
    float tension = 0.5f;

    // Display settings
    bool visible = true;
    bool show_points = true;
    bool show_tangents = false;
    Vec4 color{1.0f, 0.5f, 0.0f, 1.0f};  // Orange by default
    float line_width = 2.0f;
    int tessellation = 20;  // Points per segment for rendering

    // Runtime (not serialized)
    mutable std::unique_ptr<Spline> runtime_spline;

    // Get or create the runtime spline object
    Spline* get_spline() const;

    // Mark spline as needing rebuild
    void invalidate() const { runtime_spline.reset(); }

    // Quick evaluation helpers
    Vec3 evaluate_position(float t) const;
    SplineEvalResult evaluate(float t) const;
    float get_length() const;
};

// Render a spline for debug visualization
struct SplineDebugRenderComponent {
    bool enabled = true;
    bool render_curve = true;
    bool render_points = true;
    bool render_tangents = false;
    bool render_normals = false;
    bool render_bounds = false;

    Vec4 curve_color{1.0f, 0.5f, 0.0f, 1.0f};
    Vec4 point_color{1.0f, 1.0f, 0.0f, 1.0f};
    Vec4 tangent_color{0.0f, 1.0f, 0.0f, 1.0f};
    Vec4 normal_color{0.0f, 0.5f, 1.0f, 1.0f};

    float point_size = 5.0f;
    float tangent_scale = 1.0f;
};

// Trigger events when an entity follows a spline
struct SplineEventComponent {
    // Distance-based events
    struct DistanceEvent {
        float distance = 0.0f;          // Distance along spline
        std::string event_name;          // Event to fire
        bool triggered = false;          // Runtime: has been triggered
        bool repeat_on_loop = true;      // Re-trigger when looping
    };
    std::vector<DistanceEvent> distance_events;

    // Point-based events (trigger when passing a control point)
    struct PointEvent {
        size_t point_index = 0;
        std::string event_name;
        bool triggered = false;
    };
    std::vector<PointEvent> point_events;

    // Reset triggered flags (call when looping)
    void reset_triggers();
};

// Spline mesh generator - creates geometry along a spline
struct SplineMeshComponent {
    // Profile shape to extrude along spline
    enum class ProfileType : uint8_t {
        Circle,     // Tube/pipe
        Rectangle,  // Rectangular beam
        Custom      // Custom 2D profile
    };

    ProfileType profile_type = ProfileType::Circle;

    // Circle profile
    float radius = 0.5f;
    int radial_segments = 8;

    // Rectangle profile
    Vec2 rect_size{1.0f, 0.5f};

    // Custom profile (2D points in XY plane)
    std::vector<Vec2> custom_profile;

    // Mesh generation settings
    int segments_per_unit = 2;      // Segments per unit length
    bool cap_start = true;
    bool cap_end = true;
    bool follow_spline_roll = true;
    float uv_scale_u = 1.0f;        // UV tiling along length
    float uv_scale_v = 1.0f;        // UV tiling around circumference

    // Scale along spline
    bool use_scale_curve = false;
    std::vector<float> scale_curve; // Scale multiplier at each control point
};

// Systems

// Debug draw splines in the scene
void spline_debug_draw_system(engine::scene::World& world, double dt);

// Update spline meshes when spline changes
void spline_mesh_system(engine::scene::World& world, double dt);

} // namespace engine::spline
