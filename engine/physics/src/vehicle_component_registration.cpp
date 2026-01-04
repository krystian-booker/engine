#include <engine/physics/vehicle_component.hpp>
#include <engine/physics/vehicle.hpp>
#include <engine/reflect/reflect.hpp>

namespace {

using namespace engine::physics;
using namespace engine::reflect;

// VehicleComponent registration
struct VehicleComponentRegistrar {
    VehicleComponentRegistrar() {
        TypeRegistry::instance().register_component<VehicleComponent>(
            "VehicleComponent",
            TypeMeta()
                .set_display_name("Vehicle")
                .set_description("Vehicle physics configuration"));

        TypeRegistry::instance().register_property<VehicleComponent,
            &VehicleComponent::type>(
            "type",
            PropertyMeta()
                .set_display_name("Type")
                .set_category("General"));

        TypeRegistry::instance().register_property<VehicleComponent,
            &VehicleComponent::mode>(
            "mode",
            PropertyMeta()
                .set_display_name("Mode")
                .set_category("General"));

        TypeRegistry::instance().register_property<VehicleComponent,
            &VehicleComponent::drive_type>(
            "drive_type",
            PropertyMeta()
                .set_display_name("Drive Type")
                .set_category("General"));

        TypeRegistry::instance().register_property<VehicleComponent,
            &VehicleComponent::chassis_mass>(
            "chassis_mass",
            PropertyMeta()
                .set_display_name("Chassis Mass")
                .set_category("Chassis")
                .set_range(100.0f, 10000.0f));

        TypeRegistry::instance().register_property<VehicleComponent,
            &VehicleComponent::center_of_mass_offset>(
            "center_of_mass_offset",
            PropertyMeta()
                .set_display_name("Center of Mass Offset")
                .set_category("Chassis"));

        TypeRegistry::instance().register_property<VehicleComponent,
            &VehicleComponent::layer>(
            "layer",
            PropertyMeta()
                .set_display_name("Collision Layer")
                .set_category("Collision"));

        TypeRegistry::instance().register_property<VehicleComponent,
            &VehicleComponent::throttle>(
            "throttle",
            PropertyMeta()
                .set_display_name("Throttle")
                .set_category("Input")
                .set_range(0.0f, 1.0f)
                .set_read_only(true));

        TypeRegistry::instance().register_property<VehicleComponent,
            &VehicleComponent::brake>(
            "brake",
            PropertyMeta()
                .set_display_name("Brake")
                .set_category("Input")
                .set_range(0.0f, 1.0f)
                .set_read_only(true));

        TypeRegistry::instance().register_property<VehicleComponent,
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
    }
};
static WheelSettingsRegistrar _wheel_settings_registrar;

} // anonymous namespace
