#include <engine/environment/environment_components.hpp>
#include <engine/environment/weather.hpp>
#include <engine/environment/time_of_day.hpp>
#include <engine/reflect/type_registry.hpp>

namespace engine::environment {

void register_environment_types() {
    using namespace reflect;
    auto& registry = TypeRegistry::instance();

    // Register TimePeriod enum
    registry.register_enum<TimePeriod>("TimePeriod", {
        {TimePeriod::Dawn, "Dawn"},
        {TimePeriod::Morning, "Morning"},
        {TimePeriod::Noon, "Noon"},
        {TimePeriod::Afternoon, "Afternoon"},
        {TimePeriod::Dusk, "Dusk"},
        {TimePeriod::Evening, "Evening"},
        {TimePeriod::Night, "Night"},
        {TimePeriod::Midnight, "Midnight"}
    });

    // Register WeatherType enum
    registry.register_enum<WeatherType>("WeatherType", {
        {WeatherType::Clear, "Clear"},
        {WeatherType::PartlyCloudy, "PartlyCloudy"},
        {WeatherType::Cloudy, "Cloudy"},
        {WeatherType::Overcast, "Overcast"},
        {WeatherType::LightRain, "LightRain"},
        {WeatherType::Rain, "Rain"},
        {WeatherType::HeavyRain, "HeavyRain"},
        {WeatherType::Thunderstorm, "Thunderstorm"},
        {WeatherType::LightSnow, "LightSnow"},
        {WeatherType::Snow, "Snow"},
        {WeatherType::Blizzard, "Blizzard"},
        {WeatherType::Fog, "Fog"},
        {WeatherType::DenseFog, "DenseFog"},
        {WeatherType::Sandstorm, "Sandstorm"},
        {WeatherType::Hail, "Hail"}
    });

    // Register WeatherZone::Shape enum
    registry.register_enum<WeatherZone::Shape>("WeatherZone::Shape", {
        {WeatherZone::Shape::Box, "Box"},
        {WeatherZone::Shape::Sphere, "Sphere"},
        {WeatherZone::Shape::Capsule, "Capsule"}
    });

    // Register IndoorVolume::Shape enum
    registry.register_enum<IndoorVolume::Shape>("IndoorVolume::Shape", {
        {IndoorVolume::Shape::Box, "Box"},
        {IndoorVolume::Shape::Sphere, "Sphere"}
    });

    // Register WeatherZone component
    registry.register_component<WeatherZone>("WeatherZone",
        TypeMeta().set_display_name("Weather Zone").set_category(TypeCategory::Component));

    registry.register_property<WeatherZone, &WeatherZone::blend_distance>("blend_distance",
        PropertyMeta().set_display_name("Blend Distance").set_range(0.0f, 100.0f));
    registry.register_property<WeatherZone, &WeatherZone::shape>("shape",
        PropertyMeta().set_display_name("Shape"));
    registry.register_property<WeatherZone, &WeatherZone::priority>("priority",
        PropertyMeta().set_display_name("Priority"));
    registry.register_property<WeatherZone, &WeatherZone::override_time>("override_time",
        PropertyMeta().set_display_name("Override Time"));
    registry.register_property<WeatherZone, &WeatherZone::forced_hour>("forced_hour",
        PropertyMeta().set_display_name("Forced Hour").set_range(0.0f, 24.0f));
    registry.register_property<WeatherZone, &WeatherZone::enter_transition_time>("enter_transition_time",
        PropertyMeta().set_display_name("Enter Transition Time").set_range(0.0f, 30.0f));
    registry.register_property<WeatherZone, &WeatherZone::exit_transition_time>("exit_transition_time",
        PropertyMeta().set_display_name("Exit Transition Time").set_range(0.0f, 30.0f));
    registry.register_property<WeatherZone, &WeatherZone::enabled>("enabled",
        PropertyMeta().set_display_name("Enabled"));

    // Register IndoorVolume component
    registry.register_component<IndoorVolume>("IndoorVolume",
        TypeMeta().set_display_name("Indoor Volume").set_category(TypeCategory::Component));

    registry.register_property<IndoorVolume, &IndoorVolume::audio_dampening>("audio_dampening",
        PropertyMeta().set_display_name("Audio Dampening").set_range(0.0f, 1.0f));
    registry.register_property<IndoorVolume, &IndoorVolume::lowpass_cutoff>("lowpass_cutoff",
        PropertyMeta().set_display_name("Lowpass Cutoff").set_range(100.0f, 20000.0f));
    registry.register_property<IndoorVolume, &IndoorVolume::block_precipitation>("block_precipitation",
        PropertyMeta().set_display_name("Block Precipitation"));
    registry.register_property<IndoorVolume, &IndoorVolume::block_wind>("block_wind",
        PropertyMeta().set_display_name("Block Wind"));
    registry.register_property<IndoorVolume, &IndoorVolume::reduce_ambient_light>("reduce_ambient_light",
        PropertyMeta().set_display_name("Reduce Ambient Light"));
    registry.register_property<IndoorVolume, &IndoorVolume::ambient_reduction>("ambient_reduction",
        PropertyMeta().set_display_name("Ambient Reduction").set_range(0.0f, 1.0f));
    registry.register_property<IndoorVolume, &IndoorVolume::shape>("shape",
        PropertyMeta().set_display_name("Shape"));
    registry.register_property<IndoorVolume, &IndoorVolume::enabled>("enabled",
        PropertyMeta().set_display_name("Enabled"));

    // Register WeatherReactive component
    registry.register_component<WeatherReactive>("WeatherReactive",
        TypeMeta().set_display_name("Weather Reactive").set_category(TypeCategory::Component));

    registry.register_property<WeatherReactive, &WeatherReactive::affected_by_wetness>("affected_by_wetness",
        PropertyMeta().set_display_name("Affected By Wetness"));
    registry.register_property<WeatherReactive, &WeatherReactive::wetness_roughness_reduction>("wetness_roughness_reduction",
        PropertyMeta().set_display_name("Wetness Roughness Reduction").set_range(0.0f, 1.0f));
    registry.register_property<WeatherReactive, &WeatherReactive::wetness_darkening>("wetness_darkening",
        PropertyMeta().set_display_name("Wetness Darkening").set_range(0.0f, 1.0f));
    registry.register_property<WeatherReactive, &WeatherReactive::can_accumulate_snow>("can_accumulate_snow",
        PropertyMeta().set_display_name("Can Accumulate Snow"));
    registry.register_property<WeatherReactive, &WeatherReactive::snow_accumulation_rate>("snow_accumulation_rate",
        PropertyMeta().set_display_name("Snow Accumulation Rate").set_range(0.0f, 1.0f));
    registry.register_property<WeatherReactive, &WeatherReactive::snow_melt_rate>("snow_melt_rate",
        PropertyMeta().set_display_name("Snow Melt Rate").set_range(0.0f, 1.0f));
    registry.register_property<WeatherReactive, &WeatherReactive::current_wetness>("current_wetness",
        PropertyMeta().set_display_name("Current Wetness").set_read_only(true));
    registry.register_property<WeatherReactive, &WeatherReactive::current_snow>("current_snow",
        PropertyMeta().set_display_name("Current Snow").set_read_only(true));

    // Register WindAffected component
    registry.register_component<WindAffected>("WindAffected",
        TypeMeta().set_display_name("Wind Affected").set_category(TypeCategory::Component));

    registry.register_property<WindAffected, &WindAffected::wind_strength_multiplier>("wind_strength_multiplier",
        PropertyMeta().set_display_name("Wind Strength Multiplier").set_range(0.0f, 5.0f));
    registry.register_property<WindAffected, &WindAffected::local_wind_offset>("local_wind_offset",
        PropertyMeta().set_display_name("Local Wind Offset"));
    registry.register_property<WindAffected, &WindAffected::oscillation_frequency>("oscillation_frequency",
        PropertyMeta().set_display_name("Oscillation Frequency").set_range(0.0f, 10.0f));
    registry.register_property<WindAffected, &WindAffected::oscillation_amplitude>("oscillation_amplitude",
        PropertyMeta().set_display_name("Oscillation Amplitude").set_range(0.0f, 1.0f));
    registry.register_property<WindAffected, &WindAffected::inertia>("inertia",
        PropertyMeta().set_display_name("Inertia").set_range(0.1f, 10.0f));
    registry.register_property<WindAffected, &WindAffected::enabled>("enabled",
        PropertyMeta().set_display_name("Enabled"));

    // Register LightningAttractor component
    registry.register_component<LightningAttractor>("LightningAttractor",
        TypeMeta().set_display_name("Lightning Attractor").set_category(TypeCategory::Component));

    registry.register_property<LightningAttractor, &LightningAttractor::attraction_radius>("attraction_radius",
        PropertyMeta().set_display_name("Attraction Radius").set_range(1.0f, 500.0f));
    registry.register_property<LightningAttractor, &LightningAttractor::attraction_strength>("attraction_strength",
        PropertyMeta().set_display_name("Attraction Strength").set_range(0.0f, 10.0f));
    registry.register_property<LightningAttractor, &LightningAttractor::use_height_bonus>("use_height_bonus",
        PropertyMeta().set_display_name("Use Height Bonus"));
    registry.register_property<LightningAttractor, &LightningAttractor::strike_cooldown>("strike_cooldown",
        PropertyMeta().set_display_name("Strike Cooldown").set_range(0.0f, 60.0f));

    // Register EnvironmentProbe component
    registry.register_component<EnvironmentProbe>("EnvironmentProbe",
        TypeMeta().set_display_name("Environment Probe").set_category(TypeCategory::Component));

    registry.register_property<EnvironmentProbe, &EnvironmentProbe::temperature>("temperature",
        PropertyMeta().set_display_name("Temperature").set_read_only(true));
    registry.register_property<EnvironmentProbe, &EnvironmentProbe::wetness>("wetness",
        PropertyMeta().set_display_name("Wetness").set_read_only(true));
    registry.register_property<EnvironmentProbe, &EnvironmentProbe::wind_speed>("wind_speed",
        PropertyMeta().set_display_name("Wind Speed").set_read_only(true));
    registry.register_property<EnvironmentProbe, &EnvironmentProbe::light_intensity>("light_intensity",
        PropertyMeta().set_display_name("Light Intensity").set_read_only(true));
    registry.register_property<EnvironmentProbe, &EnvironmentProbe::is_indoor>("is_indoor",
        PropertyMeta().set_display_name("Is Indoor").set_read_only(true));
    registry.register_property<EnvironmentProbe, &EnvironmentProbe::update_interval>("update_interval",
        PropertyMeta().set_display_name("Update Interval").set_range(0.1f, 5.0f));
    registry.register_property<EnvironmentProbe, &EnvironmentProbe::enabled>("enabled",
        PropertyMeta().set_display_name("Enabled"));
}

} // namespace engine::environment
