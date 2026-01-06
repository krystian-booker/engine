#pragma once

#include <engine/settings/graphics_settings.hpp>
#include <engine/settings/audio_settings.hpp>
#include <engine/settings/input_settings.hpp>
#include <engine/settings/gameplay_settings.hpp>
#include <functional>
#include <string>

namespace engine::settings {

// ============================================================================
// Settings Changed Event
// ============================================================================

enum class SettingsCategory : uint8_t {
    Graphics,
    Audio,
    Input,
    Gameplay,
    All
};

struct SettingsChangedEvent {
    SettingsCategory category;
};

// ============================================================================
// Settings Manager
// ============================================================================

class SettingsManager {
public:
    // Singleton access
    static SettingsManager& instance();

    // Delete copy/move
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    // ========================================================================
    // Access Settings
    // ========================================================================

    GraphicsSettings& graphics() { return m_graphics; }
    const GraphicsSettings& graphics() const { return m_graphics; }

    AudioSettings& audio() { return m_audio; }
    const AudioSettings& audio() const { return m_audio; }

    InputSettings& input() { return m_input; }
    const InputSettings& input() const { return m_input; }

    GameplaySettings& gameplay() { return m_gameplay; }
    const GameplaySettings& gameplay() const { return m_gameplay; }

    // ========================================================================
    // Persistence
    // ========================================================================

    // Load settings from file
    bool load(const std::string& path = "settings.json");

    // Save settings to file
    bool save(const std::string& path = "settings.json") const;

    // Get default settings path
    std::string get_default_path() const;

    // Reset to defaults
    void reset_to_defaults();
    void reset_graphics();
    void reset_audio();
    void reset_input();
    void reset_gameplay();

    // ========================================================================
    // Apply Changes
    // ========================================================================

    // Apply settings to the engine systems
    void apply_graphics();
    void apply_audio();
    void apply_input();
    void apply_gameplay();
    void apply_all();

    // Validate and clamp settings to valid ranges
    void validate_all();

    // ========================================================================
    // Graphics Presets
    // ========================================================================

    void apply_graphics_preset(QualityPreset preset);
    QualityPreset detect_optimal_preset() const;

    // Get available resolutions
    struct Resolution {
        int width;
        int height;
        int refresh_rate;
    };
    std::vector<Resolution> get_available_resolutions() const;

    // ========================================================================
    // Audio Devices
    // ========================================================================

    struct AudioDevice {
        int index;
        std::string name;
        bool is_default;
    };
    std::vector<AudioDevice> get_output_devices() const;
    std::vector<AudioDevice> get_input_devices() const;

    // ========================================================================
    // Keybinding Helpers
    // ========================================================================

    void bind_action(const std::string& action, KeyCode key, bool secondary = false);
    void bind_action(const std::string& action, GamepadButton button);
    void unbind_action(const std::string& action);
    std::string get_binding_display(const std::string& action) const;

    // Check for conflicts
    std::vector<std::string> get_conflicting_bindings(const std::string& action, KeyCode key) const;
    std::vector<std::string> get_conflicting_bindings(const std::string& action, GamepadButton button) const;

    // ========================================================================
    // Callbacks
    // ========================================================================

    using SettingsCallback = std::function<void(SettingsCategory)>;

    void set_on_settings_changed(SettingsCallback callback);
    void set_on_graphics_changed(std::function<void()> callback);
    void set_on_audio_changed(std::function<void()> callback);
    void set_on_input_changed(std::function<void()> callback);
    void set_on_gameplay_changed(std::function<void()> callback);

    // ========================================================================
    // Dirty Tracking
    // ========================================================================

    bool has_unsaved_changes() const;
    void mark_saved();

private:
    SettingsManager();
    ~SettingsManager() = default;

    void notify_changed(SettingsCategory category);
    void setup_default_bindings();

    GraphicsSettings m_graphics;
    GraphicsSettings m_graphics_saved;  // For dirty tracking

    AudioSettings m_audio;
    AudioSettings m_audio_saved;

    InputSettings m_input;
    InputSettings m_input_saved;

    GameplaySettings m_gameplay;
    GameplaySettings m_gameplay_saved;

    SettingsCallback m_on_settings_changed;
    std::function<void()> m_on_graphics_changed;
    std::function<void()> m_on_audio_changed;
    std::function<void()> m_on_input_changed;
    std::function<void()> m_on_gameplay_changed;

    bool m_dirty = false;
};

// ============================================================================
// Global Access
// ============================================================================

inline SettingsManager& settings() { return SettingsManager::instance(); }

} // namespace engine::settings
