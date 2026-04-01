#include <engine/physics/water_volume.hpp>
#include <engine/physics/buoyancy_component.hpp>
#include <engine/physics/boat.hpp>
#include <engine/reflect/reflect.hpp>

namespace {

using namespace engine::physics;
using namespace engine::reflect;
using engine::core::Quat;
using engine::core::Vec3;

template<typename ShapeOwner>
ShapeType get_shape_type(const ShapeOwner& owner) {
    return std::visit([](const auto& shape) { return shape.type; }, owner.hull_shape);
}

template<typename ShapeOwner>
void set_shape_type(ShapeOwner& owner, ShapeType type) {
    switch (type) {
        case ShapeType::Box:
            owner.hull_shape = BoxShapeSettings{};
            break;
        case ShapeType::Sphere:
            owner.hull_shape = SphereShapeSettings{};
            break;
        case ShapeType::Capsule:
            owner.hull_shape = CapsuleShapeSettings{};
            break;
        case ShapeType::Cylinder:
            owner.hull_shape = CylinderShapeSettings{};
            break;
        case ShapeType::ConvexHull:
            owner.hull_shape = ConvexHullShapeSettings{};
            break;
        case ShapeType::Mesh:
            owner.hull_shape = MeshShapeSettings{};
            break;
        case ShapeType::HeightField:
            owner.hull_shape = HeightFieldShapeSettings{};
            break;
        case ShapeType::Compound:
            owner.hull_shape = CompoundShapeSettings{};
            break;
    }
}

template<typename ShapeT, typename ShapeOwner>
const ShapeT* get_shape_if(const ShapeOwner& owner) {
    return std::get_if<ShapeT>(&owner.hull_shape);
}

Vec3 get_center_offset(const HullSettings& hull) {
    return std::visit([](const auto& shape) { return shape.center_offset; }, hull.hull_shape);
}

void set_center_offset(HullSettings& hull, const Vec3& offset) {
    std::visit([&](auto& shape) { shape.center_offset = offset; }, hull.hull_shape);
}

Quat get_rotation_offset(const HullSettings& hull) {
    return std::visit([](const auto& shape) { return shape.rotation_offset; }, hull.hull_shape);
}

void set_rotation_offset(HullSettings& hull, const Quat& offset) {
    std::visit([&](auto& shape) { shape.rotation_offset = offset; }, hull.hull_shape);
}

struct WaterBoatEnumRegistrar {
    WaterBoatEnumRegistrar() {
        auto& registry = TypeRegistry::instance();
        registry.register_enum<WaterShape>("WaterShape", {
            {WaterShape::Box, "Box"},
            {WaterShape::Sphere, "Sphere"},
            {WaterShape::Infinite, "Infinite"},
        });
        registry.register_enum<BuoyancyMode>("BuoyancyMode", {
            {BuoyancyMode::Automatic, "Automatic"},
            {BuoyancyMode::Manual, "Manual"},
            {BuoyancyMode::Voxel, "Voxel"},
        });
        registry.register_enum<BoatMode>("BoatMode", {
            {BoatMode::Arcade, "Arcade"},
            {BoatMode::Simulation, "Simulation"},
        });
        registry.register_enum<ShapeType>("ShapeType", {
            {ShapeType::Box, "Box"},
            {ShapeType::Sphere, "Sphere"},
            {ShapeType::Capsule, "Capsule"},
            {ShapeType::Cylinder, "Cylinder"},
            {ShapeType::ConvexHull, "ConvexHull"},
            {ShapeType::Mesh, "Mesh"},
            {ShapeType::HeightField, "HeightField"},
            {ShapeType::Compound, "Compound"},
        });
    }
};
static WaterBoatEnumRegistrar _water_boat_enum_registrar;

struct WaterBoatVectorRegistrar {
    WaterBoatVectorRegistrar() {
        auto& registry = TypeRegistry::instance();
        registry.register_vector_type<BuoyancyPoint>();
        registry.register_vector_type<PropellerSettings>();
        registry.register_vector_type<RudderSettings>();
    }
};
static WaterBoatVectorRegistrar _water_boat_vector_registrar;

struct WaveSettingsRegistrar {
    WaveSettingsRegistrar() {
        TypeRegistry::instance().register_type<WaveSettings>(
            "WaveSettings",
            TypeMeta()
                .set_display_name("Wave Settings")
                .set_description("Dynamic water surface settings"));

        TypeRegistry::instance().register_property<WaveSettings, &WaveSettings::enabled>(
            "enabled",
            PropertyMeta().set_display_name("Enabled").set_category("Wave"));
        TypeRegistry::instance().register_property<WaveSettings, &WaveSettings::amplitude>(
            "amplitude",
            PropertyMeta().set_display_name("Amplitude").set_category("Wave").set_range(0.0f, 10.0f));
        TypeRegistry::instance().register_property<WaveSettings, &WaveSettings::wavelength>(
            "wavelength",
            PropertyMeta().set_display_name("Wavelength").set_category("Wave").set_range(0.1f, 1000.0f));
        TypeRegistry::instance().register_property<WaveSettings, &WaveSettings::speed>(
            "speed",
            PropertyMeta().set_display_name("Speed").set_category("Wave").set_range(0.0f, 100.0f));
        TypeRegistry::instance().register_property<WaveSettings, &WaveSettings::direction>(
            "direction",
            PropertyMeta().set_display_name("Direction").set_category("Wave"));
        TypeRegistry::instance().register_property<WaveSettings, &WaveSettings::use_gerstner>(
            "use_gerstner",
            PropertyMeta().set_display_name("Use Gerstner").set_category("Wave"));
        TypeRegistry::instance().register_property<WaveSettings, &WaveSettings::steepness>(
            "steepness",
            PropertyMeta().set_display_name("Steepness").set_category("Wave").set_range(0.0f, 1.0f));
    }
};
static WaveSettingsRegistrar _wave_settings_registrar;

struct BuoyancyPointRegistrar {
    BuoyancyPointRegistrar() {
        TypeRegistry::instance().register_type<BuoyancyPoint>(
            "BuoyancyPoint",
            TypeMeta()
                .set_display_name("Buoyancy Point")
                .set_description("Manual buoyancy sample point"));

        TypeRegistry::instance().register_property<BuoyancyPoint, &BuoyancyPoint::local_position>(
            "local_position",
            PropertyMeta().set_display_name("Local Position").set_category("Point"));
        TypeRegistry::instance().register_property<BuoyancyPoint, &BuoyancyPoint::radius>(
            "radius",
            PropertyMeta().set_display_name("Radius").set_category("Point").set_range(0.0f, 100.0f));
        TypeRegistry::instance().register_property<BuoyancyPoint, &BuoyancyPoint::volume>(
            "volume",
            PropertyMeta().set_display_name("Volume").set_category("Point").set_range(0.0f, 10000.0f));
    }
};
static BuoyancyPointRegistrar _buoyancy_point_registrar;

struct HullSettingsRegistrar {
    HullSettingsRegistrar() {
        auto& registry = TypeRegistry::instance();
        registry.register_type<HullSettings>(
            "HullSettings",
            TypeMeta()
                .set_display_name("Hull Settings")
                .set_description("Boat hull configuration"));

        registry.register_property<HullSettings, &HullSettings::hull_mass>(
            "hull_mass",
            PropertyMeta().set_display_name("Hull Mass").set_category("Hull").set_range(1.0f, 100000.0f));
        registry.register_property<HullSettings, &HullSettings::center_of_mass_offset>(
            "center_of_mass_offset",
            PropertyMeta().set_display_name("Center of Mass Offset").set_category("Hull"));
        registry.register_property<HullSettings, &HullSettings::buoyancy_points>(
            "buoyancy_points",
            PropertyMeta().set_display_name("Buoyancy Points").set_category("Buoyancy"));
        registry.register_property<HullSettings, &HullSettings::hull_drag_coefficient>(
            "hull_drag_coefficient",
            PropertyMeta().set_display_name("Drag Coefficient").set_category("Hydrodynamics").set_range(0.0f, 10.0f));
        registry.register_property<HullSettings, &HullSettings::hull_lift_coefficient>(
            "hull_lift_coefficient",
            PropertyMeta().set_display_name("Lift Coefficient").set_category("Hydrodynamics").set_range(0.0f, 10.0f));
        registry.register_property<HullSettings, &HullSettings::drag_reference_area>(
            "drag_reference_area",
            PropertyMeta().set_display_name("Drag Reference Area").set_category("Hydrodynamics"));
        registry.register_property<HullSettings, &HullSettings::hull_half_extents>(
            "hull_half_extents",
            PropertyMeta().set_display_name("Hull Half Extents").set_category("Hull"));

        registry.register_property<HullSettings, &HullSettings::hull_mass>(
            "shape_type",
            PropertyMeta().set_display_name("Shape Type").set_category("Shape"),
            [](const HullSettings& hull) { return get_shape_type(hull); },
            [](HullSettings& hull, ShapeType type) { set_shape_type(hull, type); });
        registry.register_property<HullSettings, &HullSettings::hull_mass>(
            "shape_center_offset",
            PropertyMeta().set_display_name("Center Offset").set_category("Shape"),
            [](const HullSettings& hull) { return get_center_offset(hull); },
            [](HullSettings& hull, const Vec3& value) { set_center_offset(hull, value); });
        registry.register_property<HullSettings, &HullSettings::hull_mass>(
            "shape_rotation_offset",
            PropertyMeta().set_display_name("Rotation Offset").set_category("Shape"),
            [](const HullSettings& hull) { return get_rotation_offset(hull); },
            [](HullSettings& hull, const Quat& value) { set_rotation_offset(hull, value); });
        registry.register_property<HullSettings, &HullSettings::hull_mass>(
            "box_half_extents",
            PropertyMeta().set_display_name("Box Half Extents").set_category("Shape"),
            [](const HullSettings& hull) {
                return get_shape_if<BoxShapeSettings>(hull) ? get_shape_if<BoxShapeSettings>(hull)->half_extents : Vec3{0.5f};
            },
            [](HullSettings& hull, const Vec3& value) {
                if (auto* shape = std::get_if<BoxShapeSettings>(&hull.hull_shape)) {
                    shape->half_extents = value;
                }
                hull.hull_half_extents = value;
            });
    }
};
static HullSettingsRegistrar _hull_settings_registrar;

struct PropellerSettingsRegistrar {
    PropellerSettingsRegistrar() {
        TypeRegistry::instance().register_type<PropellerSettings>(
            "PropellerSettings",
            TypeMeta()
                .set_display_name("Propeller Settings")
                .set_description("Boat propulsion configuration"));

        TypeRegistry::instance().register_property<PropellerSettings, &PropellerSettings::position>(
            "position",
            PropertyMeta().set_display_name("Position").set_category("Propeller"));
        TypeRegistry::instance().register_property<PropellerSettings, &PropellerSettings::thrust_direction>(
            "thrust_direction",
            PropertyMeta().set_display_name("Thrust Direction").set_category("Propeller"));
        TypeRegistry::instance().register_property<PropellerSettings, &PropellerSettings::max_thrust>(
            "max_thrust",
            PropertyMeta().set_display_name("Max Thrust").set_category("Propeller").set_range(0.0f, 1000000.0f));
        TypeRegistry::instance().register_property<PropellerSettings, &PropellerSettings::max_rpm>(
            "max_rpm",
            PropertyMeta().set_display_name("Max RPM").set_category("Propeller").set_range(0.0f, 20000.0f));
        TypeRegistry::instance().register_property<PropellerSettings, &PropellerSettings::propeller_radius>(
            "propeller_radius",
            PropertyMeta().set_display_name("Radius").set_category("Propeller").set_range(0.0f, 100.0f));
        TypeRegistry::instance().register_property<PropellerSettings, &PropellerSettings::efficiency>(
            "efficiency",
            PropertyMeta().set_display_name("Efficiency").set_category("Propeller").set_range(0.0f, 1.0f));
        TypeRegistry::instance().register_property<PropellerSettings, &PropellerSettings::reverse_efficiency>(
            "reverse_efficiency",
            PropertyMeta().set_display_name("Reverse Efficiency").set_category("Propeller").set_range(0.0f, 1.0f));
        TypeRegistry::instance().register_property<PropellerSettings, &PropellerSettings::spin_up_time>(
            "spin_up_time",
            PropertyMeta().set_display_name("Spin Up Time").set_category("Propeller").set_range(0.0f, 10.0f));
        TypeRegistry::instance().register_property<PropellerSettings, &PropellerSettings::spin_down_time>(
            "spin_down_time",
            PropertyMeta().set_display_name("Spin Down Time").set_category("Propeller").set_range(0.0f, 10.0f));
    }
};
static PropellerSettingsRegistrar _propeller_settings_registrar;

struct RudderSettingsRegistrar {
    RudderSettingsRegistrar() {
        TypeRegistry::instance().register_type<RudderSettings>(
            "RudderSettings",
            TypeMeta()
                .set_display_name("Rudder Settings")
                .set_description("Boat steering configuration"));

        TypeRegistry::instance().register_property<RudderSettings, &RudderSettings::position>(
            "position",
            PropertyMeta().set_display_name("Position").set_category("Rudder"));
        TypeRegistry::instance().register_property<RudderSettings, &RudderSettings::max_angle>(
            "max_angle",
            PropertyMeta().set_display_name("Max Angle").set_category("Rudder").set_angle(true).set_range(0.0f, 3.14159f));
        TypeRegistry::instance().register_property<RudderSettings, &RudderSettings::area>(
            "area",
            PropertyMeta().set_display_name("Area").set_category("Rudder").set_range(0.0f, 1000.0f));
        TypeRegistry::instance().register_property<RudderSettings, &RudderSettings::lift_coefficient>(
            "lift_coefficient",
            PropertyMeta().set_display_name("Lift Coefficient").set_category("Rudder").set_range(0.0f, 10.0f));
        TypeRegistry::instance().register_property<RudderSettings, &RudderSettings::turn_rate>(
            "turn_rate",
            PropertyMeta().set_display_name("Turn Rate").set_category("Rudder").set_range(0.0f, 10.0f));
        TypeRegistry::instance().register_property<RudderSettings, &RudderSettings::stall_angle>(
            "stall_angle",
            PropertyMeta().set_display_name("Stall Angle").set_category("Rudder").set_angle(true).set_range(0.0f, 3.14159f));
    }
};
static RudderSettingsRegistrar _rudder_settings_registrar;

struct ArcadeBoatSettingsRegistrar {
    ArcadeBoatSettingsRegistrar() {
        TypeRegistry::instance().register_type<ArcadeBoatSettings>(
            "ArcadeBoatSettings",
            TypeMeta()
                .set_display_name("Arcade Boat Settings")
                .set_description("Arcade boat handling settings"));

        TypeRegistry::instance().register_property<ArcadeBoatSettings, &ArcadeBoatSettings::max_speed>(
            "max_speed",
            PropertyMeta().set_display_name("Max Speed").set_category("Movement").set_range(0.0f, 200.0f));
        TypeRegistry::instance().register_property<ArcadeBoatSettings, &ArcadeBoatSettings::acceleration>(
            "acceleration",
            PropertyMeta().set_display_name("Acceleration").set_category("Movement").set_range(0.0f, 100.0f));
        TypeRegistry::instance().register_property<ArcadeBoatSettings, &ArcadeBoatSettings::deceleration>(
            "deceleration",
            PropertyMeta().set_display_name("Deceleration").set_category("Movement").set_range(0.0f, 100.0f));
        TypeRegistry::instance().register_property<ArcadeBoatSettings, &ArcadeBoatSettings::braking>(
            "braking",
            PropertyMeta().set_display_name("Braking").set_category("Movement").set_range(0.0f, 100.0f));
        TypeRegistry::instance().register_property<ArcadeBoatSettings, &ArcadeBoatSettings::turn_speed>(
            "turn_speed",
            PropertyMeta().set_display_name("Turn Speed").set_category("Handling").set_range(0.0f, 10.0f));
        TypeRegistry::instance().register_property<ArcadeBoatSettings, &ArcadeBoatSettings::turn_speed_at_max>(
            "turn_speed_at_max",
            PropertyMeta().set_display_name("Turn Speed At Max").set_category("Handling").set_range(0.0f, 10.0f));
        TypeRegistry::instance().register_property<ArcadeBoatSettings, &ArcadeBoatSettings::stability_roll>(
            "stability_roll",
            PropertyMeta().set_display_name("Roll Stability").set_category("Handling").set_range(0.0f, 1.0f));
        TypeRegistry::instance().register_property<ArcadeBoatSettings, &ArcadeBoatSettings::stability_pitch>(
            "stability_pitch",
            PropertyMeta().set_display_name("Pitch Stability").set_category("Handling").set_range(0.0f, 1.0f));
        TypeRegistry::instance().register_property<ArcadeBoatSettings, &ArcadeBoatSettings::drift_factor>(
            "drift_factor",
            PropertyMeta().set_display_name("Drift Factor").set_category("Handling").set_range(0.0f, 1.0f));
        TypeRegistry::instance().register_property<ArcadeBoatSettings, &ArcadeBoatSettings::wave_response>(
            "wave_response",
            PropertyMeta().set_display_name("Wave Response").set_category("Handling").set_range(0.0f, 1.0f));
    }
};
static ArcadeBoatSettingsRegistrar _arcade_boat_settings_registrar;

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
            &WaterVolumeComponent::surface_drag>(
            "surface_drag",
            PropertyMeta()
                .set_display_name("Surface Drag")
                .set_category("Physics")
                .set_range(0.0f, 10.0f));

