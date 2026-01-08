#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/environment/weather.hpp>

using namespace engine::environment;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// WeatherType Tests
// ============================================================================

TEST_CASE("WeatherType enum", "[environment][weather]") {
    REQUIRE(static_cast<uint8_t>(WeatherType::Clear) == 0);
    REQUIRE(static_cast<uint8_t>(WeatherType::PartlyCloudy) == 1);
    REQUIRE(static_cast<uint8_t>(WeatherType::Cloudy) == 2);
    REQUIRE(static_cast<uint8_t>(WeatherType::Overcast) == 3);
    REQUIRE(static_cast<uint8_t>(WeatherType::LightRain) == 4);
    REQUIRE(static_cast<uint8_t>(WeatherType::Rain) == 5);
    REQUIRE(static_cast<uint8_t>(WeatherType::HeavyRain) == 6);
    REQUIRE(static_cast<uint8_t>(WeatherType::Thunderstorm) == 7);
    REQUIRE(static_cast<uint8_t>(WeatherType::LightSnow) == 8);
    REQUIRE(static_cast<uint8_t>(WeatherType::Snow) == 9);
    REQUIRE(static_cast<uint8_t>(WeatherType::Blizzard) == 10);
    REQUIRE(static_cast<uint8_t>(WeatherType::Fog) == 11);
    REQUIRE(static_cast<uint8_t>(WeatherType::DenseFog) == 12);
    REQUIRE(static_cast<uint8_t>(WeatherType::Sandstorm) == 13);
    REQUIRE(static_cast<uint8_t>(WeatherType::Hail) == 14);
}

// ============================================================================
// WeatherParams Tests
// ============================================================================

