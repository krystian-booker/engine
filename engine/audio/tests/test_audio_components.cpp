#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/audio/audio_components.hpp>

using namespace engine::audio;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("AttenuationModel enum", "[audio][components]") {
    REQUIRE(static_cast<uint8_t>(AttenuationModel::None) == 0);
    REQUIRE(static_cast<uint8_t>(AttenuationModel::Linear) == 1);
    REQUIRE(static_cast<uint8_t>(AttenuationModel::InverseSquare) == 2);
    REQUIRE(static_cast<uint8_t>(AttenuationModel::Logarithmic) == 3);
}

TEST_CASE("AudioSource defaults", "[audio][components]") {
    AudioSource source;

    REQUIRE_FALSE(source.sound.valid());
    REQUIRE(source.playing == false);
    REQUIRE(source.loop == false);
    REQUIRE_THAT(source.volume, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(source.pitch, WithinAbs(1.0f, 0.001f));
    REQUIRE(source.spatial == true);
    REQUIRE_THAT(source.min_distance, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(source.max_distance, WithinAbs(50.0f, 0.001f));
    REQUIRE(source.attenuation == AttenuationModel::InverseSquare);
    REQUIRE_THAT(source.rolloff, WithinAbs(1.0f, 0.001f));
    REQUIRE(source.use_cone == false);
    REQUIRE_THAT(source.cone_inner_angle, WithinAbs(360.0f, 0.001f));
    REQUIRE_THAT(source.cone_outer_angle, WithinAbs(360.0f, 0.001f));
    REQUIRE_THAT(source.cone_outer_volume, WithinAbs(0.0f, 0.001f));
    REQUIRE(source.enable_doppler == true);
    REQUIRE_THAT(source.doppler_factor, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(source.computed_volume, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(source.computed_pan, WithinAbs(0.0f, 0.001f));
    REQUIRE(source.first_update == true);
}

TEST_CASE("AudioSource custom values", "[audio][components]") {
    AudioSource source;
    source.volume = 0.5f;
    source.pitch = 1.5f;
    source.min_distance = 5.0f;
    source.max_distance = 100.0f;
    source.attenuation = AttenuationModel::Linear;
    source.rolloff = 2.0f;
    source.loop = true;
    source.playing = true;

    REQUIRE_THAT(source.volume, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(source.pitch, WithinAbs(1.5f, 0.001f));
    REQUIRE_THAT(source.min_distance, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(source.max_distance, WithinAbs(100.0f, 0.001f));
    REQUIRE(source.attenuation == AttenuationModel::Linear);
    REQUIRE_THAT(source.rolloff, WithinAbs(2.0f, 0.001f));
    REQUIRE(source.loop == true);
    REQUIRE(source.playing == true);
}

TEST_CASE("AudioSource cone settings", "[audio][components]") {
    AudioSource source;
    source.use_cone = true;
    source.cone_inner_angle = 45.0f;
    source.cone_outer_angle = 90.0f;
    source.cone_outer_volume = 0.5f;

    REQUIRE(source.use_cone == true);
    REQUIRE_THAT(source.cone_inner_angle, WithinAbs(45.0f, 0.001f));
    REQUIRE_THAT(source.cone_outer_angle, WithinAbs(90.0f, 0.001f));
    REQUIRE_THAT(source.cone_outer_volume, WithinAbs(0.5f, 0.001f));
}

TEST_CASE("AudioListener defaults", "[audio][components]") {
    AudioListener listener;

    REQUIRE(listener.active == true);
    REQUIRE(listener.priority == 0);
    REQUIRE_THAT(listener.volume_scale, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(listener.velocity.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(listener.prev_position.x, WithinAbs(0.0f, 0.001f));
    REQUIRE(listener.first_update == true);
}

TEST_CASE("AudioListener custom values", "[audio][components]") {
    AudioListener listener;
    listener.active = false;
    listener.priority = 10;
    listener.volume_scale = 0.8f;
    listener.velocity = Vec3{1.0f, 0.0f, 0.0f};

    REQUIRE(listener.active == false);
    REQUIRE(listener.priority == 10);
    REQUIRE_THAT(listener.volume_scale, WithinAbs(0.8f, 0.001f));
    REQUIRE_THAT(listener.velocity.x, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("AudioTrigger defaults", "[audio][components]") {
    AudioTrigger trigger;

    REQUIRE_FALSE(trigger.sound.valid());
    REQUIRE_THAT(trigger.trigger_radius, WithinAbs(5.0f, 0.001f));
    REQUIRE(trigger.one_shot == true);
    REQUIRE(trigger.triggered == false);
    REQUIRE_THAT(trigger.cooldown, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(trigger.cooldown_timer, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("AudioTrigger custom values", "[audio][components]") {
    AudioTrigger trigger;
    trigger.sound.id = 10;
    trigger.trigger_radius = 10.0f;
    trigger.one_shot = false;
    trigger.cooldown = 5.0f;

    REQUIRE(trigger.sound.valid());
    REQUIRE_THAT(trigger.trigger_radius, WithinAbs(10.0f, 0.001f));
    REQUIRE(trigger.one_shot == false);
    REQUIRE_THAT(trigger.cooldown, WithinAbs(5.0f, 0.001f));
}

TEST_CASE("ReverbZone defaults", "[audio][components]") {
    ReverbZone zone;

    REQUIRE_THAT(zone.min_distance, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(zone.max_distance, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(zone.decay_time, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(zone.early_delay, WithinAbs(0.02f, 0.001f));
    REQUIRE_THAT(zone.late_delay, WithinAbs(0.04f, 0.001f));
    REQUIRE_THAT(zone.diffusion, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(zone.density, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(zone.high_frequency_decay, WithinAbs(0.8f, 0.001f));
    REQUIRE(zone.active == true);
}

TEST_CASE("ReverbZone custom values", "[audio][components]") {
    ReverbZone zone;
    zone.min_distance = 5.0f;
    zone.max_distance = 50.0f;
    zone.decay_time = 3.0f;
    zone.diffusion = 0.8f;
    zone.density = 0.9f;
    zone.active = false;

    REQUIRE_THAT(zone.min_distance, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(zone.max_distance, WithinAbs(50.0f, 0.001f));
    REQUIRE_THAT(zone.decay_time, WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(zone.diffusion, WithinAbs(0.8f, 0.001f));
    REQUIRE_THAT(zone.density, WithinAbs(0.9f, 0.001f));
    REQUIRE(zone.active == false);
}

TEST_CASE("calculate_attenuation", "[audio][components]") {
    float min_dist = 1.0f;
    float max_dist = 100.0f;
    float rolloff = 1.0f;

    SECTION("No attenuation") {
        float atten = calculate_attenuation(50.0f, min_dist, max_dist, AttenuationModel::None, rolloff);
        REQUIRE_THAT(atten, WithinAbs(1.0f, 0.001f));
    }

    SECTION("At min distance") {
        float atten = calculate_attenuation(min_dist, min_dist, max_dist, AttenuationModel::Linear, rolloff);
        REQUIRE_THAT(atten, WithinAbs(1.0f, 0.001f));
    }

    SECTION("At max distance") {
        float atten = calculate_attenuation(max_dist, min_dist, max_dist, AttenuationModel::Linear, rolloff);
        REQUIRE_THAT(atten, WithinAbs(0.0f, 0.01f));
    }

    SECTION("Beyond max distance") {
        float atten = calculate_attenuation(max_dist * 2, min_dist, max_dist, AttenuationModel::Linear, rolloff);
        REQUIRE_THAT(atten, WithinAbs(0.0f, 0.01f));
    }

    SECTION("Inside min distance") {
        float atten = calculate_attenuation(0.5f, min_dist, max_dist, AttenuationModel::Linear, rolloff);
        REQUIRE_THAT(atten, WithinAbs(1.0f, 0.001f));
    }
}

TEST_CASE("calculate_cone_attenuation", "[audio][components]") {
    Vec3 source_forward{0.0f, 0.0f, 1.0f};
    float inner_angle = 45.0f;
    float outer_angle = 90.0f;
    float outer_volume = 0.0f;

    SECTION("Directly in front (inside cone)") {
        Vec3 to_listener{0.0f, 0.0f, 1.0f};
        float atten = calculate_cone_attenuation(source_forward, to_listener, inner_angle, outer_angle, outer_volume);
        REQUIRE_THAT(atten, WithinAbs(1.0f, 0.01f));
    }

    SECTION("Directly behind (outside cone)") {
        Vec3 to_listener{0.0f, 0.0f, -1.0f};
        float atten = calculate_cone_attenuation(source_forward, to_listener, inner_angle, outer_angle, outer_volume);
        REQUIRE_THAT(atten, WithinAbs(outer_volume, 0.01f));
    }
}
