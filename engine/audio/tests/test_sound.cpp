#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/audio/sound.hpp>

using namespace engine::audio;
using Catch::Matchers::WithinAbs;

TEST_CASE("AudioError enum", "[audio][sound]") {
    REQUIRE(static_cast<uint8_t>(AudioError::None) == 0);
    REQUIRE(static_cast<uint8_t>(AudioError::FileNotFound) == 1);
    REQUIRE(static_cast<uint8_t>(AudioError::InvalidFormat) == 2);
    REQUIRE(static_cast<uint8_t>(AudioError::DecodingFailed) == 3);
    REQUIRE(static_cast<uint8_t>(AudioError::DeviceError) == 4);
    REQUIRE(static_cast<uint8_t>(AudioError::OutOfMemory) == 5);
    REQUIRE(static_cast<uint8_t>(AudioError::InvalidHandle) == 6);
    REQUIRE(static_cast<uint8_t>(AudioError::PlaybackFailed) == 7);
    REQUIRE(static_cast<uint8_t>(AudioError::Unknown) == 8);
}

TEST_CASE("AudioResult", "[audio][sound]") {
    SECTION("Default is ok") {
        AudioResult result;
        REQUIRE(result.ok() == true);
        REQUIRE(result.error == AudioError::None);
        REQUIRE(result.message.empty());
        REQUIRE(static_cast<bool>(result) == true);
    }

    SECTION("Error result") {
        AudioResult result;
        result.error = AudioError::FileNotFound;
        result.message = "File not found: test.wav";

        REQUIRE(result.ok() == false);
        REQUIRE(result.error == AudioError::FileNotFound);
        REQUIRE(result.message == "File not found: test.wav");
        REQUIRE(static_cast<bool>(result) == false);
    }
}

TEST_CASE("SoundHandle", "[audio][sound]") {
    SECTION("Default is invalid") {
        SoundHandle handle;
        REQUIRE_FALSE(handle.valid());
        REQUIRE(handle.id == UINT32_MAX);
    }

    SECTION("Valid handle") {
        SoundHandle handle;
        handle.id = 42;
        REQUIRE(handle.valid());
    }
}

TEST_CASE("MusicHandle", "[audio][sound]") {
    SECTION("Default is invalid") {
        MusicHandle handle;
        REQUIRE_FALSE(handle.valid());
        REQUIRE(handle.id == UINT32_MAX);
    }

    SECTION("Valid handle") {
        MusicHandle handle;
        handle.id = 100;
        REQUIRE(handle.valid());
    }
}

TEST_CASE("AudioBusHandle", "[audio][sound]") {
    SECTION("Default is invalid") {
        AudioBusHandle handle;
        REQUIRE_FALSE(handle.valid());
        REQUIRE(handle.id == UINT32_MAX);
    }

    SECTION("Valid handle") {
        AudioBusHandle handle;
        handle.id = 5;
        REQUIRE(handle.valid());
    }
}

TEST_CASE("BuiltinBus enum", "[audio][sound]") {
    REQUIRE(static_cast<uint32_t>(BuiltinBus::Master) == 0);
    REQUIRE(static_cast<uint32_t>(BuiltinBus::Music) == 1);
    REQUIRE(static_cast<uint32_t>(BuiltinBus::SFX) == 2);
    REQUIRE(static_cast<uint32_t>(BuiltinBus::Voice) == 3);
    REQUIRE(static_cast<uint32_t>(BuiltinBus::Ambient) == 4);
    REQUIRE(static_cast<uint32_t>(BuiltinBus::UI) == 5);
}

TEST_CASE("PlaybackState enum", "[audio][sound]") {
    REQUIRE(static_cast<uint8_t>(PlaybackState::Stopped) == 0);
    REQUIRE(static_cast<uint8_t>(PlaybackState::Playing) == 1);
    REQUIRE(static_cast<uint8_t>(PlaybackState::Paused) == 2);
}

TEST_CASE("SoundConfig defaults", "[audio][sound]") {
    SoundConfig config;

    REQUIRE_THAT(config.volume, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(config.pitch, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(config.pan, WithinAbs(0.0f, 0.001f));
    REQUIRE(config.loop == false);
    REQUIRE(config.spatial == false);
    REQUIRE_FALSE(config.bus.valid());
    REQUIRE_THAT(config.priority, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("SoundConfig custom values", "[audio][sound]") {
    SoundConfig config;
    config.volume = 0.5f;
    config.pitch = 1.2f;
    config.pan = -0.5f;
    config.loop = true;
    config.spatial = true;
    config.priority = 2.0f;

    REQUIRE_THAT(config.volume, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(config.pitch, WithinAbs(1.2f, 0.001f));
    REQUIRE_THAT(config.pan, WithinAbs(-0.5f, 0.001f));
    REQUIRE(config.loop == true);
    REQUIRE(config.spatial == true);
    REQUIRE_THAT(config.priority, WithinAbs(2.0f, 0.001f));
}

TEST_CASE("SpatialConfig defaults", "[audio][sound]") {
    SpatialConfig config;

    REQUIRE_THAT(config.min_distance, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(config.max_distance, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(config.rolloff_factor, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("SpatialConfig custom values", "[audio][sound]") {
    SpatialConfig config;
    config.min_distance = 5.0f;
    config.max_distance = 50.0f;
    config.rolloff_factor = 2.0f;

    REQUIRE_THAT(config.min_distance, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(config.max_distance, WithinAbs(50.0f, 0.001f));
    REQUIRE_THAT(config.rolloff_factor, WithinAbs(2.0f, 0.001f));
}
