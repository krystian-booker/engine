#include <engine/settings/settings_manager.hpp>
#include <engine/core/log.hpp>
#include <engine/core/game_events.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

namespace engine::settings {

using json = nlohmann::json;

// ============================================================================
// SettingsManager Singleton
// ============================================================================

SettingsManager::SettingsManager() {
    // Setup default input bindings
    setup_default_bindings();

    // Save initial state for dirty tracking
    mark_saved();
}

SettingsManager& SettingsManager::instance() {
    static SettingsManager s_instance;
    return s_instance;
}

// ============================================================================
// Persistence
// ============================================================================

bool SettingsManager::load(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            core::log(core::LogLevel::Warn, "[Settings] Could not open settings file: {}", path);
            return false;
        }

        json j = json::parse(file);

        // Graphics
        if (j.contains("graphics")) {
            auto& g = j["graphics"];
            if (g.contains("resolution_width")) m_graphics.resolution_width = g["resolution_width"];
            if (g.contains("resolution_height")) m_graphics.resolution_height = g["resolution_height"];
            if (g.contains("fullscreen")) m_graphics.fullscreen = g["fullscreen"];
            if (g.contains("borderless")) m_graphics.borderless = g["borderless"];
            if (g.contains("vsync")) m_graphics.vsync = g["vsync"];
            if (g.contains("framerate_limit")) m_graphics.framerate_limit = g["framerate_limit"];
            if (g.contains("gamma")) m_graphics.gamma = g["gamma"];
            if (g.contains("preset")) m_graphics.preset = static_cast<QualityPreset>(g["preset"].get<int>());
            if (g.contains("render_scale")) m_graphics.render_scale = g["render_scale"];
            if (g.contains("shadow_quality")) m_graphics.shadow_quality = static_cast<ShadowQuality>(g["shadow_quality"].get<int>());
            if (g.contains("texture_quality")) m_graphics.texture_quality = static_cast<TextureQuality>(g["texture_quality"].get<int>());
            if (g.contains("antialiasing")) m_graphics.antialiasing = static_cast<AntialiasingMode>(g["antialiasing"].get<int>());
            if (g.contains("bloom_enabled")) m_graphics.bloom_enabled = g["bloom_enabled"];
            if (g.contains("ambient_occlusion_enabled")) m_graphics.ambient_occlusion_enabled = g["ambient_occlusion_enabled"];
            if (g.contains("motion_blur_enabled")) m_graphics.motion_blur_enabled = g["motion_blur_enabled"];
            if (g.contains("depth_of_field_enabled")) m_graphics.depth_of_field_enabled = g["depth_of_field_enabled"];
            if (g.contains("volumetric_lighting")) m_graphics.volumetric_lighting = g["volumetric_lighting"];
        }

        // Audio
        if (j.contains("audio")) {
            auto& a = j["audio"];
            if (a.contains("master_volume")) m_audio.master_volume = a["master_volume"];
            if (a.contains("music_volume")) m_audio.music_volume = a["music_volume"];
            if (a.contains("sfx_volume")) m_audio.sfx_volume = a["sfx_volume"];
            if (a.contains("voice_volume")) m_audio.voice_volume = a["voice_volume"];
            if (a.contains("ambient_volume")) m_audio.ambient_volume = a["ambient_volume"];
            if (a.contains("enable_3d_audio")) m_audio.enable_3d_audio = a["enable_3d_audio"];
            if (a.contains("mute_when_unfocused")) m_audio.mute_when_unfocused = a["mute_when_unfocused"];
            if (a.contains("speaker_config")) m_audio.speaker_config = static_cast<SpeakerConfig>(a["speaker_config"].get<int>());
        }

        // Input
        if (j.contains("input")) {
            auto& i = j["input"];
            if (i.contains("mouse_sensitivity")) m_input.mouse_sensitivity = i["mouse_sensitivity"];
            if (i.contains("invert_mouse_y")) m_input.invert_mouse_y = i["invert_mouse_y"];
            if (i.contains("raw_mouse_input")) m_input.raw_mouse_input = i["raw_mouse_input"];
            if (i.contains("gamepad_sensitivity")) m_input.gamepad_sensitivity = i["gamepad_sensitivity"];
            if (i.contains("invert_gamepad_y")) m_input.invert_gamepad_y = i["invert_gamepad_y"];
            if (i.contains("vibration_enabled")) m_input.vibration_enabled = i["vibration_enabled"];
            if (i.contains("aim_assist_enabled")) m_input.aim_assist_enabled = i["aim_assist_enabled"];
            if (i.contains("aim_assist_strength")) m_input.aim_assist_strength = i["aim_assist_strength"];

            // Load keybindings
            if (i.contains("bindings")) {
                for (auto& [action, binding] : i["bindings"].items()) {
                    InputBinding b;
                    b.action = action;
                    if (binding.contains("primary_key")) b.primary_key = static_cast<KeyCode>(binding["primary_key"].get<int>());
                    if (binding.contains("secondary_key")) b.secondary_key = static_cast<KeyCode>(binding["secondary_key"].get<int>());
                    if (binding.contains("gamepad_button")) b.gamepad_button = static_cast<GamepadButton>(binding["gamepad_button"].get<int>());
                    m_input.bindings[action] = b;
                }
            }
        }

