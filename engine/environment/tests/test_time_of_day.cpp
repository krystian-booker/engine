#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/environment/time_of_day.hpp>

using namespace engine::environment;
using Catch::Matchers::WithinAbs;

// ============================================================================
// TimePeriod Tests
// ============================================================================

TEST_CASE("TimePeriod enum", "[environment][time]") {
    REQUIRE(static_cast<uint8_t>(TimePeriod::Dawn) == 0);
    REQUIRE(static_cast<uint8_t>(TimePeriod::Morning) == 1);
    REQUIRE(static_cast<uint8_t>(TimePeriod::Noon) == 2);
    REQUIRE(static_cast<uint8_t>(TimePeriod::Afternoon) == 3);
    REQUIRE(static_cast<uint8_t>(TimePeriod::Dusk) == 4);
    REQUIRE(static_cast<uint8_t>(TimePeriod::Evening) == 5);
    REQUIRE(static_cast<uint8_t>(TimePeriod::Night) == 6);
    REQUIRE(static_cast<uint8_t>(TimePeriod::Midnight) == 7);
}

// ============================================================================
// TimeOfDayConfig Tests
// ============================================================================

TEST_CASE("TimeOfDayConfig defaults", "[environment][time]") {
    TimeOfDayConfig config;

    REQUIRE_THAT(config.day_length_minutes, WithinAbs(24.0f, 0.001f));
    REQUIRE_THAT(config.start_hour, WithinAbs(8.0f, 0.001f));
    REQUIRE(config.pause_in_menus == true);
    REQUIRE_THAT(config.latitude, WithinAbs(45.0f, 0.001f));
    REQUIRE(config.day_of_year == 172);  // Summer solstice
}

TEST_CASE("TimeOfDayConfig custom values", "[environment][time]") {
    TimeOfDayConfig config;
    config.day_length_minutes = 60.0f;  // 1 hour per real minute
    config.start_hour = 12.0f;          // Start at noon
    config.pause_in_menus = false;
    config.latitude = 35.0f;            // Southern latitude
    config.day_of_year = 355;           // Near winter solstice

    REQUIRE_THAT(config.day_length_minutes, WithinAbs(60.0f, 0.001f));
    REQUIRE_THAT(config.start_hour, WithinAbs(12.0f, 0.001f));
    REQUIRE_FALSE(config.pause_in_menus);
    REQUIRE_THAT(config.latitude, WithinAbs(35.0f, 0.001f));
    REQUIRE(config.day_of_year == 355);
}

TEST_CASE("TimeOfDayConfig fast day cycle", "[environment][time]") {
    TimeOfDayConfig config;
    config.day_length_minutes = 2.0f;   // 2 minutes per full day (fast for testing)
    config.start_hour = 6.0f;           // Start at dawn

    REQUIRE_THAT(config.day_length_minutes, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(config.start_hour, WithinAbs(6.0f, 0.001f));
}

TEST_CASE("TimeOfDayConfig slow day cycle", "[environment][time]") {
    TimeOfDayConfig config;
    config.day_length_minutes = 1440.0f;  // 24 hours = real time
    config.start_hour = 0.0f;             // Start at midnight

    REQUIRE_THAT(config.day_length_minutes, WithinAbs(1440.0f, 0.001f));
    REQUIRE_THAT(config.start_hour, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("TimeOfDayConfig different latitudes", "[environment][time]") {
    SECTION("Equator") {
        TimeOfDayConfig config;
        config.latitude = 0.0f;  // Equator
        REQUIRE_THAT(config.latitude, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Arctic") {
        TimeOfDayConfig config;
        config.latitude = 70.0f;  // Arctic region
        REQUIRE_THAT(config.latitude, WithinAbs(70.0f, 0.001f));
    }

    SECTION("Southern hemisphere") {
        TimeOfDayConfig config;
        config.latitude = -35.0f;  // Southern hemisphere
        REQUIRE_THAT(config.latitude, WithinAbs(-35.0f, 0.001f));
    }
}

TEST_CASE("TimeOfDayConfig different seasons", "[environment][time]") {
    SECTION("Summer solstice (northern hemisphere)") {
        TimeOfDayConfig config;
        config.day_of_year = 172;  // June 21st (approx)
        REQUIRE(config.day_of_year == 172);
    }

    SECTION("Winter solstice") {
        TimeOfDayConfig config;
        config.day_of_year = 355;  // December 21st (approx)
        REQUIRE(config.day_of_year == 355);
    }

    SECTION("Spring equinox") {
        TimeOfDayConfig config;
        config.day_of_year = 80;  // March 21st (approx)
        REQUIRE(config.day_of_year == 80);
    }

    SECTION("Fall equinox") {
        TimeOfDayConfig config;
        config.day_of_year = 265;  // September 22nd (approx)
        REQUIRE(config.day_of_year == 265);
    }
}
