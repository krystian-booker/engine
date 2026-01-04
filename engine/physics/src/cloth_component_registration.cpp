#include <engine/physics/cloth_component.hpp>
#include <engine/physics/cloth.hpp>
#include <engine/reflect/reflect.hpp>

namespace {

using namespace engine::physics;
using namespace engine::reflect;

// ClothComponent registration
struct ClothComponentRegistrar {
    ClothComponentRegistrar() {
        TypeRegistry::instance().register_component<ClothComponent>(
            "ClothComponent",
            TypeMeta()
                .set_display_name("Cloth")
                .set_description("Cloth/soft body physics configuration"));

        TypeRegistry::instance().register_property<ClothComponent,
            &ClothComponent::type>(
            "type",
            PropertyMeta()
                .set_display_name("Type")
                .set_category("General"));

        TypeRegistry::instance().register_property<ClothComponent,
            &ClothComponent::mass>(
            "mass",
            PropertyMeta()
                .set_display_name("Mass")
                .set_category("Physics")
                .set_range(0.01f, 100.0f));

        TypeRegistry::instance().register_property<ClothComponent,
            &ClothComponent::edge_stiffness>(
            "edge_stiffness",
            PropertyMeta()
                .set_display_name("Edge Stiffness")
                .set_category("Physics")
                .set_range(0.0f, 1.0f));

        TypeRegistry::instance().register_property<ClothComponent,
            &ClothComponent::bend_stiffness>(
            "bend_stiffness",
            PropertyMeta()
                .set_display_name("Bend Stiffness")
                .set_category("Physics")
                .set_range(0.0f, 1.0f));

        TypeRegistry::instance().register_property<ClothComponent,
            &ClothComponent::shear_stiffness>(
            "shear_stiffness",
            PropertyMeta()
                .set_display_name("Shear Stiffness")
                .set_category("Physics")
                .set_range(0.0f, 1.0f));

        TypeRegistry::instance().register_property<ClothComponent,
            &ClothComponent::damping>(
            "damping",
            PropertyMeta()
                .set_display_name("Damping")
                .set_category("Physics")
                .set_range(0.0f, 1.0f));

        TypeRegistry::instance().register_property<ClothComponent,
            &ClothComponent::solver_iterations>(
            "solver_iterations",
            PropertyMeta()
                .set_display_name("Solver Iterations")
                .set_category("Solver")
                .set_range(1, 16));

        TypeRegistry::instance().register_property<ClothComponent,
            &ClothComponent::wind_mode>(
            "wind_mode",
            PropertyMeta()
                .set_display_name("Wind Mode")
                .set_category("Wind"));

        TypeRegistry::instance().register_property<ClothComponent,
            &ClothComponent::visual_update_rate>(
            "visual_update_rate",
            PropertyMeta()
                .set_display_name("Visual Update Rate")
                .set_category("Optimization")
                .set_range(15.0f, 120.0f));

        TypeRegistry::instance().register_property<ClothComponent,
            &ClothComponent::visual_max_distance>(
            "visual_max_distance",
            PropertyMeta()
                .set_display_name("Visual Max Distance")
                .set_category("Optimization")
                .set_range(10.0f, 500.0f));

        TypeRegistry::instance().register_property<ClothComponent,
            &ClothComponent::use_gravity>(
            "use_gravity",
            PropertyMeta()
                .set_display_name("Use Gravity")
                .set_category("Physics"));

        TypeRegistry::instance().register_property<ClothComponent,
            &ClothComponent::custom_gravity>(
            "custom_gravity",
            PropertyMeta()
                .set_display_name("Custom Gravity")
                .set_category("Physics"));

        TypeRegistry::instance().register_property<ClothComponent,
            &ClothComponent::sleep_threshold>(
            "sleep_threshold",
            PropertyMeta()
                .set_display_name("Sleep Threshold")
                .set_category("Optimization")
                .set_range(0.0f, 1.0f));
    }
};
static ClothComponentRegistrar _cloth_component_registrar;

// ClothGridSettings registration
struct ClothGridSettingsRegistrar {
    ClothGridSettingsRegistrar() {
        TypeRegistry::instance().register_type<ClothGridSettings>(
            "ClothGridSettings",
            TypeMeta()
                .set_display_name("Cloth Grid Settings")
                .set_description("Procedural cloth grid generation parameters"));

        TypeRegistry::instance().register_property<ClothGridSettings,
            &ClothGridSettings::width_segments>(
            "width_segments",
            PropertyMeta()
                .set_display_name("Width Segments")
                .set_category("Grid")
                .set_range(1u, 100u));

        TypeRegistry::instance().register_property<ClothGridSettings,
            &ClothGridSettings::height_segments>(
            "height_segments",
            PropertyMeta()
                .set_display_name("Height Segments")
                .set_category("Grid")
                .set_range(1u, 100u));

        TypeRegistry::instance().register_property<ClothGridSettings,
            &ClothGridSettings::width>(
            "width",
            PropertyMeta()
                .set_display_name("Width")
                .set_category("Grid")
                .set_range(0.1f, 50.0f));

        TypeRegistry::instance().register_property<ClothGridSettings,
            &ClothGridSettings::height>(
            "height",
            PropertyMeta()
                .set_display_name("Height")
                .set_category("Grid")
                .set_range(0.1f, 50.0f));

        TypeRegistry::instance().register_property<ClothGridSettings,
            &ClothGridSettings::double_sided>(
            "double_sided",
            PropertyMeta()
                .set_display_name("Double Sided")
                .set_category("Grid"));
    }
};
static ClothGridSettingsRegistrar _cloth_grid_settings_registrar;

// ClothCollisionSettings registration
struct ClothCollisionSettingsRegistrar {
    ClothCollisionSettingsRegistrar() {
        TypeRegistry::instance().register_type<ClothCollisionSettings>(
            "ClothCollisionSettings",
            TypeMeta()
                .set_display_name("Cloth Collision Settings")
                .set_description("Cloth collision configuration"));

        TypeRegistry::instance().register_property<ClothCollisionSettings,
            &ClothCollisionSettings::self_collision>(
            "self_collision",
            PropertyMeta()
                .set_display_name("Self Collision")
                .set_category("Collision"));

        TypeRegistry::instance().register_property<ClothCollisionSettings,
            &ClothCollisionSettings::world_collision>(
            "world_collision",
            PropertyMeta()
                .set_display_name("World Collision")
                .set_category("Collision"));

        TypeRegistry::instance().register_property<ClothCollisionSettings,
            &ClothCollisionSettings::dynamic_collision>(
            "dynamic_collision",
            PropertyMeta()
                .set_display_name("Dynamic Collision")
                .set_category("Collision"));

        TypeRegistry::instance().register_property<ClothCollisionSettings,
            &ClothCollisionSettings::collision_margin>(
            "collision_margin",
            PropertyMeta()
                .set_display_name("Collision Margin")
                .set_category("Collision")
                .set_range(0.001f, 0.5f));

        TypeRegistry::instance().register_property<ClothCollisionSettings,
            &ClothCollisionSettings::collision_mask>(
            "collision_mask",
            PropertyMeta()
                .set_display_name("Collision Mask")
                .set_category("Collision"));
    }
};
static ClothCollisionSettingsRegistrar _cloth_collision_settings_registrar;

// ClothWindSettings registration
struct ClothWindSettingsRegistrar {
    ClothWindSettingsRegistrar() {
        TypeRegistry::instance().register_type<ClothWindSettings>(
            "ClothWindSettings",
            TypeMeta()
                .set_display_name("Cloth Wind Settings")
                .set_description("Wind effect configuration for cloth"));

        TypeRegistry::instance().register_property<ClothWindSettings,
            &ClothWindSettings::direction>(
            "direction",
            PropertyMeta()
                .set_display_name("Direction")
                .set_category("Wind"));

        TypeRegistry::instance().register_property<ClothWindSettings,
            &ClothWindSettings::strength>(
            "strength",
            PropertyMeta()
                .set_display_name("Strength")
                .set_category("Wind")
                .set_range(0.0f, 50.0f));

        TypeRegistry::instance().register_property<ClothWindSettings,
            &ClothWindSettings::turbulence>(
            "turbulence",
            PropertyMeta()
                .set_display_name("Turbulence")
                .set_category("Wind")
                .set_range(0.0f, 1.0f));

        TypeRegistry::instance().register_property<ClothWindSettings,
            &ClothWindSettings::turbulence_frequency>(
            "turbulence_frequency",
            PropertyMeta()
                .set_display_name("Turbulence Frequency")
                .set_category("Wind")
                .set_range(0.1f, 10.0f));

        TypeRegistry::instance().register_property<ClothWindSettings,
            &ClothWindSettings::drag_coefficient>(
            "drag_coefficient",
            PropertyMeta()
                .set_display_name("Drag Coefficient")
                .set_category("Wind")
                .set_range(0.0f, 2.0f));
    }
};
static ClothWindSettingsRegistrar _cloth_wind_settings_registrar;

// ClothAttachment registration
struct ClothAttachmentRegistrar {
    ClothAttachmentRegistrar() {
        TypeRegistry::instance().register_type<ClothAttachment>(
            "ClothAttachment",
            TypeMeta()
                .set_display_name("Cloth Attachment")
                .set_description("Cloth vertex attachment configuration"));

        TypeRegistry::instance().register_property<ClothAttachment,
            &ClothAttachment::vertex_index>(
            "vertex_index",
            PropertyMeta()
                .set_display_name("Vertex Index")
                .set_category("Attachment"));

        TypeRegistry::instance().register_property<ClothAttachment,
            &ClothAttachment::type>(
            "type",
            PropertyMeta()
                .set_display_name("Type")
                .set_category("Attachment"));

        TypeRegistry::instance().register_property<ClothAttachment,
            &ClothAttachment::attach_to_entity>(
            "attach_to_entity",
            PropertyMeta()
                .set_display_name("Attach to Entity")
                .set_category("Attachment"));

        TypeRegistry::instance().register_property<ClothAttachment,
            &ClothAttachment::local_offset>(
            "local_offset",
            PropertyMeta()
                .set_display_name("Local Offset")
                .set_category("Attachment"));

        TypeRegistry::instance().register_property<ClothAttachment,
            &ClothAttachment::world_position>(
            "world_position",
            PropertyMeta()
                .set_display_name("World Position")
                .set_category("Attachment"));

        TypeRegistry::instance().register_property<ClothAttachment,
            &ClothAttachment::spring_stiffness>(
            "spring_stiffness",
            PropertyMeta()
                .set_display_name("Spring Stiffness")
                .set_category("Spring")
                .set_range(1.0f, 10000.0f));

        TypeRegistry::instance().register_property<ClothAttachment,
            &ClothAttachment::spring_damping>(
            "spring_damping",
            PropertyMeta()
                .set_display_name("Spring Damping")
                .set_category("Spring")
                .set_range(0.0f, 100.0f));

        TypeRegistry::instance().register_property<ClothAttachment,
            &ClothAttachment::max_distance>(
            "max_distance",
            PropertyMeta()
                .set_display_name("Max Distance")
                .set_category("Spring")
                .set_range(0.0f, 1.0f));
    }
};
static ClothAttachmentRegistrar _cloth_attachment_registrar;

} // anonymous namespace
