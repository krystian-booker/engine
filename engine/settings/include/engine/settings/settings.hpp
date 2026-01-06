#pragma once

// ============================================================================
// Engine Settings System - Umbrella Header
// ============================================================================
//
// Runtime configurable game settings with persistence and presets.
//
// Quick Start:
// ------------
// 1. Load settings on startup:
//    settings().load();
//
// 2. Access settings:
//    float sensitivity = settings().input().mouse_sensitivity;
//    bool vsync = settings().graphics().vsync;
//
// 3. Modify settings:
//    settings().graphics().vsync = false;
//    settings().apply_graphics();
//
// 4. Save settings:
//    settings().save();
//
// Settings Categories:
// --------------------
// - GraphicsSettings: Resolution, quality, effects
// - AudioSettings: Volume levels, output devices
// - InputSettings: Keybindings, sensitivity
// - GameplaySettings: Difficulty, accessibility, HUD
//
// ============================================================================

#include <engine/settings/graphics_settings.hpp>
#include <engine/settings/audio_settings.hpp>
#include <engine/settings/input_settings.hpp>
#include <engine/settings/gameplay_settings.hpp>
#include <engine/settings/settings_manager.hpp>
