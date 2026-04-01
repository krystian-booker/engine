#include <engine/physics/vehicle_component.hpp>
#include <engine/physics/vehicle.hpp>
#include <engine/reflect/reflect.hpp>

namespace {

using namespace engine::physics;
using namespace engine::reflect;
using engine::core::Quat;
using engine::core::Vec3;

ShapeType get_chassis_shape_type(const VehicleComponent& vehicle) {
    return std::visit([](const auto& shape) { return shape.type; }, vehicle.chassis_shape);
}

void set_chassis_shape_type(VehicleComponent& vehicle, ShapeType type) {
    switch (type) {
        case ShapeType::Box:
            vehicle.chassis_shape = BoxShapeSettings{};
            break;
        case ShapeType::Sphere:
            vehicle.chassis_shape = SphereShapeSettings{};
            break;
        case ShapeType::Capsule:
            vehicle.chassis_shape = CapsuleShapeSettings{};
            break;
        case ShapeType::Cylinder:
            vehicle.chassis_shape = CylinderShapeSettings{};
            break;
        case ShapeType::ConvexHull:
            vehicle.chassis_shape = ConvexHullShapeSettings{};
            break;
        case ShapeType::Mesh:
            vehicle.chassis_shape = MeshShapeSettings{};
            break;
        case ShapeType::HeightField:
            vehicle.chassis_shape = HeightFieldShapeSettings{};
            break;
        case ShapeType::Compound:
            vehicle.chassis_shape = CompoundShapeSettings{};
            break;
    }
}

template<typename ShapeT>
const ShapeT* get_shape_if(const VehicleComponent& vehicle) {
    return std::get_if<ShapeT>(&vehicle.chassis_shape);
}

Vec3 get_center_offset(const VehicleComponent& vehicle) {
    return std::visit([](const auto& shape) { return shape.center_offset; }, vehicle.chassis_shape);
}

void set_center_offset(VehicleComponent& vehicle, const Vec3& offset) {
    std::visit([&](auto& shape) { shape.center_offset = offset; }, vehicle.chassis_shape);
}

Quat get_rotation_offset(const VehicleComponent& vehicle) {
    return std::visit([](const auto& shape) { return shape.rotation_offset; }, vehicle.chassis_shape);
}

void set_rotation_offset(VehicleComponent& vehicle, const Quat& offset) {
    std::visit([&](auto& shape) { shape.rotation_offset = offset; }, vehicle.chassis_shape);
}

struct VehicleEnumRegistrar {
    VehicleEnumRegistrar() {
        auto& registry = TypeRegistry::instance();
        registry.register_enum<VehicleType>("VehicleType", {
            {VehicleType::Wheeled, "Wheeled"},
            {VehicleType::Tracked, "Tracked"},
            {VehicleType::Motorcycle, "Motorcycle"},
        });
        registry.register_enum<VehicleMode>("VehicleMode", {
            {VehicleMode::Arcade, "Arcade"},
            {VehicleMode::Simulation, "Simulation"},
        });
        registry.register_enum<DriveType>("DriveType", {
            {DriveType::FrontWheelDrive, "FrontWheelDrive"},
            {DriveType::RearWheelDrive, "RearWheelDrive"},
            {DriveType::AllWheelDrive, "AllWheelDrive"},
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
        registry.register_enum<DifferentialType>("DifferentialType", {
            {DifferentialType::Open, "Open"},
            {DifferentialType::Limited, "Limited"},
            {DifferentialType::Locked, "Locked"},
        });
    }
};
static VehicleEnumRegistrar _vehicle_enum_registrar;

struct VehicleVectorRegistrar {
    VehicleVectorRegistrar() {
        auto& registry = TypeRegistry::instance();
        registry.register_vector_type<WheelSettings>();
        registry.register_vector_type<AntiRollBarSettings>();
    }
};
static VehicleVectorRegistrar _vehicle_vector_registrar;

// VehicleComponent registration
struct VehicleComponentRegistrar {
    VehicleComponentRegistrar() {
        auto& registry = TypeRegistry::instance();
        registry.register_component<VehicleComponent>(
            "VehicleComponent",
            TypeMeta()
                .set_display_name("Vehicle")
                .set_description("Vehicle physics configuration"));

        registry.register_property<VehicleComponent,
            &VehicleComponent::type>(
            "type",
            PropertyMeta()
                .set_display_name("Type")
                .set_category("General"));

        registry.register_property<VehicleComponent,
            &VehicleComponent::mode>(
            "mode",
            PropertyMeta()
                .set_display_name("Mode")
                .set_category("General"));

        registry.register_property<VehicleComponent,
            &VehicleComponent::drive_type>(
            "drive_type",
            PropertyMeta()
                .set_display_name("Drive Type")
                .set_category("General"));

        registry.register_property<VehicleComponent,
            &VehicleComponent::chassis_mass>(
            "chassis_mass",
            PropertyMeta()
                .set_display_name("Chassis Mass")
                .set_category("Chassis")
                .set_range(100.0f, 10000.0f));

        registry.register_property<VehicleComponent,
            &VehicleComponent::center_of_mass_offset>(
            "center_of_mass_offset",
            PropertyMeta()
                .set_display_name("Center of Mass Offset")
                .set_category("Chassis"));

        registry.register_property<VehicleComponent, &VehicleComponent::chassis_mass>(
            "chassis_shape_type",
            PropertyMeta()
                .set_display_name("Shape Type")
                .set_category("Chassis Shape"),
            [](const VehicleComponent& vehicle) { return get_chassis_shape_type(vehicle); },
            [](VehicleComponent& vehicle, ShapeType type) { set_chassis_shape_type(vehicle, type); });

        registry.register_property<VehicleComponent, &VehicleComponent::chassis_mass>(
            "chassis_shape_center_offset",
            PropertyMeta()
                .set_display_name("Center Offset")
                .set_category("Chassis Shape"),
            [](const VehicleComponent& vehicle) { return get_center_offset(vehicle); },
            [](VehicleComponent& vehicle, const Vec3& value) { set_center_offset(vehicle, value); });

        registry.register_property<VehicleComponent, &VehicleComponent::chassis_mass>(
            "chassis_shape_rotation_offset",
            PropertyMeta()
                .set_display_name("Rotation Offset")
                .set_category("Chassis Shape"),
            [](const VehicleComponent& vehicle) { return get_rotation_offset(vehicle); },
            [](VehicleComponent& vehicle, const Quat& value) { set_rotation_offset(vehicle, value); });

        registry.register_property<VehicleComponent, &VehicleComponent::chassis_mass>(
            "chassis_box_half_extents",
            PropertyMeta()
                .set_display_name("Box Half Extents")
                .set_category("Chassis Shape"),
            [](const VehicleComponent& vehicle) {
                return get_shape_if<BoxShapeSettings>(vehicle) ? get_shape_if<BoxShapeSettings>(vehicle)->half_extents : Vec3{0.5f};
            },
            [](VehicleComponent& vehicle, const Vec3& value) {
                if (auto* shape = std::get_if<BoxShapeSettings>(&vehicle.chassis_shape)) {
                    shape->half_extents = value;
                }
            });

        registry.register_property<VehicleComponent, &VehicleComponent::chassis_mass>(
            "chassis_sphere_radius",
            PropertyMeta()
                .set_display_name("Sphere Radius")
                .set_category("Chassis Shape")
                .set_range(0.0f, 1000.0f),
            [](const VehicleComponent& vehicle) {
                return get_shape_if<SphereShapeSettings>(vehicle) ? get_shape_if<SphereShapeSettings>(vehicle)->radius : 0.5f;
            },
            [](VehicleComponent& vehicle, float value) {
                if (auto* shape = std::get_if<SphereShapeSettings>(&vehicle.chassis_shape)) {
                    shape->radius = value;
                }
            });

        registry.register_property<VehicleComponent, &VehicleComponent::chassis_mass>(
            "chassis_capsule_radius",
            PropertyMeta()
                .set_display_name("Capsule Radius")
                .set_category("Chassis Shape")
                .set_range(0.0f, 1000.0f),
            [](const VehicleComponent& vehicle) {
                return get_shape_if<CapsuleShapeSettings>(vehicle) ? get_shape_if<CapsuleShapeSettings>(vehicle)->radius : 0.5f;
            },
            [](VehicleComponent& vehicle, float value) {
                if (auto* shape = std::get_if<CapsuleShapeSettings>(&vehicle.chassis_shape)) {
                    shape->radius = value;
                }
            });

        registry.register_property<VehicleComponent, &VehicleComponent::chassis_mass>(
            "chassis_capsule_half_height",
            PropertyMeta()
                .set_display_name("Capsule Half Height")
                .set_category("Chassis Shape")
                .set_range(0.0f, 1000.0f),
            [](const VehicleComponent& vehicle) {
                return get_shape_if<CapsuleShapeSettings>(vehicle) ? get_shape_if<CapsuleShapeSettings>(vehicle)->half_height : 0.5f;
            },
            [](VehicleComponent& vehicle, float value) {
                if (auto* shape = std::get_if<CapsuleShapeSettings>(&vehicle.chassis_shape)) {
                    shape->half_height = value;
                }
            });

        registry.register_property<VehicleComponent, &VehicleComponent::chassis_mass>(
            "chassis_cylinder_radius",
            PropertyMeta()
                .set_display_name("Cylinder Radius")
                .set_category("Chassis Shape")
                .set_range(0.0f, 1000.0f),
            [](const VehicleComponent& vehicle) {
                return get_shape_if<CylinderShapeSettings>(vehicle) ? get_shape_if<CylinderShapeSettings>(vehicle)->radius : 0.5f;
            },
            [](VehicleComponent& vehicle, float value) {
                if (auto* shape = std::get_if<CylinderShapeSettings>(&vehicle.chassis_shape)) {
                    shape->radius = value;
                }
            });

        registry.register_property<VehicleComponent, &VehicleComponent::chassis_mass>(
            "chassis_cylinder_half_height",
            PropertyMeta()
                .set_display_name("Cylinder Half Height")
                .set_category("Chassis Shape")
                .set_range(0.0f, 1000.0f),
            [](const VehicleComponent& vehicle) {
                return get_shape_if<CylinderShapeSettings>(vehicle) ? get_shape_if<CylinderShapeSettings>(vehicle)->half_height : 0.5f;
            },
            [](VehicleComponent& vehicle, float value) {
                if (auto* shape = std::get_if<CylinderShapeSettings>(&vehicle.chassis_shape)) {
                    shape->half_height = value;
                }
            });

        registry.register_property<VehicleComponent, &VehicleComponent::chassis_mass>(
            "chassis_convex_points",
            PropertyMeta()
                .set_display_name("Convex Points")
                .set_category("Chassis Shape"),
            [](const VehicleComponent& vehicle) {
                return get_shape_if<ConvexHullShapeSettings>(vehicle) ? get_shape_if<ConvexHullShapeSettings>(vehicle)->points : std::vector<Vec3>{};
            },
            [](VehicleComponent& vehicle, const std::vector<Vec3>& value) {
                if (auto* shape = std::get_if<ConvexHullShapeSettings>(&vehicle.chassis_shape)) {
                    shape->points = value;
                }
            });

        registry.register_property<VehicleComponent, &VehicleComponent::chassis_mass>(
            "chassis_mesh_vertices",
            PropertyMeta()
                .set_display_name("Mesh Vertices")
                .set_category("Chassis Shape"),
            [](const VehicleComponent& vehicle) {
                return get_shape_if<MeshShapeSettings>(vehicle) ? get_shape_if<MeshShapeSettings>(vehicle)->vertices : std::vector<Vec3>{};
            },
            [](VehicleComponent& vehicle, const std::vector<Vec3>& value) {
                if (auto* shape = std::get_if<MeshShapeSettings>(&vehicle.chassis_shape)) {
                    shape->vertices = value;
                }
            });

        registry.register_property<VehicleComponent, &VehicleComponent::chassis_mass>(
            "chassis_mesh_indices",
            PropertyMeta()
                .set_display_name("Mesh Indices")
                .set_category("Chassis Shape"),
            [](const VehicleComponent& vehicle) {
                return get_shape_if<MeshShapeSettings>(vehicle) ? get_shape_if<MeshShapeSettings>(vehicle)->indices : std::vector<uint32_t>{};
            },
            [](VehicleComponent& vehicle, const std::vector<uint32_t>& value) {
                if (auto* shape = std::get_if<MeshShapeSettings>(&vehicle.chassis_shape)) {
                    shape->indices = value;
                }
            });

        registry.register_property<VehicleComponent, &VehicleComponent::chassis_mass>(
            "chassis_heightfield_heights",
            PropertyMeta()
                .set_display_name("HeightField Heights")
                .set_category("Chassis Shape"),
            [](const VehicleComponent& vehicle) {
                return get_shape_if<HeightFieldShapeSettings>(vehicle) ? get_shape_if<HeightFieldShapeSettings>(vehicle)->heights : std::vector<float>{};
            },
            [](VehicleComponent& vehicle, const std::vector<float>& value) {
                if (auto* shape = std::get_if<HeightFieldShapeSettings>(&vehicle.chassis_shape)) {
                    shape->heights = value;
                }
            });

        registry.register_property<VehicleComponent, &VehicleComponent::chassis_mass>(
            "chassis_heightfield_num_rows",
            PropertyMeta().set_display_name("HeightField Rows").set_category("Chassis Shape"),
            [](const VehicleComponent& vehicle) {
                return get_shape_if<HeightFieldShapeSettings>(vehicle) ? get_shape_if<HeightFieldShapeSettings>(vehicle)->num_rows : 0u;
            },
            [](VehicleComponent& vehicle, uint32_t value) {
                if (auto* shape = std::get_if<HeightFieldShapeSettings>(&vehicle.chassis_shape)) {
                    shape->num_rows = value;
                }
            });

        registry.register_property<VehicleComponent, &VehicleComponent::chassis_mass>(
            "chassis_heightfield_num_cols",
            PropertyMeta().set_display_name("HeightField Cols").set_category("Chassis Shape"),
            [](const VehicleComponent& vehicle) {
                return get_shape_if<HeightFieldShapeSettings>(vehicle) ? get_shape_if<HeightFieldShapeSettings>(vehicle)->num_cols : 0u;
            },
            [](VehicleComponent& vehicle, uint32_t value) {
                if (auto* shape = std::get_if<HeightFieldShapeSettings>(&vehicle.chassis_shape)) {
                    shape->num_cols = value;
                }
            });

        registry.register_property<VehicleComponent, &VehicleComponent::chassis_mass>(
            "chassis_heightfield_scale",
            PropertyMeta().set_display_name("HeightField Scale").set_category("Chassis Shape"),
            [](const VehicleComponent& vehicle) {
                return get_shape_if<HeightFieldShapeSettings>(vehicle) ? get_shape_if<HeightFieldShapeSettings>(vehicle)->scale : Vec3{1.0f};
            },
            [](VehicleComponent& vehicle, const Vec3& value) {
                if (auto* shape = std::get_if<HeightFieldShapeSettings>(&vehicle.chassis_shape)) {
                    shape->scale = value;
                }
            });

        registry.register_property<VehicleComponent, &VehicleComponent::chassis_mass>(
            "chassis_heightfield_offset",
            PropertyMeta().set_display_name("HeightField Offset").set_category("Chassis Shape"),
            [](const VehicleComponent& vehicle) {
                return get_shape_if<HeightFieldShapeSettings>(vehicle) ? get_shape_if<HeightFieldShapeSettings>(vehicle)->offset : Vec3{0.0f};
            },
            [](VehicleComponent& vehicle, const Vec3& value) {
                if (auto* shape = std::get_if<HeightFieldShapeSettings>(&vehicle.chassis_shape)) {
                    shape->offset = value;
                }
            });

        registry.register_property<VehicleComponent,
            &VehicleComponent::layer>(
            "layer",
            PropertyMeta()
                .set_display_name("Collision Layer")
                .set_category("Collision"));

        registry.register_property<VehicleComponent,
            &VehicleComponent::wheel_collision_mask>(
            "wheel_collision_mask",
            PropertyMeta()
                .set_display_name("Wheel Collision Mask")
                .set_category("Collision"));

        registry.register_property<VehicleComponent,
            &VehicleComponent::wheels>(
            "wheels",
            PropertyMeta()
                .set_display_name("Wheels")
                .set_category("Chassis"));

        registry.register_property<VehicleComponent,
            &VehicleComponent::anti_roll_bars>(
            "anti_roll_bars",
            PropertyMeta()
                .set_display_name("Anti-Roll Bars")
                .set_category("Chassis"));

        registry.register_property<VehicleComponent,
            &VehicleComponent::arcade>(
            "arcade",
            PropertyMeta()
                .set_display_name("Arcade Settings")
                .set_category("Handling"));

        registry.register_property<VehicleComponent,
            &VehicleComponent::simulation>(
            "simulation",
            PropertyMeta()
                .set_display_name("Simulation Settings")
                .set_category("Handling"));

        registry.register_property<VehicleComponent,
            &VehicleComponent::throttle>(
            "throttle",
            PropertyMeta()
                .set_display_name("Throttle")
                .set_category("Input")
                .set_range(0.0f, 1.0f)
                .set_read_only(true));

        registry.register_property<VehicleComponent,
            &VehicleComponent::brake>(
            "brake",
            PropertyMeta()
                .set_display_name("Brake")
                .set_category("Input")
                .set_range(0.0f, 1.0f)
                .set_read_only(true));

        registry.register_property<VehicleComponent,
            &VehicleComponent::steering>(
            "steering",
            PropertyMeta()
                .set_display_name("Steering")
                .set_category("Input")
                .set_range(-1.0f, 1.0f)
                .set_read_only(true));

        TypeRegistry::instance().register_property<VehicleComponent,
            &VehicleComponent::handbrake>(
            "handbrake",
            PropertyMeta()
                .set_display_name("Handbrake")
                .set_category("Input")
                .set_read_only(true));
    }
};
static VehicleComponentRegistrar _vehicle_component_registrar;

// ArcadeVehicleSettings registration
struct ArcadeVehicleSettingsRegistrar {
    ArcadeVehicleSettingsRegistrar() {
        TypeRegistry::instance().register_type<ArcadeVehicleSettings>(
            "ArcadeVehicleSettings",
            TypeMeta()
                .set_display_name("Arcade Vehicle Settings")
                .set_description("Arcade mode vehicle parameters"));

        TypeRegistry::instance().register_property<ArcadeVehicleSettings,
            &ArcadeVehicleSettings::max_speed>(
            "max_speed",
            PropertyMeta()
                .set_display_name("Max Speed")
                .set_category("Speed")
                .set_range(5.0f, 100.0f));

        TypeRegistry::instance().register_property<ArcadeVehicleSettings,
            &ArcadeVehicleSettings::reverse_max_speed>(
            "reverse_max_speed",
            PropertyMeta()
                .set_display_name("Reverse Max Speed")
                .set_category("Speed")
                .set_range(1.0f, 30.0f));

        TypeRegistry::instance().register_property<ArcadeVehicleSettings,
            &ArcadeVehicleSettings::acceleration>(
            "acceleration",
            PropertyMeta()
                .set_display_name("Acceleration")
                .set_category("Speed")
                .set_range(1.0f, 50.0f));

        TypeRegistry::instance().register_property<ArcadeVehicleSettings,
            &ArcadeVehicleSettings::braking>(
            "braking",
            PropertyMeta()
                .set_display_name("Braking")
                .set_category("Speed")
                .set_range(5.0f, 100.0f));

        TypeRegistry::instance().register_property<ArcadeVehicleSettings,
            &ArcadeVehicleSettings::deceleration>(
            "deceleration",
            PropertyMeta()
                .set_display_name("Deceleration")
                .set_category("Speed")
                .set_range(0.5f, 20.0f));

        TypeRegistry::instance().register_property<ArcadeVehicleSettings,
            &ArcadeVehicleSettings::steering_speed>(
            "steering_speed",
            PropertyMeta()
                .set_display_name("Steering Speed")
                .set_category("Steering")
                .set_range(0.5f, 10.0f));

        TypeRegistry::instance().register_property<ArcadeVehicleSettings,
            &ArcadeVehicleSettings::steering_return_speed>(
            "steering_return_speed",
            PropertyMeta()
                .set_display_name("Steering Return Speed")
                .set_category("Steering")
                .set_range(0.5f, 10.0f));

        TypeRegistry::instance().register_property<ArcadeVehicleSettings,
            &ArcadeVehicleSettings::speed_sensitive_steering>(
            "speed_sensitive_steering",
            PropertyMeta()
                .set_display_name("Speed Sensitive Steering")
                .set_category("Steering")
                .set_range(0.0f, 1.0f));

        TypeRegistry::instance().register_property<ArcadeVehicleSettings,
            &ArcadeVehicleSettings::downforce>(
            "downforce",
            PropertyMeta()
                .set_display_name("Downforce")
                .set_category("Physics")
                .set_range(0.0f, 5.0f));

        TypeRegistry::instance().register_property<ArcadeVehicleSettings,
            &ArcadeVehicleSettings::air_control>(
            "air_control",
            PropertyMeta()
                .set_display_name("Air Control")
                .set_category("Physics")
                .set_range(0.0f, 1.0f));

        TypeRegistry::instance().register_property<ArcadeVehicleSettings,
            &ArcadeVehicleSettings::drift_factor>(
            "drift_factor",
            PropertyMeta()
                .set_display_name("Drift Factor")
                .set_category("Physics")
                .set_range(0.0f, 1.0f));

        TypeRegistry::instance().register_property<ArcadeVehicleSettings,
            &ArcadeVehicleSettings::auto_handbrake_at_low_speed>(
            "auto_handbrake_at_low_speed",
            PropertyMeta()
                .set_display_name("Auto Handbrake at Low Speed")
                .set_category("Behavior"));

        TypeRegistry::instance().register_property<ArcadeVehicleSettings,
            &ArcadeVehicleSettings::instant_reverse>(
            "instant_reverse",
            PropertyMeta()
                .set_display_name("Instant Reverse")
                .set_category("Behavior"));
    }
};
static ArcadeVehicleSettingsRegistrar _arcade_settings_registrar;

// SimulationVehicleSettings registration
struct SimulationVehicleSettingsRegistrar {
    SimulationVehicleSettingsRegistrar() {
        TypeRegistry::instance().register_type<SimulationVehicleSettings>(
            "SimulationVehicleSettings",
            TypeMeta()
                .set_display_name("Simulation Vehicle Settings")
                .set_description("Simulation mode vehicle parameters"));

        TypeRegistry::instance().register_property<SimulationVehicleSettings,
            &SimulationVehicleSettings::max_rpm>(
            "max_rpm",
            PropertyMeta()
                .set_display_name("Max RPM")
                .set_category("Engine")
                .set_range(3000.0f, 15000.0f));

        TypeRegistry::instance().register_property<SimulationVehicleSettings,
            &SimulationVehicleSettings::idle_rpm>(
            "idle_rpm",
            PropertyMeta()
                .set_display_name("Idle RPM")
                .set_category("Engine")
                .set_range(500.0f, 2000.0f));

        TypeRegistry::instance().register_property<SimulationVehicleSettings,
            &SimulationVehicleSettings::redline_rpm>(
            "redline_rpm",
            PropertyMeta()
                .set_display_name("Redline RPM")
                .set_category("Engine")
                .set_range(3000.0f, 12000.0f));

        TypeRegistry::instance().register_property<SimulationVehicleSettings,
            &SimulationVehicleSettings::max_torque>(
            "max_torque",
            PropertyMeta()
                .set_display_name("Max Torque")
                .set_category("Engine")
                .set_range(50.0f, 1000.0f));

        TypeRegistry::instance().register_property<SimulationVehicleSettings,
            &SimulationVehicleSettings::peak_torque_rpm>(
            "peak_torque_rpm",
            PropertyMeta()
                .set_display_name("Peak Torque RPM")
                .set_category("Engine")
                .set_range(1000.0f, 8000.0f));

        TypeRegistry::instance().register_property<SimulationVehicleSettings,
            &SimulationVehicleSettings::gear_ratios>(
            "gear_ratios",
            PropertyMeta()
                .set_display_name("Gear Ratios")
                .set_category("Transmission"));

        TypeRegistry::instance().register_property<SimulationVehicleSettings,
            &SimulationVehicleSettings::final_drive_ratio>(
            "final_drive_ratio",
            PropertyMeta()
                .set_display_name("Final Drive Ratio")
                .set_category("Transmission")
                .set_range(1.0f, 6.0f));

        TypeRegistry::instance().register_property<SimulationVehicleSettings,
            &SimulationVehicleSettings::shift_time>(
            "shift_time",
            PropertyMeta()
                .set_display_name("Shift Time")
                .set_category("Transmission")
                .set_range(0.05f, 1.0f));

        TypeRegistry::instance().register_property<SimulationVehicleSettings,
            &SimulationVehicleSettings::auto_transmission>(
            "auto_transmission",
            PropertyMeta()
                .set_display_name("Auto Transmission")
                .set_category("Transmission"));

        TypeRegistry::instance().register_property<SimulationVehicleSettings,
            &SimulationVehicleSettings::shift_up_rpm>(
            "shift_up_rpm",
            PropertyMeta()
                .set_display_name("Shift Up RPM")
                .set_category("Transmission")
                .set_range(2000.0f, 10000.0f));

        TypeRegistry::instance().register_property<SimulationVehicleSettings,
            &SimulationVehicleSettings::shift_down_rpm>(
            "shift_down_rpm",
            PropertyMeta()
                .set_display_name("Shift Down RPM")
                .set_category("Transmission")
                .set_range(1000.0f, 4000.0f));

        TypeRegistry::instance().register_property<SimulationVehicleSettings,
            &SimulationVehicleSettings::differential_type>(
            "differential_type",
            PropertyMeta()
                .set_display_name("Differential Type")
                .set_category("Differential"));

        TypeRegistry::instance().register_property<SimulationVehicleSettings,
            &SimulationVehicleSettings::limited_slip_ratio>(
            "limited_slip_ratio",
            PropertyMeta()
                .set_display_name("Limited Slip Ratio")
                .set_category("Differential")
                .set_range(0.0f, 1.0f));

        TypeRegistry::instance().register_property<SimulationVehicleSettings,
            &SimulationVehicleSettings::front_anti_roll>(
            "front_anti_roll",
            PropertyMeta()
                .set_display_name("Front Anti-Roll")
                .set_category("Suspension")
                .set_range(0.0f, 5000.0f));

        TypeRegistry::instance().register_property<SimulationVehicleSettings,
            &SimulationVehicleSettings::rear_anti_roll>(
            "rear_anti_roll",
            PropertyMeta()
                .set_display_name("Rear Anti-Roll")
                .set_category("Suspension")
                .set_range(0.0f, 5000.0f));

        TypeRegistry::instance().register_property<SimulationVehicleSettings,
            &SimulationVehicleSettings::clutch_strength>(
            "clutch_strength",
            PropertyMeta()
                .set_display_name("Clutch Strength")
                .set_category("Transmission")
                .set_range(1.0f, 50.0f));
    }
};
static SimulationVehicleSettingsRegistrar _simulation_settings_registrar;

// AntiRollBarSettings registration
struct AntiRollBarSettingsRegistrar {
    AntiRollBarSettingsRegistrar() {
        TypeRegistry::instance().register_type<AntiRollBarSettings>(
            "AntiRollBarSettings",
            TypeMeta()
                .set_display_name("Anti-Roll Bar Settings")
                .set_description("Links left/right wheels for roll resistance"));

        TypeRegistry::instance().register_property<AntiRollBarSettings,
            &AntiRollBarSettings::left_wheel_index>(
            "left_wheel_index",
            PropertyMeta()
                .set_display_name("Left Wheel Index")
                .set_category("Anti-Roll"));

        TypeRegistry::instance().register_property<AntiRollBarSettings,
            &AntiRollBarSettings::right_wheel_index>(
            "right_wheel_index",
            PropertyMeta()
                .set_display_name("Right Wheel Index")
                .set_category("Anti-Roll"));

        TypeRegistry::instance().register_property<AntiRollBarSettings,
            &AntiRollBarSettings::stiffness>(
            "stiffness",
            PropertyMeta()
                .set_display_name("Stiffness")
                .set_category("Anti-Roll")
                .set_range(0.0f, 100000.0f));
    }
};
static AntiRollBarSettingsRegistrar _anti_roll_bar_settings_registrar;

// WheelSettings registration
struct WheelSettingsRegistrar {
    WheelSettingsRegistrar() {
        TypeRegistry::instance().register_type<WheelSettings>(
            "WheelSettings",
            TypeMeta()
                .set_display_name("Wheel Settings")
                .set_description("Individual wheel configuration"));

        TypeRegistry::instance().register_property<WheelSettings,
            &WheelSettings::attachment_point>(
            "attachment_point",
            PropertyMeta()
                .set_display_name("Attachment Point")
                .set_category("Position"));

        TypeRegistry::instance().register_property<WheelSettings,
            &WheelSettings::wheel_direction>(
            "wheel_direction",
            PropertyMeta()
                .set_display_name("Wheel Direction")
                .set_category("Position"));

        TypeRegistry::instance().register_property<WheelSettings,
            &WheelSettings::steering_axis>(
            "steering_axis",
            PropertyMeta()
                .set_display_name("Steering Axis")
                .set_category("Position"));

        TypeRegistry::instance().register_property<WheelSettings,
            &WheelSettings::radius>(
            "radius",
            PropertyMeta()
                .set_display_name("Radius")
                .set_category("Geometry")
                .set_range(0.1f, 1.5f));

        TypeRegistry::instance().register_property<WheelSettings,
            &WheelSettings::width>(
            "width",
            PropertyMeta()
                .set_display_name("Width")
                .set_category("Geometry")
                .set_range(0.05f, 0.8f));

        TypeRegistry::instance().register_property<WheelSettings,
            &WheelSettings::suspension_min>(
            "suspension_min",
            PropertyMeta()
                .set_display_name("Suspension Min")
                .set_category("Suspension")
                .set_range(0.0f, 0.5f));

        TypeRegistry::instance().register_property<WheelSettings,
            &WheelSettings::suspension_max>(
            "suspension_max",
            PropertyMeta()
                .set_display_name("Suspension Max")
                .set_category("Suspension")
                .set_range(0.1f, 1.0f));

        TypeRegistry::instance().register_property<WheelSettings,
            &WheelSettings::suspension_stiffness>(
            "suspension_stiffness",
            PropertyMeta()
                .set_display_name("Suspension Stiffness")
                .set_category("Suspension")
                .set_range(1000.0f, 200000.0f));

        TypeRegistry::instance().register_property<WheelSettings,
            &WheelSettings::suspension_damping>(
            "suspension_damping",
            PropertyMeta()
                .set_display_name("Suspension Damping")
                .set_category("Suspension")
                .set_range(100.0f, 5000.0f));

        TypeRegistry::instance().register_property<WheelSettings,
            &WheelSettings::suspension_preload>(
            "suspension_preload",
            PropertyMeta()
                .set_display_name("Suspension Preload")
                .set_category("Suspension")
                .set_range(0.0f, 1.0f));

        TypeRegistry::instance().register_property<WheelSettings,
            &WheelSettings::longitudinal_friction>(
            "longitudinal_friction",
            PropertyMeta()
                .set_display_name("Longitudinal Friction")
                .set_category("Tire")
                .set_range(0.1f, 3.0f));

        TypeRegistry::instance().register_property<WheelSettings,
            &WheelSettings::lateral_friction>(
            "lateral_friction",
            PropertyMeta()
                .set_display_name("Lateral Friction")
                .set_category("Tire")
                .set_range(0.1f, 3.0f));

        TypeRegistry::instance().register_property<WheelSettings,
            &WheelSettings::max_steering_angle>(
            "max_steering_angle",
            PropertyMeta()
                .set_display_name("Max Steering Angle")
                .set_category("Steering")
                .set_range(0.0f, 1.0f));

        TypeRegistry::instance().register_property<WheelSettings,
            &WheelSettings::is_steerable>(
            "is_steerable",
            PropertyMeta()
                .set_display_name("Is Steerable")
                .set_category("Function"));

        TypeRegistry::instance().register_property<WheelSettings,
            &WheelSettings::is_driven>(
            "is_driven",
            PropertyMeta()
                .set_display_name("Is Driven")
                .set_category("Function"));

        TypeRegistry::instance().register_property<WheelSettings,
            &WheelSettings::has_handbrake>(
            "has_handbrake",
            PropertyMeta()
                .set_display_name("Has Handbrake")
                .set_category("Function"));

        TypeRegistry::instance().register_property<WheelSettings,
            &WheelSettings::anti_roll_bar_group>(
            "anti_roll_bar_group",
            PropertyMeta()
                .set_display_name("Anti-Roll Bar Group")
                .set_category("Function"));
    }
};
static WheelSettingsRegistrar _wheel_settings_registrar;

} // anonymous namespace

namespace engine::physics {

void ensure_vehicle_component_registration() {}

} // namespace engine::physics
