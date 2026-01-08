#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/audio/audio_engine.hpp>

using namespace engine::audio;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("ReverbPreset enum", "[audio][engine]") {
    REQUIRE(static_cast<uint8_t>(ReverbPreset::None) == 0);
    REQUIRE(static_cast<uint8_t>(ReverbPreset::SmallRoom) == 1);
    REQUIRE(static_cast<uint8_t>(ReverbPreset::MediumRoom) == 2);
    REQUIRE(static_cast<uint8_t>(ReverbPreset::LargeRoom) == 3);
    REQUIRE(static_cast<uint8_t>(ReverbPreset::Hall) == 4);
    REQUIRE(static_cast<uint8_t>(ReverbPreset::Cathedral) == 5);
    REQUIRE(static_cast<uint8_t>(ReverbPreset::Cave) == 6);
    REQUIRE(static_cast<uint8_t>(ReverbPreset::Underwater) == 7);
    REQUIRE(static_cast<uint8_t>(ReverbPreset::Bathroom) == 8);
    REQUIRE(static_cast<uint8_t>(ReverbPreset::Arena) == 9);
    REQUIRE(static_cast<uint8_t>(ReverbPreset::Forest) == 10);
    REQUIRE(static_cast<uint8_t>(ReverbPreset::Custom) == 11);
}

TEST_CASE("ReverbParams defaults", "[audio][engine]") {
    AudioEngine::ReverbParams params;

    REQUIRE_THAT(params.room_size, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(params.damping, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(params.width, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(params.wet_volume, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(params.dry_volume, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(params.mode, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("ReverbParams custom values", "[audio][engine]") {
    AudioEngine::ReverbParams params;
    params.room_size = 0.8f;
    params.damping = 0.3f;
    params.width = 0.5f;
    params.wet_volume = 0.6f;
    params.dry_volume = 0.8f;
    params.mode = 1.0f;

    REQUIRE_THAT(params.room_size, WithinAbs(0.8f, 0.001f));
    REQUIRE_THAT(params.damping, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(params.width, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(params.wet_volume, WithinAbs(0.6f, 0.001f));
    REQUIRE_THAT(params.dry_volume, WithinAbs(0.8f, 0.001f));
    REQUIRE_THAT(params.mode, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("FilterParams defaults", "[audio][engine]") {
    AudioEngine::FilterParams params;

    REQUIRE_THAT(params.lowpass_cutoff, WithinAbs(20000.0f, 0.1f));
    REQUIRE_THAT(params.highpass_cutoff, WithinAbs(20.0f, 0.1f));
    REQUIRE(params.lowpass_enabled == false);
    REQUIRE(params.highpass_enabled == false);
}

TEST_CASE("FilterParams custom values", "[audio][engine]") {
    AudioEngine::FilterParams params;
    params.lowpass_cutoff = 5000.0f;
    params.highpass_cutoff = 100.0f;
    params.lowpass_enabled = true;
    params.highpass_enabled = true;

    REQUIRE_THAT(params.lowpass_cutoff, WithinAbs(5000.0f, 0.1f));
    REQUIRE_THAT(params.highpass_cutoff, WithinAbs(100.0f, 0.1f));
    REQUIRE(params.lowpass_enabled == true);
    REQUIRE(params.highpass_enabled == true);
}

TEST_CASE("AudioEngine get_reverb_preset", "[audio][engine]") {
    SECTION("None preset") {
        auto params = AudioEngine::get_reverb_preset(ReverbPreset::None);
        REQUIRE_THAT(params.wet_volume, WithinAbs(0.0f, 0.01f));
    }

    SECTION("Hall preset") {
        auto params = AudioEngine::get_reverb_preset(ReverbPreset::Hall);
        // Hall should have larger room size
        REQUIRE(params.room_size > 0.5f);
    }

    SECTION("Cathedral preset") {
        auto params = AudioEngine::get_reverb_preset(ReverbPreset::Cathedral);
        // Cathedral should have large room size and longer decay
        REQUIRE(params.room_size > 0.7f);
    }

    SECTION("Bathroom preset") {
        auto params = AudioEngine::get_reverb_preset(ReverbPreset::Bathroom);
        // Bathroom should be smaller
        REQUIRE(params.room_size < 0.5f);
    }
}

// Note: Full AudioEngine tests require audio device initialization.
// These tests cover the data structures and static methods.
// Integration tests would require proper audio device setup.
