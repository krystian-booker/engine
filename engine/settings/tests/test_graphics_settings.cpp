#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/settings/graphics_settings.hpp>

using namespace engine::settings;
using Catch::Matchers::WithinAbs;

TEST_CASE("GraphicsSettings default values", "[settings][graphics]") {
    GraphicsSettings gs;

    SECTION("Display defaults") {
        REQUIRE(gs.resolution_width == 1920);
        REQUIRE(gs.resolution_height == 1080);
        REQUIRE(gs.refresh_rate == 60);
        REQUIRE(gs.fullscreen == false);
        REQUIRE(gs.borderless == false);
        REQUIRE(gs.vsync == true);
        REQUIRE(gs.framerate_limit == 0);
        REQUIRE_THAT(gs.gamma, WithinAbs(1.0f, 0.001f));
    }

    SECTION("Quality defaults") {
        REQUIRE(gs.preset == QualityPreset::High);
        REQUIRE_THAT(gs.render_scale, WithinAbs(1.0f, 0.001f));
        REQUIRE(gs.shadow_quality == ShadowQuality::High);
        REQUIRE(gs.texture_quality == TextureQuality::High);
        REQUIRE(gs.antialiasing == AntialiasingMode::TAA);
        REQUIRE(gs.anisotropic_filtering == 8);
    }

    SECTION("Effects defaults") {
        REQUIRE(gs.bloom_enabled == true);
        REQUIRE(gs.ambient_occlusion_enabled == true);
        REQUIRE(gs.depth_of_field_enabled == true);
        REQUIRE(gs.volumetric_lighting == true);
    }
}

TEST_CASE("GraphicsSettings presets", "[settings][graphics][presets]") {
    GraphicsSettings gs;

    SECTION("Low preset reduces quality") {
        gs.apply_preset(QualityPreset::Low);

        REQUIRE(gs.preset == QualityPreset::Low);
        REQUIRE(gs.shadow_quality == ShadowQuality::Low);
        REQUIRE(gs.texture_quality == TextureQuality::Low);
    }

    SECTION("Ultra preset maximizes quality") {
        gs.apply_preset(QualityPreset::Ultra);

        REQUIRE(gs.preset == QualityPreset::Ultra);
        REQUIRE(gs.shadow_quality == ShadowQuality::Ultra);
        REQUIRE(gs.texture_quality == TextureQuality::Ultra);
    }

    SECTION("Medium preset balanced") {
        gs.apply_preset(QualityPreset::Medium);
        REQUIRE(gs.preset == QualityPreset::Medium);
        REQUIRE(gs.shadow_quality == ShadowQuality::Medium);
    }

    SECTION("High preset") {
        gs.apply_preset(QualityPreset::High);
        REQUIRE(gs.preset == QualityPreset::High);
        REQUIRE(gs.shadow_quality == ShadowQuality::High);
    }
}

TEST_CASE("GraphicsSettings validation", "[settings][graphics][validation]") {
    GraphicsSettings gs;

    SECTION("Clamps resolution") {
        gs.resolution_width = -100;
        gs.resolution_height = 0;
        gs.validate();

        REQUIRE(gs.resolution_width > 0);
        REQUIRE(gs.resolution_height > 0);
    }

    SECTION("Clamps gamma") {
        gs.gamma = -1.0f;
        gs.validate();
        REQUIRE(gs.gamma >= 0.0f);

        gs.gamma = 10.0f;
        gs.validate();
        REQUIRE(gs.gamma <= 5.0f); // Reasonable max
    }

    SECTION("Clamps render scale") {
        gs.render_scale = -0.5f;
        gs.validate();
        REQUIRE(gs.render_scale >= 0.25f);

        gs.render_scale = 5.0f;
        gs.validate();
        REQUIRE(gs.render_scale <= 2.0f);
    }

    SECTION("Clamps bloom intensity") {
        gs.bloom_intensity = -1.0f;
        gs.validate();
        REQUIRE(gs.bloom_intensity >= 0.0f);
    }

    SECTION("Clamps anisotropic filtering") {
        gs.anisotropic_filtering = 32;
        gs.validate();
        REQUIRE(gs.anisotropic_filtering <= 16);

        gs.anisotropic_filtering = 0;
        gs.validate();
        REQUIRE(gs.anisotropic_filtering >= 1);
    }
}

TEST_CASE("GraphicsSettings equality", "[settings][graphics]") {
    GraphicsSettings gs1;
    GraphicsSettings gs2;

    SECTION("Default settings are equal") {
        REQUIRE(gs1 == gs2);
    }

    SECTION("Different settings are not equal") {
        gs1.resolution_width = 2560;
        REQUIRE_FALSE(gs1 == gs2);
    }

    SECTION("Modified same way are equal") {
        gs1.resolution_width = 2560;
        gs2.resolution_width = 2560;
        REQUIRE(gs1 == gs2);
    }
}

TEST_CASE("Quality enum helper functions", "[settings][enums]") {
    SECTION("Preset names") {
        REQUIRE(get_preset_name(QualityPreset::Low) == "Low");
        REQUIRE(get_preset_name(QualityPreset::Medium) == "Medium");
        REQUIRE(get_preset_name(QualityPreset::High) == "High");
        REQUIRE(get_preset_name(QualityPreset::Ultra) == "Ultra");
        REQUIRE(get_preset_name(QualityPreset::Custom) == "Custom");
    }

    SECTION("AA mode names") {
        REQUIRE(get_aa_mode_name(AntialiasingMode::None) == "None");
        REQUIRE(get_aa_mode_name(AntialiasingMode::FXAA) == "FXAA");
        REQUIRE(get_aa_mode_name(AntialiasingMode::TAA) == "TAA");
        REQUIRE(get_aa_mode_name(AntialiasingMode::MSAA_2x) == "MSAA 2x");
        REQUIRE(get_aa_mode_name(AntialiasingMode::MSAA_4x) == "MSAA 4x");
        REQUIRE(get_aa_mode_name(AntialiasingMode::MSAA_8x) == "MSAA 8x");
    }

    SECTION("Shadow quality names") {
        REQUIRE(get_shadow_quality_name(ShadowQuality::Off) == "Off");
        REQUIRE(get_shadow_quality_name(ShadowQuality::Low) == "Low");
        REQUIRE(get_shadow_quality_name(ShadowQuality::Medium) == "Medium");
        REQUIRE(get_shadow_quality_name(ShadowQuality::High) == "High");
        REQUIRE(get_shadow_quality_name(ShadowQuality::Ultra) == "Ultra");
    }

    SECTION("Texture quality names") {
        REQUIRE(get_texture_quality_name(TextureQuality::Low) == "Low");
        REQUIRE(get_texture_quality_name(TextureQuality::Medium) == "Medium");
        REQUIRE(get_texture_quality_name(TextureQuality::High) == "High");
        REQUIRE(get_texture_quality_name(TextureQuality::Ultra) == "Ultra");
    }
}

TEST_CASE("AntialiasingMode enum values", "[settings][enums]") {
    REQUIRE(static_cast<uint8_t>(AntialiasingMode::None) == 0);
    REQUIRE(static_cast<uint8_t>(AntialiasingMode::FXAA) == 1);
    REQUIRE(static_cast<uint8_t>(AntialiasingMode::TAA) == 2);
    REQUIRE(static_cast<uint8_t>(AntialiasingMode::MSAA_2x) == 3);
    REQUIRE(static_cast<uint8_t>(AntialiasingMode::MSAA_4x) == 4);
    REQUIRE(static_cast<uint8_t>(AntialiasingMode::MSAA_8x) == 5);
}