TEST_CASE("WeatherParams defaults", "[environment][weather]") {
    WeatherParams params;

    REQUIRE(params.type == WeatherType::Clear);
    REQUIRE_THAT(params.cloud_coverage, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(params.precipitation_intensity, WithinAbs(0.0f, 0.001f));
    REQUIRE(params.precipitation_is_snow == false);
    REQUIRE_THAT(params.fog_density, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(params.fog_height, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(params.fog_color.x, WithinAbs(0.7f, 0.001f));
    REQUIRE_THAT(params.fog_color.y, WithinAbs(0.75f, 0.001f));
    REQUIRE_THAT(params.fog_color.z, WithinAbs(0.8f, 0.001f));
    REQUIRE_THAT(params.wind_speed, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(params.wind_direction.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(params.wind_direction.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(params.wind_direction.z, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(params.wind_gustiness, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(params.wetness, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(params.snow_accumulation, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(params.thunder_frequency, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(params.lightning_intensity, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(params.rain_volume, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(params.wind_volume, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(params.thunder_volume, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(params.temperature, WithinAbs(20.0f, 0.001f));
    REQUIRE_THAT(params.visibility, WithinAbs(1000.0f, 0.001f));
}

TEST_CASE("WeatherParams clear weather", "[environment][weather]") {
    WeatherParams params;
    params.type = WeatherType::Clear;
    params.cloud_coverage = 0.0f;
    params.precipitation_intensity = 0.0f;
    params.fog_density = 0.0f;
    params.visibility = 10000.0f;
    params.wind_speed = 2.0f;
    params.temperature = 25.0f;

    REQUIRE(params.type == WeatherType::Clear);
    REQUIRE_THAT(params.cloud_coverage, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(params.visibility, WithinAbs(10000.0f, 0.001f));
    REQUIRE_THAT(params.temperature, WithinAbs(25.0f, 0.001f));
}

TEST_CASE("WeatherParams rain weather", "[environment][weather]") {
    WeatherParams params;
    params.type = WeatherType::Rain;
    params.cloud_coverage = 0.9f;
    params.precipitation_intensity = 0.6f;
    params.precipitation_is_snow = false;
    params.wetness = 0.8f;
    params.visibility = 500.0f;
    params.rain_volume = 0.7f;
    params.wind_speed = 5.0f;
    params.wind_gustiness = 0.3f;

    REQUIRE(params.type == WeatherType::Rain);
    REQUIRE_THAT(params.cloud_coverage, WithinAbs(0.9f, 0.001f));
    REQUIRE_THAT(params.precipitation_intensity, WithinAbs(0.6f, 0.001f));
    REQUIRE_FALSE(params.precipitation_is_snow);
    REQUIRE_THAT(params.wetness, WithinAbs(0.8f, 0.001f));
    REQUIRE_THAT(params.rain_volume, WithinAbs(0.7f, 0.001f));
}

TEST_CASE("WeatherParams snow weather", "[environment][weather]") {
    WeatherParams params;
    params.type = WeatherType::Snow;
    params.cloud_coverage = 0.85f;
    params.precipitation_intensity = 0.5f;
    params.precipitation_is_snow = true;
    params.snow_accumulation = 0.3f;
    params.temperature = -5.0f;
    params.visibility = 300.0f;

    REQUIRE(params.type == WeatherType::Snow);
    REQUIRE(params.precipitation_is_snow);
    REQUIRE_THAT(params.snow_accumulation, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(params.temperature, WithinAbs(-5.0f, 0.001f));
}

TEST_CASE("WeatherParams thunderstorm", "[environment][weather]") {
    WeatherParams params;
    params.type = WeatherType::Thunderstorm;
    params.cloud_coverage = 1.0f;
    params.precipitation_intensity = 0.9f;
    params.thunder_frequency = 2.0f;  // 2 strikes per minute
    params.lightning_intensity = 1.5f;
    params.thunder_volume = 0.9f;
    params.wind_speed = 15.0f;
    params.wind_gustiness = 0.8f;

    REQUIRE(params.type == WeatherType::Thunderstorm);
    REQUIRE_THAT(params.thunder_frequency, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(params.lightning_intensity, WithinAbs(1.5f, 0.001f));
    REQUIRE_THAT(params.thunder_volume, WithinAbs(0.9f, 0.001f));
    REQUIRE_THAT(params.wind_speed, WithinAbs(15.0f, 0.001f));
}

TEST_CASE("WeatherParams fog", "[environment][weather]") {
    WeatherParams params;
    params.type = WeatherType::DenseFog;
    params.fog_density = 0.9f;
    params.fog_height = 50.0f;
    params.fog_color = Vec3{0.6f, 0.65f, 0.7f};
    params.visibility = 50.0f;

    REQUIRE(params.type == WeatherType::DenseFog);
    REQUIRE_THAT(params.fog_density, WithinAbs(0.9f, 0.001f));
    REQUIRE_THAT(params.fog_height, WithinAbs(50.0f, 0.001f));
    REQUIRE_THAT(params.visibility, WithinAbs(50.0f, 0.001f));
}

TEST_CASE("WeatherParams wind", "[environment][weather]") {
    WeatherParams params;
    params.wind_speed = 10.0f;
    params.wind_direction = Vec3{0.707f, 0.0f, 0.707f};  // NE direction
    params.wind_gustiness = 0.5f;
    params.wind_volume = 0.6f;

    REQUIRE_THAT(params.wind_speed, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(params.wind_direction.x, WithinAbs(0.707f, 0.001f));
    REQUIRE_THAT(params.wind_direction.z, WithinAbs(0.707f, 0.001f));
    REQUIRE_THAT(params.wind_gustiness, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(params.wind_volume, WithinAbs(0.6f, 0.001f));
}

// ============================================================================
// WeatherSequenceEntry Tests
// ============================================================================

TEST_CASE("WeatherSystem::WeatherSequenceEntry", "[environment][weather]") {
    WeatherSystem::WeatherSequenceEntry entry;
    entry.type = WeatherType::Rain;
    entry.duration = 300.0f;  // 5 minutes
    entry.transition_time = 30.0f;

    REQUIRE(entry.type == WeatherType::Rain);
    REQUIRE_THAT(entry.duration, WithinAbs(300.0f, 0.001f));
    REQUIRE_THAT(entry.transition_time, WithinAbs(30.0f, 0.001f));
}
