#include <engine/physics/water_volume.hpp>
#include <engine/physics/buoyancy_component.hpp>
#include <engine/physics/boat.hpp>
#include <engine/reflect/reflect.hpp>

namespace {

using namespace engine::physics;
using namespace engine::reflect;

// WaterVolumeComponent registration
struct WaterVolumeComponentRegistrar {
    WaterVolumeComponentRegistrar() {
        TypeRegistry::instance().register_component<WaterVolumeComponent>(
            "WaterVolumeComponent",
            TypeMeta()
                .set_display_name("Water Volume")
                .set_description("Defines a water region for buoyancy and swimming"));

        TypeRegistry::instance().register_property<WaterVolumeComponent,
            &WaterVolumeComponent::shape>(
            "shape",
            PropertyMeta()
                .set_display_name("Shape")
                .set_category("Volume"));

        TypeRegistry::instance().register_property<WaterVolumeComponent,
            &WaterVolumeComponent::box_half_extents>(
            "box_half_extents",
            PropertyMeta()
                .set_display_name("Box Half Extents")
                .set_category("Volume"));

        TypeRegistry::instance().register_property<WaterVolumeComponent,
            &WaterVolumeComponent::sphere_radius>(
            "sphere_radius",
            PropertyMeta()
                .set_display_name("Sphere Radius")
                .set_category("Volume")
                .set_range(0.1f, 1000.0f));

        TypeRegistry::instance().register_property<WaterVolumeComponent,
            &WaterVolumeComponent::surface_height>(
            "surface_height",
            PropertyMeta()
                .set_display_name("Surface Height")
                .set_category("Volume"));

        TypeRegistry::instance().register_property<WaterVolumeComponent,
            &WaterVolumeComponent::density>(
            "density",
            PropertyMeta()
                .set_display_name("Density")
                .set_category("Physics")
                .set_range(100.0f, 2000.0f));

        TypeRegistry::instance().register_property<WaterVolumeComponent,
            &WaterVolumeComponent::linear_drag>(
            "linear_drag",
            PropertyMeta()
                .set_display_name("Linear Drag")
                .set_category("Physics")
                .set_range(0.0f, 10.0f));

        TypeRegistry::instance().register_property<WaterVolumeComponent,
            &WaterVolumeComponent::angular_drag>(
            "angular_drag",
            PropertyMeta()
                .set_display_name("Angular Drag")
                .set_category("Physics")
                .set_range(0.0f, 10.0f));

        TypeRegistry::instance().register_property<WaterVolumeComponent,
            &WaterVolumeComponent::flow_velocity>(
            "flow_velocity",
            PropertyMeta()
                .set_display_name("Flow Velocity")
                .set_category("Physics"));

        TypeRegistry::instance().register_property<WaterVolumeComponent,
            &WaterVolumeComponent::water_color>(
            "water_color",
            PropertyMeta()
                .set_display_name("Water Color")
                .set_category("Rendering"));

        TypeRegistry::instance().register_property<WaterVolumeComponent,
            &WaterVolumeComponent::transparency>(
            "transparency",
            PropertyMeta()
                .set_display_name("Transparency")
                .set_category("Rendering")
                .set_range(0.0f, 1.0f));
    }
};
static WaterVolumeComponentRegistrar _water_volume_registrar;

// BuoyancyComponent registration
struct BuoyancyComponentRegistrar {
    BuoyancyComponentRegistrar() {
        TypeRegistry::instance().register_component<BuoyancyComponent>(
            "BuoyancyComponent",
            TypeMeta()
                .set_display_name("Buoyancy")
                .set_description("Makes rigid bodies float in water"));

        TypeRegistry::instance().register_property<BuoyancyComponent,
            &BuoyancyComponent::mode>(
            "mode",
            PropertyMeta()
                .set_display_name("Mode")
                .set_category("Buoyancy"));

        TypeRegistry::instance().register_property<BuoyancyComponent,
            &BuoyancyComponent::buoyancy_multiplier>(
            "buoyancy_multiplier",
            PropertyMeta()
                .set_display_name("Buoyancy Multiplier")
                .set_category("Buoyancy")
                .set_range(0.0f, 5.0f));

        TypeRegistry::instance().register_property<BuoyancyComponent,
            &BuoyancyComponent::water_drag_multiplier>(
            "water_drag_multiplier",
            PropertyMeta()
                .set_display_name("Water Drag Multiplier")
                .set_category("Physics")
                .set_range(0.0f, 5.0f));

        TypeRegistry::instance().register_property<BuoyancyComponent,
            &BuoyancyComponent::linear_damping_in_water>(
            "linear_damping_in_water",
            PropertyMeta()
                .set_display_name("Linear Damping in Water")
                .set_category("Physics")
                .set_range(0.0f, 2.0f));

        TypeRegistry::instance().register_property<BuoyancyComponent,
            &BuoyancyComponent::angular_damping_in_water>(
            "angular_damping_in_water",
            PropertyMeta()
                .set_display_name("Angular Damping in Water")
                .set_category("Physics")
                .set_range(0.0f, 2.0f));

        TypeRegistry::instance().register_property<BuoyancyComponent,
            &BuoyancyComponent::center_of_buoyancy_offset_y>(
            "center_of_buoyancy_offset_y",
            PropertyMeta()
                .set_display_name("Buoyancy Center Offset Y")
                .set_category("Buoyancy"));

        TypeRegistry::instance().register_property<BuoyancyComponent,
            &BuoyancyComponent::apply_rotational_damping>(
            "apply_rotational_damping",
            PropertyMeta()
                .set_display_name("Apply Rotational Damping")
                .set_category("Physics"));
    }
};
static BuoyancyComponentRegistrar _buoyancy_registrar;

// BoatComponent registration
struct BoatComponentRegistrar {
    BoatComponentRegistrar() {
        TypeRegistry::instance().register_component<BoatComponent>(
            "BoatComponent",
            TypeMeta()
                .set_display_name("Boat")
                .set_description("Boat/ship physics controller"));

        TypeRegistry::instance().register_property<BoatComponent,
            &BoatComponent::mode>(
            "mode",
            PropertyMeta()
                .set_display_name("Mode")
                .set_category("General"));

        TypeRegistry::instance().register_property<BoatComponent,
            &BoatComponent::layer>(
            "layer",
            PropertyMeta()
                .set_display_name("Collision Layer")
                .set_category("Collision"));

        // Note: Hull, propeller, and rudder settings would need nested
        // property registration for full editor support
    }
};
static BoatComponentRegistrar _boat_registrar;

} // anonymous namespace