        // Gameplay
        if (j.contains("gameplay")) {
            auto& gp = j["gameplay"];
            if (gp.contains("difficulty")) m_gameplay.difficulty = static_cast<Difficulty>(gp["difficulty"].get<int>());
            if (gp.contains("camera_distance")) m_gameplay.camera_distance = gp["camera_distance"];
            if (gp.contains("field_of_view")) m_gameplay.field_of_view = gp["field_of_view"];
            if (gp.contains("subtitles_enabled")) m_gameplay.subtitles_enabled = gp["subtitles_enabled"];
            if (gp.contains("subtitle_size")) m_gameplay.subtitle_size = static_cast<SubtitleSize>(gp["subtitle_size"].get<int>());
            if (gp.contains("colorblind_mode")) m_gameplay.colorblind_mode = static_cast<ColorblindMode>(gp["colorblind_mode"].get<int>());
            if (gp.contains("show_damage_numbers")) m_gameplay.show_damage_numbers = gp["show_damage_numbers"];
            if (gp.contains("hud_mode")) m_gameplay.hud_mode = static_cast<HUDMode>(gp["hud_mode"].get<int>());
            if (gp.contains("tutorials_enabled")) m_gameplay.tutorials_enabled = gp["tutorials_enabled"];
            if (gp.contains("text_language")) m_gameplay.text_language = gp["text_language"];
            if (gp.contains("voice_language")) m_gameplay.voice_language = gp["voice_language"];
        }

        validate_all();
        mark_saved();
        core::log(core::LogLevel::Info, "[Settings] Loaded settings from: {}", path);
        return true;

    } catch (const std::exception& e) {
        core::log(core::LogLevel::Error, "[Settings] Failed to load settings: {}", e.what());
        return false;
    }
}

bool SettingsManager::save(const std::string& path) const {
    try {
        json j;

        // Graphics
        j["graphics"] = {
            {"resolution_width", m_graphics.resolution_width},
            {"resolution_height", m_graphics.resolution_height},
            {"fullscreen", m_graphics.fullscreen},
            {"borderless", m_graphics.borderless},
            {"vsync", m_graphics.vsync},
            {"framerate_limit", m_graphics.framerate_limit},
            {"gamma", m_graphics.gamma},
            {"preset", static_cast<int>(m_graphics.preset)},
            {"render_scale", m_graphics.render_scale},
            {"shadow_quality", static_cast<int>(m_graphics.shadow_quality)},
            {"texture_quality", static_cast<int>(m_graphics.texture_quality)},
            {"antialiasing", static_cast<int>(m_graphics.antialiasing)},
            {"bloom_enabled", m_graphics.bloom_enabled},
            {"ambient_occlusion_enabled", m_graphics.ambient_occlusion_enabled},
            {"motion_blur_enabled", m_graphics.motion_blur_enabled},
            {"depth_of_field_enabled", m_graphics.depth_of_field_enabled},
            {"volumetric_lighting", m_graphics.volumetric_lighting}
        };

        // Audio
        j["audio"] = {
            {"master_volume", m_audio.master_volume},
            {"music_volume", m_audio.music_volume},
            {"sfx_volume", m_audio.sfx_volume},
            {"voice_volume", m_audio.voice_volume},
            {"ambient_volume", m_audio.ambient_volume},
            {"enable_3d_audio", m_audio.enable_3d_audio},
            {"mute_when_unfocused", m_audio.mute_when_unfocused},
            {"speaker_config", static_cast<int>(m_audio.speaker_config)}
        };

        // Input
        json bindings_json;
        for (const auto& [action, binding] : m_input.bindings) {
            bindings_json[action] = {
                {"primary_key", static_cast<int>(binding.primary_key)},
                {"secondary_key", static_cast<int>(binding.secondary_key)},
                {"gamepad_button", static_cast<int>(binding.gamepad_button)}
            };
        }

        j["input"] = {
            {"mouse_sensitivity", m_input.mouse_sensitivity},
            {"invert_mouse_y", m_input.invert_mouse_y},
            {"raw_mouse_input", m_input.raw_mouse_input},
            {"gamepad_sensitivity", m_input.gamepad_sensitivity},
            {"invert_gamepad_y", m_input.invert_gamepad_y},
            {"vibration_enabled", m_input.vibration_enabled},
            {"aim_assist_enabled", m_input.aim_assist_enabled},
            {"aim_assist_strength", m_input.aim_assist_strength},
            {"bindings", bindings_json}
        };

        // Gameplay
        j["gameplay"] = {
            {"difficulty", static_cast<int>(m_gameplay.difficulty)},
            {"camera_distance", m_gameplay.camera_distance},
            {"field_of_view", m_gameplay.field_of_view},
            {"subtitles_enabled", m_gameplay.subtitles_enabled},
            {"subtitle_size", static_cast<int>(m_gameplay.subtitle_size)},
            {"colorblind_mode", static_cast<int>(m_gameplay.colorblind_mode)},
            {"show_damage_numbers", m_gameplay.show_damage_numbers},
            {"hud_mode", static_cast<int>(m_gameplay.hud_mode)},
            {"tutorials_enabled", m_gameplay.tutorials_enabled},
            {"text_language", m_gameplay.text_language},
            {"voice_language", m_gameplay.voice_language}
        };

        std::ofstream file(path);
        if (!file.is_open()) {
            core::log(core::LogLevel::Error, "[Settings] Could not open settings file for writing: {}", path);
            return false;
        }

        file << j.dump(4);
        core::log(core::LogLevel::Info, "[Settings] Saved settings to: {}", path);
        return true;

    } catch (const std::exception& e) {
        core::log(core::LogLevel::Error, "[Settings] Failed to save settings: {}", e.what());
        return false;
    }
}

