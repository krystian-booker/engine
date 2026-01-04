#pragma once

#include <engine/ui/ui_types.hpp>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine::ui {

// Key codes for shortcuts (subset of common keys)
// These are abstract key codes - platform-specific mapping happens in Application
enum class Key : uint32_t {
    None = 0,

    // Letters
    A = 'A', B = 'B', C = 'C', D = 'D', E = 'E', F = 'F', G = 'G', H = 'H',
    I = 'I', J = 'J', K = 'K', L = 'L', M = 'M', N = 'N', O = 'O', P = 'P',
    Q = 'Q', R = 'R', S = 'S', T = 'T', U = 'U', V = 'V', W = 'W', X = 'X',
    Y = 'Y', Z = 'Z',

    // Numbers
    Num0 = '0', Num1 = '1', Num2 = '2', Num3 = '3', Num4 = '4',
    Num5 = '5', Num6 = '6', Num7 = '7', Num8 = '8', Num9 = '9',

    // Function keys
    F1 = 256, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    // Special keys
    Escape = 300,
    Enter,
    Tab,
    Backspace,
    Delete,
    Insert,
    Home,
    End,
    PageUp,
    PageDown,
    Left,
    Right,
    Up,
    Down,
    Space,

    // Punctuation
    Minus,
    Equals,
    LeftBracket,
    RightBracket,
    Backslash,
    Semicolon,
    Apostrophe,
    Comma,
    Period,
    Slash,
    Grave
};

// Key combination with modifiers
struct KeyCombo {
    Key key = Key::None;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;

    KeyCombo() = default;
    explicit KeyCombo(Key k) : key(k) {}
    KeyCombo(Key k, bool c, bool s, bool a) : key(k), ctrl(c), shift(s), alt(a) {}

    // Convenience constructors
    static KeyCombo Ctrl(Key k) { return KeyCombo(k, true, false, false); }
    static KeyCombo Shift(Key k) { return KeyCombo(k, false, true, false); }
    static KeyCombo Alt(Key k) { return KeyCombo(k, false, false, true); }
    static KeyCombo CtrlShift(Key k) { return KeyCombo(k, true, true, false); }
    static KeyCombo CtrlAlt(Key k) { return KeyCombo(k, true, false, true); }

    bool operator==(const KeyCombo& other) const {
        return key == other.key && ctrl == other.ctrl &&
               shift == other.shift && alt == other.alt;
    }

    bool operator!=(const KeyCombo& other) const {
        return !(*this == other);
    }

    // Get display string (e.g., "Ctrl+S", "F5")
    std::string to_string() const;

    // Parse from string (e.g., "Ctrl+S", "Alt+F4")
    static KeyCombo from_string(const std::string& str);
};

// Extended input state that includes key states for shortcuts
struct ShortcutInputState {
    // Currently held modifier keys
    bool ctrl_held = false;
    bool shift_held = false;
    bool alt_held = false;

    // Keys pressed this frame (cleared each frame)
    std::vector<Key> keys_pressed;

    // Check if a specific key combination was just pressed
    bool was_pressed(const KeyCombo& combo) const;

    // Clear per-frame state
    void clear_frame() {
        keys_pressed.clear();
    }
};

// Callback type for shortcut actions
using ShortcutCallback = std::function<void()>;

// Manages keyboard shortcuts for the UI system
class UIShortcutManager {
public:
    UIShortcutManager() = default;
    ~UIShortcutManager() = default;

    // Register a shortcut
    // action_id: Unique identifier for this action (e.g., "save", "undo", "close_dialog")
    // combo: Key combination that triggers the action
    // callback: Function to call when shortcut is triggered
    // scope: Optional scope name - shortcut only active when this scope is active
    void register_shortcut(const std::string& action_id, KeyCombo combo,
                           ShortcutCallback callback, const std::string& scope = "");

    // Unregister a shortcut
    void unregister_shortcut(const std::string& action_id);

    // Enable/disable a specific shortcut
    void set_enabled(const std::string& action_id, bool enabled);
    bool is_enabled(const std::string& action_id) const;

    // Update the key combination for an existing shortcut (for remapping)
    void set_key_combo(const std::string& action_id, KeyCombo combo);
    KeyCombo get_key_combo(const std::string& action_id) const;

    // Scope management - shortcuts only active when their scope is in the stack
    // Global scope ("") is always active
    void push_scope(const std::string& scope);
    void pop_scope();
    void clear_scopes();
    const std::string& current_scope() const;

    // Process input and trigger matching shortcuts
    // Call this each frame after updating ShortcutInputState
    void process_input(const ShortcutInputState& input);

    // Get all registered shortcuts (for displaying in settings UI)
    struct ShortcutInfo {
        std::string action_id;
        KeyCombo combo;
        std::string scope;
        bool enabled;
    };
    std::vector<ShortcutInfo> get_all_shortcuts() const;

    // Check if any shortcut uses a specific key combo (for conflict detection)
    bool is_combo_used(KeyCombo combo, const std::string& exclude_action = "") const;

    // Block/unblock all shortcuts (useful during text input)
    void set_blocked(bool blocked) { m_blocked = blocked; }
    bool is_blocked() const { return m_blocked; }

private:
    struct Shortcut {
        KeyCombo combo;
        ShortcutCallback callback;
        std::string scope;
        bool enabled = true;
    };

    std::unordered_map<std::string, Shortcut> m_shortcuts;
    std::vector<std::string> m_scope_stack;
    bool m_blocked = false;

    bool is_scope_active(const std::string& scope) const;
};

} // namespace engine::ui
