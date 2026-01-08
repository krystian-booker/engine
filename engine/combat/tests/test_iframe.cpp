#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/combat/iframe.hpp>

using namespace engine::combat;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// IFrameSource Tests
// ============================================================================

TEST_CASE("IFrameSource enum", "[combat][iframe]") {
    REQUIRE(static_cast<uint8_t>(IFrameSource::Dodge) == 0);
    REQUIRE(static_cast<uint8_t>(IFrameSource::Hit) == 1);
    REQUIRE(static_cast<uint8_t>(IFrameSource::Attack) == 2);
    REQUIRE(static_cast<uint8_t>(IFrameSource::Skill) == 3);
    REQUIRE(static_cast<uint8_t>(IFrameSource::Spawn) == 4);
    REQUIRE(static_cast<uint8_t>(IFrameSource::Custom) == 5);
}

// ============================================================================
// IFrameComponent Tests
// ============================================================================

TEST_CASE("IFrameComponent defaults", "[combat][iframe]") {
    IFrameComponent iframe;

    REQUIRE(iframe.is_invincible == false);
    REQUIRE_THAT(iframe.remaining_time, WithinAbs(0.0f, 0.001f));
    REQUIRE(iframe.source == IFrameSource::Dodge);
    REQUIRE(iframe.flash_enabled == true);
    REQUIRE_THAT(iframe.flash_interval, WithinAbs(0.1f, 0.001f));
    REQUIRE_THAT(iframe.flash_timer, WithinAbs(0.0f, 0.001f));
    REQUIRE(iframe.flash_visible == true);
    REQUIRE_THAT(iframe.flash_color.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(iframe.flash_color.y, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(iframe.flash_color.z, WithinAbs(1.0f, 0.001f));
    REQUIRE(iframe.dodge_sound.empty());
}

TEST_CASE("IFrameComponent default durations", "[combat][iframe]") {
    REQUIRE_THAT(IFrameComponent::DEFAULT_DODGE_DURATION, WithinAbs(0.4f, 0.001f));
    REQUIRE_THAT(IFrameComponent::DEFAULT_HIT_DURATION, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(IFrameComponent::DEFAULT_SPAWN_DURATION, WithinAbs(2.0f, 0.001f));
}

TEST_CASE("IFrameComponent grant", "[combat][iframe]") {
    IFrameComponent iframe;

    SECTION("Grant basic i-frames") {
        iframe.grant(0.5f, IFrameSource::Dodge);

        REQUIRE(iframe.is_invincible == true);
        REQUIRE_THAT(iframe.remaining_time, WithinAbs(0.5f, 0.001f));
        REQUIRE(iframe.source == IFrameSource::Dodge);
        REQUIRE_THAT(iframe.flash_timer, WithinAbs(0.0f, 0.001f));
        REQUIRE(iframe.flash_visible == true);
    }

    SECTION("Grant extends duration if longer") {
        iframe.grant(0.3f, IFrameSource::Dodge);
        REQUIRE_THAT(iframe.remaining_time, WithinAbs(0.3f, 0.001f));

        iframe.grant(0.5f, IFrameSource::Hit);  // Longer duration
        REQUIRE_THAT(iframe.remaining_time, WithinAbs(0.5f, 0.001f));
        REQUIRE(iframe.source == IFrameSource::Hit);
    }

    SECTION("Grant doesn't reduce duration if shorter") {
        iframe.grant(0.5f, IFrameSource::Spawn);
        REQUIRE_THAT(iframe.remaining_time, WithinAbs(0.5f, 0.001f));

        iframe.grant(0.2f, IFrameSource::Dodge);  // Shorter duration - no effect
        REQUIRE_THAT(iframe.remaining_time, WithinAbs(0.5f, 0.001f));
        REQUIRE(iframe.source == IFrameSource::Spawn);  // Source unchanged
    }
}

TEST_CASE("IFrameComponent grant_default", "[combat][iframe]") {
    IFrameComponent iframe;

    SECTION("Dodge default") {
        iframe.grant_default(IFrameSource::Dodge);
        REQUIRE_THAT(iframe.remaining_time, WithinAbs(0.4f, 0.001f));
        REQUIRE(iframe.source == IFrameSource::Dodge);
    }

    SECTION("Hit default") {
        iframe.grant_default(IFrameSource::Hit);
        REQUIRE_THAT(iframe.remaining_time, WithinAbs(0.5f, 0.001f));
        REQUIRE(iframe.source == IFrameSource::Hit);
    }

    SECTION("Spawn default") {
        iframe.grant_default(IFrameSource::Spawn);
        REQUIRE_THAT(iframe.remaining_time, WithinAbs(2.0f, 0.001f));
        REQUIRE(iframe.source == IFrameSource::Spawn);
    }

    SECTION("Custom default") {
        iframe.grant_default(IFrameSource::Custom);
        REQUIRE_THAT(iframe.remaining_time, WithinAbs(0.3f, 0.001f));  // Fallback
        REQUIRE(iframe.source == IFrameSource::Custom);
    }
}

TEST_CASE("IFrameComponent update", "[combat][iframe]") {
    IFrameComponent iframe;
    iframe.grant(0.5f, IFrameSource::Dodge);

    SECTION("Update reduces remaining time") {
        bool ended = iframe.update(0.1f);
        REQUIRE_FALSE(ended);
        REQUIRE(iframe.is_invincible == true);
        REQUIRE_THAT(iframe.remaining_time, WithinAbs(0.4f, 0.001f));
    }

    SECTION("Update toggles flash") {
        iframe.flash_interval = 0.1f;
        iframe.update(0.15f);  // Past one flash interval
        REQUIRE_THAT(iframe.flash_timer, WithinAbs(0.05f, 0.01f));
    }

    SECTION("Update returns true when i-frames end") {
        bool ended = iframe.update(0.6f);  // More than remaining
        REQUIRE(ended);
        REQUIRE(iframe.is_invincible == false);
        REQUIRE_THAT(iframe.remaining_time, WithinAbs(0.0f, 0.001f));
        REQUIRE(iframe.flash_visible == true);  // Reset to visible
    }

    SECTION("Update does nothing when not invincible") {
        IFrameComponent not_invincible;
        bool ended = not_invincible.update(0.1f);
        REQUIRE_FALSE(ended);
    }
}

TEST_CASE("IFrameComponent cancel", "[combat][iframe]") {
    IFrameComponent iframe;
    iframe.grant(1.0f, IFrameSource::Spawn);

    REQUIRE(iframe.is_invincible == true);
    REQUIRE_THAT(iframe.remaining_time, WithinAbs(1.0f, 0.001f));

    iframe.cancel();

    REQUIRE(iframe.is_invincible == false);
    REQUIRE_THAT(iframe.remaining_time, WithinAbs(0.0f, 0.001f));
    REQUIRE(iframe.flash_visible == true);
}

TEST_CASE("IFrameComponent get_progress", "[combat][iframe]") {
    IFrameComponent iframe;

    SECTION("Not invincible returns 0") {
        float progress = iframe.get_progress();
        REQUIRE_THAT(progress, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Invincible returns 1") {
        iframe.grant(1.0f, IFrameSource::Dodge);
        float progress = iframe.get_progress();
        REQUIRE_THAT(progress, WithinAbs(1.0f, 0.001f));
    }

    SECTION("After time passes still returns 1 if remaining > 0") {
        iframe.grant(1.0f, IFrameSource::Dodge);
        iframe.update(0.5f);
        float progress = iframe.get_progress();
        REQUIRE_THAT(progress, WithinAbs(1.0f, 0.001f));  // Binary implementation
    }
}

TEST_CASE("IFrameComponent flash configuration", "[combat][iframe]") {
    IFrameComponent iframe;
    iframe.flash_enabled = true;
    iframe.flash_interval = 0.05f;
    iframe.flash_color = Vec3{1.0f, 0.0f, 0.0f};  // Red flash

    REQUIRE(iframe.flash_enabled == true);
    REQUIRE_THAT(iframe.flash_interval, WithinAbs(0.05f, 0.001f));
    REQUIRE_THAT(iframe.flash_color.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(iframe.flash_color.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(iframe.flash_color.z, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("IFrameComponent flash disabled", "[combat][iframe]") {
    IFrameComponent iframe;
    iframe.flash_enabled = false;
    iframe.grant(0.5f, IFrameSource::Dodge);

    iframe.update(0.2f);

    // Flash should not have toggled
    REQUIRE(iframe.flash_visible == true);
}
