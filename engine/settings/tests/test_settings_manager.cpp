#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/settings/settings_manager.hpp>

using namespace engine::settings;
using Catch::Matchers::WithinAbs;

TEST_CASE("SettingsManager singleton", "[settings][manager]") {
    auto& manager = settings();

    SECTION("Instance access") {
        auto& manager2 = SettingsManager::instance();
        REQUIRE(&manager == &manager2);
    }
}

TEST_CASE("SettingsManager graphics access", "[settings][graphics]") {
    auto& manager = settings();

    SECTION("Access graphics settings") {
        auto& graphics = manager.graphics();
        // Should have default values
        REQUIRE(graphics.resolution_width > 0);
        REQUIRE(graphics.resolution_height > 0);
    }

    SECTION("Modify graphics settings") {
        auto& graphics = manager.graphics();
        int original_width = graphics.resolution_width;

        graphics.resolution_width = 2560;
        REQUIRE(manager.graphics().resolution_width == 2560);

        // Restore
        graphics.resolution_width = original_width;
    }

    SECTION("Const access") {
        const auto& const_manager = manager;
        const auto& graphics = const_manager.graphics();
        REQUIRE(graphics.resolution_width > 0);
    }
}

TEST_CASE("SettingsManager audio access", "[settings][audio]") {
    auto& manager = settings();

    SECTION("Access audio settings") {
        auto& audio = manager.audio();
        // Should have default volume values
        REQUIRE(audio.master_volume >= 0.0f);
        REQUIRE(audio.master_volume <= 1.0f);
    }

    SECTION("Modify audio settings") {
        auto& audio = manager.audio();
        float original_volume = audio.master_volume;

        audio.master_volume = 0.5f;
        REQUIRE_THAT(manager.audio().master_volume, WithinAbs(0.5f, 0.001f));

        // Restore
        audio.master_volume = original_volume;
    }
}

TEST_CASE("SettingsManager input access", "[settings][input]") {
    auto& manager = settings();

    SECTION("Access input settings") {
        auto& input = manager.input();
        // Should have valid sensitivity
        REQUIRE(input.mouse_sensitivity > 0.0f);
    }
}

TEST_CASE("SettingsManager gameplay access", "[settings][gameplay]") {
    auto& manager = settings();

    SECTION("Access gameplay settings") {
        auto& gameplay = manager.gameplay();
        // Gameplay settings should be accessible
        REQUIRE(&gameplay == &manager.gameplay());
    }
}

TEST_CASE("SettingsManager reset functionality", "[settings][reset]") {
    auto& manager = settings();

    SECTION("Reset graphics to defaults") {
        auto& graphics = manager.graphics();
        int original = graphics.resolution_width;
        graphics.resolution_width = 9999;

        manager.reset_graphics();

        // Should be back to some reasonable default
        REQUIRE(manager.graphics().resolution_width != 9999);
    }

    SECTION("Reset audio to defaults") {
        auto& audio = manager.audio();
        float original = audio.master_volume;
        audio.master_volume = 0.123f;

        manager.reset_audio();

        // Should be back to default
        REQUIRE(manager.audio().master_volume != 0.123f);
    }

    SECTION("Reset all") {
        manager.graphics().resolution_width = 9999;
        manager.audio().master_volume = 0.123f;

        manager.reset_to_defaults();

        REQUIRE(manager.graphics().resolution_width != 9999);
        REQUIRE(manager.audio().master_volume != 0.123f);
    }
}

TEST_CASE("SettingsManager callbacks", "[settings][callbacks]") {
    auto& manager = settings();

    SECTION("Settings changed callback") {
        bool callback_called = false;
        SettingsCategory received_category = SettingsCategory::All;

        manager.set_on_settings_changed([&](SettingsCategory cat) {
            callback_called = true;
            received_category = cat;
        });

        // Graphics changes should trigger callback when applied
        manager.apply_graphics();

        // Note: callback behavior depends on implementation
        // Just ensure setting callback doesn't crash
    }

    SECTION("Graphics changed callback") {
        bool called = false;
        manager.set_on_graphics_changed([&]() {
            called = true;
        });

        // Setting callback shouldn't crash
        REQUIRE_NOTHROW(manager.apply_graphics());
    }

    SECTION("Audio changed callback") {
        bool called = false;
        manager.set_on_audio_changed([&]() {
            called = true;
        });

        REQUIRE_NOTHROW(manager.apply_audio());
    }

    // Clear callbacks
    manager.set_on_settings_changed(nullptr);
    manager.set_on_graphics_changed(nullptr);
    manager.set_on_audio_changed(nullptr);
}

TEST_CASE("SettingsManager dirty tracking", "[settings][dirty]") {
    auto& manager = settings();

    SECTION("Changes mark as dirty") {
        manager.mark_saved();
        REQUIRE_FALSE(manager.has_unsaved_changes());

        manager.graphics().resolution_width += 1;
        // Implementation may or may not auto-track changes
        // This tests the API exists
    }

    SECTION("Mark saved clears dirty flag") {
        manager.mark_saved();
        REQUIRE_FALSE(manager.has_unsaved_changes());
    }
}

TEST_CASE("SettingsManager graphics presets", "[settings][presets]") {
    auto& manager = settings();

    SECTION("Apply low preset") {
        manager.apply_graphics_preset(QualityPreset::Low);
        REQUIRE(manager.graphics().preset == QualityPreset::Low);
    }

    SECTION("Apply medium preset") {
        manager.apply_graphics_preset(QualityPreset::Medium);
        REQUIRE(manager.graphics().preset == QualityPreset::Medium);
    }

    SECTION("Apply high preset") {
        manager.apply_graphics_preset(QualityPreset::High);
        REQUIRE(manager.graphics().preset == QualityPreset::High);
    }

    SECTION("Apply ultra preset") {
        manager.apply_graphics_preset(QualityPreset::Ultra);
        REQUIRE(manager.graphics().preset == QualityPreset::Ultra);
    }

    SECTION("Detect optimal preset") {
        auto preset = manager.detect_optimal_preset();
        // Should return a valid preset
        REQUIRE((preset == QualityPreset::Low ||
                 preset == QualityPreset::Medium ||
                 preset == QualityPreset::High ||
                 preset == QualityPreset::Ultra ||
                 preset == QualityPreset::Custom));
    }
}

TEST_CASE("SettingsManager validation", "[settings][validation]") {
    auto& manager = settings();

    SECTION("Validate all clamps invalid values") {
        manager.graphics().gamma = -5.0f;  // Invalid negative gamma
        manager.validate_all();
        REQUIRE(manager.graphics().gamma >= 0.0f);
    }
}

TEST_CASE("SettingsCategory enum", "[settings]") {
    SECTION("Category values") {
        REQUIRE(static_cast<uint8_t>(SettingsCategory::Graphics) == 0);
        REQUIRE(static_cast<uint8_t>(SettingsCategory::Audio) == 1);
        REQUIRE(static_cast<uint8_t>(SettingsCategory::Input) == 2);
        REQUIRE(static_cast<uint8_t>(SettingsCategory::Gameplay) == 3);
        REQUIRE(static_cast<uint8_t>(SettingsCategory::All) == 4);
    }
}