        TypeRegistry::instance().register_property<WaterVolumeComponent,
            &WaterVolumeComponent::flow_velocity>(
            "flow_velocity",
            PropertyMeta()
                .set_display_name("Flow Velocity")
                .set_category("Physics"));

        TypeRegistry::instance().register_property<WaterVolumeComponent,
            &WaterVolumeComponent::waves>(
            "waves",
            PropertyMeta()
                .set_display_name("Waves")
                .set_category("Wave"));

        TypeRegistry::instance().register_property<WaterVolumeComponent,
            &WaterVolumeComponent::water_color>(
            "water_color",
            PropertyMeta()
                .set_display_name("Water Color")
                .set_category("Rendering"));

        TypeRegistry::instance().register_property<WaterVolumeComponent,
            &WaterVolumeComponent::deep_color>(
            "deep_color",
            PropertyMeta()
                .set_display_name("Deep Color")
                .set_category("Rendering"));

        TypeRegistry::instance().register_property<WaterVolumeComponent,
            &WaterVolumeComponent::transparency>(
            "transparency",
            PropertyMeta()
                .set_display_name("Transparency")
                .set_category("Rendering")
                .set_range(0.0f, 1.0f));

        TypeRegistry::instance().register_property<WaterVolumeComponent,
            &WaterVolumeComponent::refraction_strength>(
            "refraction_strength",
            PropertyMeta()
                .set_display_name("Refraction Strength")
                .set_category("Rendering")
                .set_range(0.0f, 10.0f));