std::string SettingsManager::get_default_path() const {
    return "settings.json";
}

void SettingsManager::reset_to_defaults() {
    reset_graphics();
    reset_audio();
    reset_input();
    reset_gameplay();
}

void SettingsManager::reset_graphics() {
    m_graphics = GraphicsSettings{};
    notify_changed(SettingsCategory::Graphics);
}

void SettingsManager::reset_audio() {
    m_audio = AudioSettings{};
    notify_changed(SettingsCategory::Audio);
}

void SettingsManager::reset_input() {
    m_input = InputSettings{};
    setup_default_bindings();
    notify_changed(SettingsCategory::Input);
}

void SettingsManager::reset_gameplay() {
    m_gameplay = GameplaySettings{};
    notify_changed(SettingsCategory::Gameplay);
}

// ============================================================================
// Apply Changes
// ============================================================================

void SettingsManager::apply_graphics() {
    m_graphics.validate();
    notify_changed(SettingsCategory::Graphics);

    // TODO: Notify renderer to apply changes
    core::log(core::LogLevel::Debug, "[Settings] Applied graphics settings");
}

void SettingsManager::apply_audio() {
    m_audio.validate();
    notify_changed(SettingsCategory::Audio);

    // TODO: Notify audio engine to apply changes
    core::log(core::LogLevel::Debug, "[Settings] Applied audio settings");
}

void SettingsManager::apply_input() {
    m_input.validate();
    notify_changed(SettingsCategory::Input);

    // TODO: Notify input system to apply changes
    core::log(core::LogLevel::Debug, "[Settings] Applied input settings");
}

void SettingsManager::apply_gameplay() {
    m_gameplay.validate();
    notify_changed(SettingsCategory::Gameplay);

    core::log(core::LogLevel::Debug, "[Settings] Applied gameplay settings");
}

void SettingsManager::apply_all() {
    apply_graphics();
    apply_audio();
    apply_input();
    apply_gameplay();
}

void SettingsManager::validate_all() {
    m_graphics.validate();
    m_audio.validate();
    m_input.validate();
    m_gameplay.validate();
}

// ============================================================================
// Graphics Presets
// ============================================================================

void SettingsManager::apply_graphics_preset(QualityPreset preset) {
    m_graphics.apply_preset(preset);
    apply_graphics();
}

QualityPreset SettingsManager::detect_optimal_preset() const {
    // TODO: Detect based on GPU capabilities
    return QualityPreset::High;
}

std::vector<SettingsManager::Resolution> SettingsManager::get_available_resolutions() const {
    // TODO: Query from display system
    return {
        {1280, 720, 60},
        {1920, 1080, 60},
        {2560, 1440, 60},
        {3840, 2160, 60}
    };
}

// ============================================================================
// Audio Devices
// ============================================================================

