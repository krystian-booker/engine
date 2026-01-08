#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/environment/environment_components.hpp>

using namespace engine::environment;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// WeatherZone Tests
// ============================================================================

TEST_CASE("WeatherZone Shape enum", "[environment][component]") {
    REQUIRE(static_cast<uint8_t>(WeatherZone::Shape::Box) == 0);
    REQUIRE(static_cast<uint8_t>(WeatherZone::Shape::Sphere) == 1);
    REQUIRE(static_cast<uint8_t>(WeatherZone::Shape::Capsule) == 2);
}

TEST_CASE("WeatherZone defaults", "[environment][component]") {
    WeatherZone zone;

    REQUIRE_THAT(zone.blend_distance, WithinAbs(10.0f, 0.001f));
    REQUIRE(zone.shape == WeatherZone::Shape::Box);
    REQUIRE(zone.priority == 0);
    REQUIRE(zone.override_time == false);
    REQUIRE_THAT(zone.forced_hour, WithinAbs(12.0f, 0.001f));
    REQUIRE_THAT(zone.enter_transition_time, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(zone.exit_transition_time, WithinAbs(2.0f, 0.001f));
    REQUIRE(zone.enabled == true);
}

TEST_CASE("WeatherZone cave configuration", "[environment][component]") {
    WeatherZone zone;
    zone.override_params.type = WeatherType::Clear;
    zone.override_params.fog_density = 0.5f;
    zone.override_params.visibility = 100.0f;
    zone.shape = WeatherZone::Shape::Box;
    zone.priority = 10;
    zone.override_time = true;
    zone.forced_hour = 12.0f;  // Always noon lighting
    zone.blend_distance = 5.0f;

    REQUIRE(zone.override_params.type == WeatherType::Clear);
    REQUIRE_THAT(zone.override_params.fog_density, WithinAbs(0.5f, 0.001f));
    REQUIRE(zone.override_time);
    REQUIRE_THAT(zone.forced_hour, WithinAbs(12.0f, 0.001f));
}

// ============================================================================
// IndoorVolume Tests
// ============================================================================

TEST_CASE("IndoorVolume Shape enum", "[environment][component]") {
    REQUIRE(static_cast<uint8_t>(IndoorVolume::Shape::Box) == 0);
    REQUIRE(static_cast<uint8_t>(IndoorVolume::Shape::Sphere) == 1);
}

TEST_CASE("IndoorVolume defaults", "[environment][component]") {
    IndoorVolume volume;

    REQUIRE_THAT(volume.audio_dampening, WithinAbs(0.8f, 0.001f));
    REQUIRE_THAT(volume.lowpass_cutoff, WithinAbs(1000.0f, 0.001f));
    REQUIRE(volume.block_precipitation == true);
    REQUIRE(volume.block_wind == true);
    REQUIRE(volume.reduce_ambient_light == false);
    REQUIRE_THAT(volume.ambient_reduction, WithinAbs(0.3f, 0.001f));
    REQUIRE(volume.shape == IndoorVolume::Shape::Box);
    REQUIRE(volume.enabled == true);
}

TEST_CASE("IndoorVolume building configuration", "[environment][component]") {
    IndoorVolume volume;
    volume.audio_dampening = 0.9f;
    volume.lowpass_cutoff = 800.0f;
    volume.block_precipitation = true;
    volume.block_wind = true;
    volume.reduce_ambient_light = true;
    volume.ambient_reduction = 0.5f;
    volume.shape = IndoorVolume::Shape::Box;

    REQUIRE_THAT(volume.audio_dampening, WithinAbs(0.9f, 0.001f));
    REQUIRE(volume.reduce_ambient_light);
    REQUIRE_THAT(volume.ambient_reduction, WithinAbs(0.5f, 0.001f));
}

// ============================================================================
// TimeOfDayListener Tests
// ============================================================================

TEST_CASE("TimeOfDayListener defaults", "[environment][component]") {
    TimeOfDayListener listener;

    REQUIRE_FALSE(listener.on_period_change);
    REQUIRE_FALSE(listener.on_update);
    REQUIRE(listener.hour_triggers.empty());
    REQUIRE(listener.enabled == true);
}

TEST_CASE("TimeOfDayListener HourTrigger", "[environment][component]") {
    TimeOfDayListener::HourTrigger trigger;
    trigger.hour = 6.0f;
    trigger.triggered_today = false;

    REQUIRE_THAT(trigger.hour, WithinAbs(6.0f, 0.001f));
    REQUIRE_FALSE(trigger.triggered_today);
}

TEST_CASE("TimeOfDayListener with hour triggers", "[environment][component]") {
    TimeOfDayListener listener;

    TimeOfDayListener::HourTrigger dawn_trigger;
    dawn_trigger.hour = 6.0f;
    dawn_trigger.triggered_today = false;

    TimeOfDayListener::HourTrigger noon_trigger;
    noon_trigger.hour = 12.0f;
    noon_trigger.triggered_today = false;

    listener.hour_triggers.push_back(dawn_trigger);
    listener.hour_triggers.push_back(noon_trigger);

    REQUIRE(listener.hour_triggers.size() == 2);
    REQUIRE_THAT(listener.hour_triggers[0].hour, WithinAbs(6.0f, 0.001f));
    REQUIRE_THAT(listener.hour_triggers[1].hour, WithinAbs(12.0f, 0.001f));
}

// ============================================================================
// WeatherReactive Tests
// ============================================================================

TEST_CASE("WeatherReactive defaults", "[environment][component]") {
    WeatherReactive reactive;

    REQUIRE(reactive.affected_by_wetness == true);
    REQUIRE_THAT(reactive.wetness_roughness_reduction, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(reactive.wetness_darkening, WithinAbs(0.1f, 0.001f));
    REQUIRE(reactive.can_accumulate_snow == false);
    REQUIRE_THAT(reactive.snow_accumulation_rate, WithinAbs(0.1f, 0.001f));
    REQUIRE_THAT(reactive.snow_melt_rate, WithinAbs(0.05f, 0.001f));
    REQUIRE_THAT(reactive.current_wetness, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(reactive.current_snow, WithinAbs(0.0f, 0.001f));
    REQUIRE(reactive.wetness_param == "_Wetness");
    REQUIRE(reactive.snow_param == "_SnowAmount");
}

TEST_CASE("WeatherReactive snow surface", "[environment][component]") {
    WeatherReactive reactive;
    reactive.can_accumulate_snow = true;
    reactive.snow_accumulation_rate = 0.2f;
    reactive.snow_melt_rate = 0.1f;
    reactive.current_snow = 0.5f;

    REQUIRE(reactive.can_accumulate_snow);
    REQUIRE_THAT(reactive.snow_accumulation_rate, WithinAbs(0.2f, 0.001f));
    REQUIRE_THAT(reactive.current_snow, WithinAbs(0.5f, 0.001f));
}

// ============================================================================
// WindAffected Tests
// ============================================================================

TEST_CASE("WindAffected defaults", "[environment][component]") {
    WindAffected wind;

    REQUIRE_THAT(wind.wind_strength_multiplier, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(wind.local_wind_offset.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(wind.local_wind_offset.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(wind.local_wind_offset.z, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(wind.oscillation_frequency, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(wind.oscillation_amplitude, WithinAbs(0.1f, 0.001f));
    REQUIRE_THAT(wind.inertia, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(wind.current_wind_effect.x, WithinAbs(0.0f, 0.001f));
    REQUIRE(wind.affected_bones.empty());
    REQUIRE(wind.enabled == true);
}

TEST_CASE("WindAffected tree configuration", "[environment][component]") {
    WindAffected wind;
    wind.wind_strength_multiplier = 0.8f;
    wind.oscillation_frequency = 0.5f;
    wind.oscillation_amplitude = 0.2f;
    wind.inertia = 2.0f;  // Heavy tree
    wind.affected_bones = {"trunk", "branch_1", "branch_2", "leaves"};

    REQUIRE_THAT(wind.wind_strength_multiplier, WithinAbs(0.8f, 0.001f));
    REQUIRE_THAT(wind.oscillation_frequency, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(wind.inertia, WithinAbs(2.0f, 0.001f));
    REQUIRE(wind.affected_bones.size() == 4);
}

TEST_CASE("WindAffected flag configuration", "[environment][component]") {
    WindAffected wind;
    wind.wind_strength_multiplier = 1.5f;  // Very responsive
    wind.oscillation_frequency = 2.0f;
    wind.oscillation_amplitude = 0.5f;
    wind.inertia = 0.3f;  // Light cloth

    REQUIRE_THAT(wind.wind_strength_multiplier, WithinAbs(1.5f, 0.001f));
    REQUIRE_THAT(wind.inertia, WithinAbs(0.3f, 0.001f));
}

// ============================================================================
// LightningAttractor Tests
// ============================================================================

TEST_CASE("LightningAttractor defaults", "[environment][component]") {
    LightningAttractor attractor;

    REQUIRE_THAT(attractor.attraction_radius, WithinAbs(50.0f, 0.001f));
    REQUIRE_THAT(attractor.attraction_strength, WithinAbs(1.0f, 0.001f));
    REQUIRE(attractor.use_height_bonus == true);
    REQUIRE_FALSE(attractor.on_strike);
    REQUIRE_THAT(attractor.strike_cooldown, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(attractor.time_since_last_strike, WithinAbs(999.0f, 0.001f));
}

TEST_CASE("LightningAttractor tower configuration", "[environment][component]") {
    LightningAttractor attractor;
    attractor.attraction_radius = 100.0f;
    attractor.attraction_strength = 2.0f;  // High attraction
    attractor.use_height_bonus = true;
    attractor.strike_cooldown = 5.0f;

    REQUIRE_THAT(attractor.attraction_radius, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(attractor.attraction_strength, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(attractor.strike_cooldown, WithinAbs(5.0f, 0.001f));
}

// ============================================================================
// EnvironmentProbe Tests
// ============================================================================

TEST_CASE("EnvironmentProbe defaults", "[environment][component]") {
    EnvironmentProbe probe;

    REQUIRE_THAT(probe.temperature, WithinAbs(20.0f, 0.001f));
    REQUIRE_THAT(probe.wetness, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(probe.wind_speed, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(probe.wind_direction.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(probe.light_intensity, WithinAbs(1.0f, 0.001f));
    REQUIRE(probe.is_indoor == false);
    REQUIRE_THAT(probe.update_interval, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(probe.time_since_update, WithinAbs(0.0f, 0.001f));
    REQUIRE(probe.enabled == true);
}

TEST_CASE("EnvironmentProbe outdoor configuration", "[environment][component]") {
    EnvironmentProbe probe;
    probe.temperature = 15.0f;
    probe.wetness = 0.7f;
    probe.wind_speed = 5.0f;
    probe.wind_direction = Vec3{1.0f, 0.0f, 0.0f};
    probe.light_intensity = 0.8f;
    probe.is_indoor = false;

    REQUIRE_THAT(probe.temperature, WithinAbs(15.0f, 0.001f));
    REQUIRE_THAT(probe.wetness, WithinAbs(0.7f, 0.001f));
    REQUIRE_THAT(probe.wind_speed, WithinAbs(5.0f, 0.001f));
    REQUIRE_FALSE(probe.is_indoor);
}

TEST_CASE("EnvironmentProbe indoor configuration", "[environment][component]") {
    EnvironmentProbe probe;
    probe.temperature = 22.0f;  // Warm indoors
    probe.wetness = 0.0f;
    probe.wind_speed = 0.0f;
    probe.light_intensity = 0.5f;  // Dimmer indoors
    probe.is_indoor = true;
    probe.update_interval = 1.0f;  // Less frequent updates indoors

    REQUIRE_THAT(probe.temperature, WithinAbs(22.0f, 0.001f));
    REQUIRE_THAT(probe.wetness, WithinAbs(0.0f, 0.001f));
    REQUIRE(probe.is_indoor);
    REQUIRE_THAT(probe.update_interval, WithinAbs(1.0f, 0.001f));
}