        TypeRegistry::instance().register_property<WaterVolumeComponent,
            &WaterVolumeComponent::foam_threshold>(
            "foam_threshold",
            PropertyMeta()
                .set_display_name("Foam Threshold")
                .set_category("Rendering")
                .set_range(0.0f, 10.0f));
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
            &BuoyancyComponent::buoyancy_points>(
            "buoyancy_points",
            PropertyMeta()
                .set_display_name("Buoyancy Points")
                .set_category("Buoyancy"));

        TypeRegistry::instance().register_property<BuoyancyComponent,
            &BuoyancyComponent::voxel_resolution>(
            "voxel_resolution",
            PropertyMeta()
                .set_display_name("Voxel Resolution")
                .set_category("Buoyancy"));

        TypeRegistry::instance().register_property<BuoyancyComponent,
            &BuoyancyComponent::max_voxels>(
            "max_voxels",
            PropertyMeta()
                .set_display_name("Max Voxels")
                .set_category("Buoyancy")
                .set_range(1u, 4096u));

        TypeRegistry::instance().register_property<BuoyancyComponent,
            &BuoyancyComponent::volume_override>(
            "volume_override",
            PropertyMeta()
                .set_display_name("Volume Override")
                .set_category("Buoyancy")
                .set_range(0.0f, 100000.0f));

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
            &BuoyancyComponent::surface_splash_threshold>(
            "surface_splash_threshold",
            PropertyMeta()
                .set_display_name("Surface Splash Threshold")
                .set_category("Surface")
                .set_range(0.0f, 100.0f));