std::vector<SettingsManager::AudioDevice> SettingsManager::get_output_devices() const {
    // TODO: Query from audio system
    return {{0, "Default Device", true}};
}

std::vector<SettingsManager::AudioDevice> SettingsManager::get_input_devices() const {
    // TODO: Query from audio system
    return {{0, "Default Microphone", true}};
}

// ============================================================================
// Keybinding Helpers
// ============================================================================

void SettingsManager::bind_action(const std::string& action, KeyCode key, bool secondary) {
    if (secondary) {
        if (m_input.bindings.contains(action)) {
            m_input.bindings[action].secondary_key = key;
        }
    } else {
        m_input.set_binding(action, key);
    }
    m_dirty = true;
}

void SettingsManager::bind_action(const std::string& action, GamepadButton button) {
    m_input.set_binding(action, button);
    m_dirty = true;
}

void SettingsManager::unbind_action(const std::string& action) {
    m_input.clear_binding(action);
    m_dirty = true;
}

std::string SettingsManager::get_binding_display(const std::string& action) const {
    const auto* binding = m_input.get_binding(action);
    if (!binding) return "Unbound";

    std::string result;
    if (binding->primary_key != KeyCode::None) {
        result = get_key_name(binding->primary_key);
    }
    if (binding->gamepad_button != GamepadButton::None) {
        if (!result.empty()) result += " / ";
        result += get_button_name(binding->gamepad_button);
    }
    return result.empty() ? "Unbound" : result;
}

std::vector<std::string> SettingsManager::get_conflicting_bindings(const std::string& action, KeyCode key) const {
    std::vector<std::string> conflicts;
    for (const auto& [other_action, binding] : m_input.bindings) {
        if (other_action != action) {
            if (binding.primary_key == key || binding.secondary_key == key) {
                conflicts.push_back(other_action);
            }
        }
    }
    return conflicts;
}

std::vector<std::string> SettingsManager::get_conflicting_bindings(const std::string& action, GamepadButton button) const {
    std::vector<std::string> conflicts;
    for (const auto& [other_action, binding] : m_input.bindings) {
        if (other_action != action && binding.gamepad_button == button) {
            conflicts.push_back(other_action);
        }
    }
    return conflicts;
}

// ============================================================================
// Callbacks
// ============================================================================

void SettingsManager::set_on_settings_changed(SettingsCallback callback) {
    m_on_settings_changed = std::move(callback);
}

void SettingsManager::set_on_graphics_changed(std::function<void()> callback) {
    m_on_graphics_changed = std::move(callback);
}

void SettingsManager::set_on_audio_changed(std::function<void()> callback) {
    m_on_audio_changed = std::move(callback);
}

void SettingsManager::set_on_input_changed(std::function<void()> callback) {
    m_on_input_changed = std::move(callback);
}

void SettingsManager::set_on_gameplay_changed(std::function<void()> callback) {
    m_on_gameplay_changed = std::move(callback);
}

// ============================================================================
// Dirty Tracking
// ============================================================================

bool SettingsManager::has_unsaved_changes() const {
    return m_graphics != m_graphics_saved ||
           m_audio != m_audio_saved ||
           !(m_input == m_input_saved) ||
           m_gameplay != m_gameplay_saved;
}

void SettingsManager::mark_saved() {
    m_graphics_saved = m_graphics;
    m_audio_saved = m_audio;
    m_input_saved = m_input;
    m_gameplay_saved = m_gameplay;
    m_dirty = false;
}

// ============================================================================
// Internal
// ============================================================================

void SettingsManager::notify_changed(SettingsCategory category) {
    m_dirty = true;

    if (m_on_settings_changed) {
        m_on_settings_changed(category);
    }

    switch (category) {
        case SettingsCategory::Graphics:
            if (m_on_graphics_changed) m_on_graphics_changed();
            break;
        case SettingsCategory::Audio:
            if (m_on_audio_changed) m_on_audio_changed();
            break;
        case SettingsCategory::Input:
            if (m_on_input_changed) m_on_input_changed();
            break;
        case SettingsCategory::Gameplay:
            if (m_on_gameplay_changed) m_on_gameplay_changed();
            break;
        case SettingsCategory::All:
            if (m_on_graphics_changed) m_on_graphics_changed();
            if (m_on_audio_changed) m_on_audio_changed();
            if (m_on_input_changed) m_on_input_changed();
            if (m_on_gameplay_changed) m_on_gameplay_changed();
            break;
    }

    // Publish event
    SettingsChangedEvent event;
    event.category = category;
    core::game_events().broadcast(event);
}

void SettingsManager::setup_default_bindings() {
    m_input.reset_to_defaults();
}

} // namespace engine::settings
