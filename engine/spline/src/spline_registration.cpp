#include <engine/reflect/reflect.hpp>
#include <engine/spline/spline.hpp>
#include <engine/spline/spline_component.hpp>
#include <engine/spline/spline_follower.hpp>

// This file registers all spline components with the reflection system.
// Components are automatically registered at static initialization time.

namespace {

using namespace engine::spline;
using namespace engine::reflect;

// Register SplineMode enum
struct SplineModeRegistrar {
    SplineModeRegistrar() {
        TypeRegistry::instance().register_enum<SplineMode>("SplineMode", {
            {SplineMode::Linear, "Linear"},
            {SplineMode::Bezier, "Bezier"},
            {SplineMode::CatmullRom, "CatmullRom"},
            {SplineMode::BSpline, "BSpline"}
        });
    }
};
static SplineModeRegistrar _splinemode_registrar;

// Register SplineEndMode enum
struct SplineEndModeRegistrar {
    SplineEndModeRegistrar() {
        TypeRegistry::instance().register_enum<SplineEndMode>("SplineEndMode", {
            {SplineEndMode::Clamp, "Clamp"},
            {SplineEndMode::Loop, "Loop"},
            {SplineEndMode::PingPong, "PingPong"}
        });
    }
};
static SplineEndModeRegistrar _splineendmode_registrar;

// Register FollowMode enum
struct FollowModeRegistrar {
    FollowModeRegistrar() {
        TypeRegistry::instance().register_enum<FollowMode>("FollowMode", {
            {FollowMode::Distance, "Distance"},
            {FollowMode::Parameter, "Parameter"},
            {FollowMode::Time, "Time"}
        });
    }
};
static FollowModeRegistrar _followmode_registrar;

// Register FollowEndBehavior enum
struct FollowEndBehaviorRegistrar {
    FollowEndBehaviorRegistrar() {
        TypeRegistry::instance().register_enum<FollowEndBehavior>("FollowEndBehavior", {
            {FollowEndBehavior::Stop, "Stop"},
            {FollowEndBehavior::Loop, "Loop"},
            {FollowEndBehavior::PingPong, "PingPong"},
            {FollowEndBehavior::Destroy, "Destroy"},
            {FollowEndBehavior::Custom, "Custom"}
        });
    }
};
static FollowEndBehaviorRegistrar _followendbehavior_registrar;

// Register FollowOrientation enum
struct FollowOrientationRegistrar {
    FollowOrientationRegistrar() {
        TypeRegistry::instance().register_enum<FollowOrientation>("FollowOrientation", {
            {FollowOrientation::None, "None"},
            {FollowOrientation::FollowTangent, "FollowTangent"},
            {FollowOrientation::FollowPath, "FollowPath"},
            {FollowOrientation::LookAt, "LookAt"},
            {FollowOrientation::Custom, "Custom"}
        });
    }
};
static FollowOrientationRegistrar _followorientation_registrar;

// Register SplineComponent
struct SplineComponentRegistrar {
    SplineComponentRegistrar() {
        TypeRegistry::instance().register_component<SplineComponent>("SplineComponent",
            TypeMeta().set_display_name("Spline").set_description("Defines a spline curve for paths, rails, and procedural geometry"));

        TypeRegistry::instance().register_property<SplineComponent, &SplineComponent::mode>("mode",
            PropertyMeta().set_display_name("Mode"));

        TypeRegistry::instance().register_property<SplineComponent, &SplineComponent::end_mode>("end_mode",
            PropertyMeta().set_display_name("End Mode"));

        TypeRegistry::instance().register_property<SplineComponent, &SplineComponent::catmull_rom_alpha>("catmull_rom_alpha",
            PropertyMeta().set_display_name("Catmull-Rom Alpha").set_range(0.0f, 1.0f));

        TypeRegistry::instance().register_property<SplineComponent, &SplineComponent::auto_tangents>("auto_tangents",
            PropertyMeta().set_display_name("Auto Tangents"));

        TypeRegistry::instance().register_property<SplineComponent, &SplineComponent::tension>("tension",
            PropertyMeta().set_display_name("Tension").set_range(0.0f, 1.0f));

        TypeRegistry::instance().register_property<SplineComponent, &SplineComponent::visible>("visible",
            PropertyMeta().set_display_name("Visible"));

        TypeRegistry::instance().register_property<SplineComponent, &SplineComponent::show_points>("show_points",
            PropertyMeta().set_display_name("Show Points"));

        TypeRegistry::instance().register_property<SplineComponent, &SplineComponent::show_tangents>("show_tangents",
            PropertyMeta().set_display_name("Show Tangents"));

        TypeRegistry::instance().register_property<SplineComponent, &SplineComponent::color>("color",
            PropertyMeta().set_display_name("Color"));

        TypeRegistry::instance().register_property<SplineComponent, &SplineComponent::line_width>("line_width",
            PropertyMeta().set_display_name("Line Width").set_range(1.0f, 10.0f));

        TypeRegistry::instance().register_property<SplineComponent, &SplineComponent::tessellation>("tessellation",
            PropertyMeta().set_display_name("Tessellation").set_range(1.0f, 100.0f));
    }
};
static SplineComponentRegistrar _spline_registrar;

// Register SplineDebugRenderComponent
struct SplineDebugRenderComponentRegistrar {
    SplineDebugRenderComponentRegistrar() {
        TypeRegistry::instance().register_component<SplineDebugRenderComponent>("SplineDebugRenderComponent",
            TypeMeta().set_display_name("Spline Debug Render").set_description("Debug visualization settings for a spline"));

        TypeRegistry::instance().register_property<SplineDebugRenderComponent, &SplineDebugRenderComponent::enabled>("enabled",
            PropertyMeta().set_display_name("Enabled"));

        TypeRegistry::instance().register_property<SplineDebugRenderComponent, &SplineDebugRenderComponent::render_curve>("render_curve",
            PropertyMeta().set_display_name("Render Curve"));

        TypeRegistry::instance().register_property<SplineDebugRenderComponent, &SplineDebugRenderComponent::render_points>("render_points",
            PropertyMeta().set_display_name("Render Points"));

        TypeRegistry::instance().register_property<SplineDebugRenderComponent, &SplineDebugRenderComponent::render_tangents>("render_tangents",
            PropertyMeta().set_display_name("Render Tangents"));

        TypeRegistry::instance().register_property<SplineDebugRenderComponent, &SplineDebugRenderComponent::render_normals>("render_normals",
            PropertyMeta().set_display_name("Render Normals"));

        TypeRegistry::instance().register_property<SplineDebugRenderComponent, &SplineDebugRenderComponent::render_bounds>("render_bounds",
            PropertyMeta().set_display_name("Render Bounds"));

        TypeRegistry::instance().register_property<SplineDebugRenderComponent, &SplineDebugRenderComponent::curve_color>("curve_color",
            PropertyMeta().set_display_name("Curve Color"));

        TypeRegistry::instance().register_property<SplineDebugRenderComponent, &SplineDebugRenderComponent::point_color>("point_color",
            PropertyMeta().set_display_name("Point Color"));

        TypeRegistry::instance().register_property<SplineDebugRenderComponent, &SplineDebugRenderComponent::point_size>("point_size",
            PropertyMeta().set_display_name("Point Size").set_range(1.0f, 20.0f));
    }
};
static SplineDebugRenderComponentRegistrar _splinedebugrender_registrar;

// Register SplineFollowerComponent
struct SplineFollowerComponentRegistrar {
    SplineFollowerComponentRegistrar() {
        TypeRegistry::instance().register_component<SplineFollowerComponent>("SplineFollowerComponent",
            TypeMeta().set_display_name("Spline Follower").set_description("Makes an entity follow a spline path"));

        TypeRegistry::instance().register_property<SplineFollowerComponent, &SplineFollowerComponent::current_distance>("current_distance",
            PropertyMeta().set_display_name("Current Distance").set_read_only(true));

        TypeRegistry::instance().register_property<SplineFollowerComponent, &SplineFollowerComponent::current_t>("current_t",
            PropertyMeta().set_display_name("Current T").set_read_only(true));

        TypeRegistry::instance().register_property<SplineFollowerComponent, &SplineFollowerComponent::is_moving>("is_moving",
            PropertyMeta().set_display_name("Is Moving"));

        TypeRegistry::instance().register_property<SplineFollowerComponent, &SplineFollowerComponent::is_reversed>("is_reversed",
            PropertyMeta().set_display_name("Is Reversed"));

        TypeRegistry::instance().register_property<SplineFollowerComponent, &SplineFollowerComponent::follow_mode>("follow_mode",
            PropertyMeta().set_display_name("Follow Mode"));

        TypeRegistry::instance().register_property<SplineFollowerComponent, &SplineFollowerComponent::speed>("speed",
            PropertyMeta().set_display_name("Speed").set_range(0.0f, 100.0f));

        TypeRegistry::instance().register_property<SplineFollowerComponent, &SplineFollowerComponent::duration>("duration",
            PropertyMeta().set_display_name("Duration").set_range(0.1f, 300.0f));

        TypeRegistry::instance().register_property<SplineFollowerComponent, &SplineFollowerComponent::end_behavior>("end_behavior",
            PropertyMeta().set_display_name("End Behavior"));

        TypeRegistry::instance().register_property<SplineFollowerComponent, &SplineFollowerComponent::max_loops>("max_loops",
            PropertyMeta().set_display_name("Max Loops"));

        TypeRegistry::instance().register_property<SplineFollowerComponent, &SplineFollowerComponent::orientation>("orientation",
            PropertyMeta().set_display_name("Orientation"));

        TypeRegistry::instance().register_property<SplineFollowerComponent, &SplineFollowerComponent::up_vector>("up_vector",
            PropertyMeta().set_display_name("Up Vector"));

        TypeRegistry::instance().register_property<SplineFollowerComponent, &SplineFollowerComponent::rotation_smoothing>("rotation_smoothing",
            PropertyMeta().set_display_name("Rotation Smoothing").set_range(0.0f, 20.0f));

        TypeRegistry::instance().register_property<SplineFollowerComponent, &SplineFollowerComponent::offset>("offset",
            PropertyMeta().set_display_name("Offset"));

        TypeRegistry::instance().register_property<SplineFollowerComponent, &SplineFollowerComponent::offset_in_spline_space>("offset_in_spline_space",
            PropertyMeta().set_display_name("Offset In Spline Space"));
    }
};
static SplineFollowerComponentRegistrar _splinefollower_registrar;

// Register SplineAttachmentComponent
struct SplineAttachmentComponentRegistrar {
    SplineAttachmentComponentRegistrar() {
        TypeRegistry::instance().register_component<SplineAttachmentComponent>("SplineAttachmentComponent",
            TypeMeta().set_display_name("Spline Attachment").set_description("Attaches an entity to a fixed point on a spline"));

        TypeRegistry::instance().register_property<SplineAttachmentComponent, &SplineAttachmentComponent::t>("t",
            PropertyMeta().set_display_name("T").set_range(0.0f, 1.0f));

        TypeRegistry::instance().register_property<SplineAttachmentComponent, &SplineAttachmentComponent::distance>("distance",
            PropertyMeta().set_display_name("Distance").set_range(0.0f, 10000.0f));

        TypeRegistry::instance().register_property<SplineAttachmentComponent, &SplineAttachmentComponent::use_distance>("use_distance",
            PropertyMeta().set_display_name("Use Distance"));

        TypeRegistry::instance().register_property<SplineAttachmentComponent, &SplineAttachmentComponent::offset>("offset",
            PropertyMeta().set_display_name("Offset"));

        TypeRegistry::instance().register_property<SplineAttachmentComponent, &SplineAttachmentComponent::offset_in_spline_space>("offset_in_spline_space",
            PropertyMeta().set_display_name("Offset In Spline Space"));

        TypeRegistry::instance().register_property<SplineAttachmentComponent, &SplineAttachmentComponent::match_rotation>("match_rotation",
            PropertyMeta().set_display_name("Match Rotation"));

        TypeRegistry::instance().register_property<SplineAttachmentComponent, &SplineAttachmentComponent::rotation_offset>("rotation_offset",
            PropertyMeta().set_display_name("Rotation Offset"));
    }
};
static SplineAttachmentComponentRegistrar _splineattachment_registrar;

// Register SplineMeshComponent
struct SplineMeshComponentRegistrar {
    SplineMeshComponentRegistrar() {
        TypeRegistry::instance().register_component<SplineMeshComponent>("SplineMeshComponent",
            TypeMeta().set_display_name("Spline Mesh").set_description("Generates a mesh along a spline"));

        TypeRegistry::instance().register_property<SplineMeshComponent, &SplineMeshComponent::radius>("radius",
            PropertyMeta().set_display_name("Radius").set_range(0.01f, 100.0f));

        TypeRegistry::instance().register_property<SplineMeshComponent, &SplineMeshComponent::radial_segments>("radial_segments",
            PropertyMeta().set_display_name("Radial Segments").set_range(3.0f, 64.0f));

        TypeRegistry::instance().register_property<SplineMeshComponent, &SplineMeshComponent::rect_size>("rect_size",
            PropertyMeta().set_display_name("Rect Size"));

        TypeRegistry::instance().register_property<SplineMeshComponent, &SplineMeshComponent::segments_per_unit>("segments_per_unit",
            PropertyMeta().set_display_name("Segments Per Unit").set_range(1.0f, 20.0f));

        TypeRegistry::instance().register_property<SplineMeshComponent, &SplineMeshComponent::cap_start>("cap_start",
            PropertyMeta().set_display_name("Cap Start"));

        TypeRegistry::instance().register_property<SplineMeshComponent, &SplineMeshComponent::cap_end>("cap_end",
            PropertyMeta().set_display_name("Cap End"));

        TypeRegistry::instance().register_property<SplineMeshComponent, &SplineMeshComponent::follow_spline_roll>("follow_spline_roll",
            PropertyMeta().set_display_name("Follow Spline Roll"));

        TypeRegistry::instance().register_property<SplineMeshComponent, &SplineMeshComponent::uv_scale_u>("uv_scale_u",
            PropertyMeta().set_display_name("UV Scale U").set_range(0.01f, 100.0f));

        TypeRegistry::instance().register_property<SplineMeshComponent, &SplineMeshComponent::uv_scale_v>("uv_scale_v",
            PropertyMeta().set_display_name("UV Scale V").set_range(0.01f, 100.0f));
    }
};
static SplineMeshComponentRegistrar _splinemesh_registrar;

} // anonymous namespace