        TypeRegistry::instance().register_property<BuoyancyComponent,
            &BuoyancyComponent::surface_exit_threshold>(
            "surface_exit_threshold",
            PropertyMeta()
                .set_display_name("Surface Exit Threshold")
                .set_category("Surface")
                .set_range(0.0f, 100.0f));

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
            &BoatComponent::hull>(
            "hull",
            PropertyMeta()
                .set_display_name("Hull")
                .set_category("Hull"));

        TypeRegistry::instance().register_property<BoatComponent,
            &BoatComponent::propellers>(
            "propellers",
            PropertyMeta()
                .set_display_name("Propellers")
                .set_category("Propulsion"));

        TypeRegistry::instance().register_property<BoatComponent,
            &BoatComponent::rudders>(
            "rudders",
            PropertyMeta()
                .set_display_name("Rudders")
                .set_category("Steering"));

        TypeRegistry::instance().register_property<BoatComponent,
            &BoatComponent::arcade>(
            "arcade",
            PropertyMeta()
                .set_display_name("Arcade")
                .set_category("Handling"));

        TypeRegistry::instance().register_property<BoatComponent,
            &BoatComponent::layer>(
            "layer",
            PropertyMeta()
                .set_display_name("Collision Layer")
                .set_category("Collision"));

        TypeRegistry::instance().register_property<BoatComponent,
            &BoatComponent::collision_mask>(
            "collision_mask",
            PropertyMeta()
                .set_display_name("Collision Mask")
                .set_category("Collision"));

        TypeRegistry::instance().register_property<BoatComponent,
            &BoatComponent::throttle>(
            "throttle",
            PropertyMeta()
                .set_display_name("Throttle")
                .set_category("Input"));

        TypeRegistry::instance().register_property<BoatComponent,
            &BoatComponent::rudder>(
            "rudder",
            PropertyMeta()
                .set_display_name("Rudder")
                .set_category("Input"));

        TypeRegistry::instance().register_property<BoatComponent,
            &BoatComponent::engine_on>(
            "engine_on",
            PropertyMeta()
                .set_display_name("Engine On")
                .set_category("Input"));

    }
};
static BoatComponentRegistrar _boat_registrar;

} // anonymous namespace

namespace engine::physics {

void ensure_water_component_registration() {}

} // namespace engine::physics
