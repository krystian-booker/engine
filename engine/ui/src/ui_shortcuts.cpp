#include <engine/ui/ui_shortcuts.hpp>
#include <algorithm>
#include <sstream>
#include <cctype>

namespace engine::ui {

std::string KeyCombo::to_string() const {
    if (key == Key::None) {
        return "";
    }

    std::string result;

    if (ctrl) {
        result += "Ctrl+";
    }
    if (alt) {
        result += "Alt+";
    }
    if (shift) {
        result += "Shift+";
    }

    // Convert key to string
    switch (key) {
        // Letters
        case Key::A: result += "A"; break;
        case Key::B: result += "B"; break;
        case Key::C: result += "C"; break;
        case Key::D: result += "D"; break;
        case Key::E: result += "E"; break;
        case Key::F: result += "F"; break;
        case Key::G: result += "G"; break;
        case Key::H: result += "H"; break;
        case Key::I: result += "I"; break;
        case Key::J: result += "J"; break;
        case Key::K: result += "K"; break;
        case Key::L: result += "L"; break;
        case Key::M: result += "M"; break;
        case Key::N: result += "N"; break;
        case Key::O: result += "O"; break;
        case Key::P: result += "P"; break;
        case Key::Q: result += "Q"; break;
        case Key::R: result += "R"; break;
        case Key::S: result += "S"; break;
        case Key::T: result += "T"; break;
        case Key::U: result += "U"; break;
        case Key::V: result += "V"; break;
        case Key::W: result += "W"; break;
        case Key::X: result += "X"; break;
        case Key::Y: result += "Y"; break;
        case Key::Z: result += "Z"; break;

        // Numbers
        case Key::Num0: result += "0"; break;
        case Key::Num1: result += "1"; break;
        case Key::Num2: result += "2"; break;
        case Key::Num3: result += "3"; break;
        case Key::Num4: result += "4"; break;
        case Key::Num5: result += "5"; break;
        case Key::Num6: result += "6"; break;
        case Key::Num7: result += "7"; break;
        case Key::Num8: result += "8"; break;
        case Key::Num9: result += "9"; break;

        // Function keys
        case Key::F1: result += "F1"; break;
        case Key::F2: result += "F2"; break;
        case Key::F3: result += "F3"; break;
        case Key::F4: result += "F4"; break;
        case Key::F5: result += "F5"; break;
        case Key::F6: result += "F6"; break;
        case Key::F7: result += "F7"; break;
        case Key::F8: result += "F8"; break;
        case Key::F9: result += "F9"; break;
        case Key::F10: result += "F10"; break;
        case Key::F11: result += "F11"; break;
        case Key::F12: result += "F12"; break;

        // Special keys
        case Key::Escape: result += "Escape"; break;
        case Key::Enter: result += "Enter"; break;
        case Key::Tab: result += "Tab"; break;
        case Key::Backspace: result += "Backspace"; break;
        case Key::Delete: result += "Delete"; break;
        case Key::Insert: result += "Insert"; break;
        case Key::Home: result += "Home"; break;
        case Key::End: result += "End"; break;
        case Key::PageUp: result += "PageUp"; break;
        case Key::PageDown: result += "PageDown"; break;
        case Key::Left: result += "Left"; break;
        case Key::Right: result += "Right"; break;
        case Key::Up: result += "Up"; break;
        case Key::Down: result += "Down"; break;
        case Key::Space: result += "Space"; break;

        default: result += "?"; break;
    }

    return result;
}

KeyCombo KeyCombo::from_string(const std::string& str) {
    KeyCombo combo;

    // Parse modifiers and key from string like "Ctrl+Shift+S"
    std::string remaining = str;

    // Check for modifiers
    auto to_upper = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        return s;
    };

    while (true) {
        size_t plus_pos = remaining.find('+');
        if (plus_pos == std::string::npos) {
            break;
        }

        std::string modifier = to_upper(remaining.substr(0, plus_pos));
        remaining = remaining.substr(plus_pos + 1);

        if (modifier == "CTRL" || modifier == "CONTROL") {
            combo.ctrl = true;
        } else if (modifier == "ALT") {
            combo.alt = true;
        } else if (modifier == "SHIFT") {
            combo.shift = true;
        }
    }

    // Parse key
    std::string key_str = to_upper(remaining);

    // Single character keys
    if (key_str.length() == 1) {
        char c = key_str[0];
        if (c >= 'A' && c <= 'Z') {
            combo.key = static_cast<Key>(c);
        } else if (c >= '0' && c <= '9') {
            combo.key = static_cast<Key>(c);
        }
    }
    // Function keys
    else if (key_str.length() >= 2 && key_str[0] == 'F') {
        int num = std::atoi(key_str.c_str() + 1);
        if (num >= 1 && num <= 12) {
            combo.key = static_cast<Key>(static_cast<int>(Key::F1) + num - 1);
        }
    }
    // Special keys
    else if (key_str == "ESCAPE" || key_str == "ESC") { combo.key = Key::Escape; }
    else if (key_str == "ENTER" || key_str == "RETURN") { combo.key = Key::Enter; }
    else if (key_str == "TAB") { combo.key = Key::Tab; }
    else if (key_str == "BACKSPACE") { combo.key = Key::Backspace; }
    else if (key_str == "DELETE" || key_str == "DEL") { combo.key = Key::Delete; }
    else if (key_str == "INSERT" || key_str == "INS") { combo.key = Key::Insert; }
    else if (key_str == "HOME") { combo.key = Key::Home; }
    else if (key_str == "END") { combo.key = Key::End; }
    else if (key_str == "PAGEUP" || key_str == "PGUP") { combo.key = Key::PageUp; }
    else if (key_str == "PAGEDOWN" || key_str == "PGDN") { combo.key = Key::PageDown; }
    else if (key_str == "LEFT") { combo.key = Key::Left; }
    else if (key_str == "RIGHT") { combo.key = Key::Right; }
    else if (key_str == "UP") { combo.key = Key::Up; }
    else if (key_str == "DOWN") { combo.key = Key::Down; }
    else if (key_str == "SPACE") { combo.key = Key::Space; }

    return combo;
}

bool ShortcutInputState::was_pressed(const KeyCombo& combo) const {
    // Check modifiers
    if (combo.ctrl != ctrl_held) return false;
    if (combo.shift != shift_held) return false;
    if (combo.alt != alt_held) return false;

    // Check if key was pressed this frame
    for (Key k : keys_pressed) {
        if (k == combo.key) {
            return true;
        }
    }

    return false;
}

void UIShortcutManager::register_shortcut(const std::string& action_id, KeyCombo combo,
                                           ShortcutCallback callback, const std::string& scope) {
    m_shortcuts[action_id] = Shortcut{combo, std::move(callback), scope, true};
}

void UIShortcutManager::unregister_shortcut(const std::string& action_id) {
    m_shortcuts.erase(action_id);
}

void UIShortcutManager::set_enabled(const std::string& action_id, bool enabled) {
    auto it = m_shortcuts.find(action_id);
    if (it != m_shortcuts.end()) {
        it->second.enabled = enabled;
    }
}

bool UIShortcutManager::is_enabled(const std::string& action_id) const {
    auto it = m_shortcuts.find(action_id);
    return it != m_shortcuts.end() && it->second.enabled;
}

void UIShortcutManager::set_key_combo(const std::string& action_id, KeyCombo combo) {
    auto it = m_shortcuts.find(action_id);
    if (it != m_shortcuts.end()) {
        it->second.combo = combo;
    }
}

KeyCombo UIShortcutManager::get_key_combo(const std::string& action_id) const {
    auto it = m_shortcuts.find(action_id);
    if (it != m_shortcuts.end()) {
        return it->second.combo;
    }
    return KeyCombo{};
}

void UIShortcutManager::push_scope(const std::string& scope) {
    m_scope_stack.push_back(scope);
}

void UIShortcutManager::pop_scope() {
    if (!m_scope_stack.empty()) {
        m_scope_stack.pop_back();
    }
}

void UIShortcutManager::clear_scopes() {
    m_scope_stack.clear();
}

const std::string& UIShortcutManager::current_scope() const {
    static const std::string empty;
    return m_scope_stack.empty() ? empty : m_scope_stack.back();
}

bool UIShortcutManager::is_scope_active(const std::string& scope) const {
    // Global scope (empty string) is always active
    if (scope.empty()) {
        return true;
    }

    // Check if scope is in the stack
    for (const auto& s : m_scope_stack) {
        if (s == scope) {
            return true;
        }
    }

    return false;
}

void UIShortcutManager::process_input(const ShortcutInputState& input) {
    if (m_blocked) {
        return;
    }

    // Check each shortcut
    for (const auto& [action_id, shortcut] : m_shortcuts) {
        if (!shortcut.enabled) {
            continue;
        }

        if (!is_scope_active(shortcut.scope)) {
            continue;
        }

        if (input.was_pressed(shortcut.combo)) {
            if (shortcut.callback) {
                shortcut.callback();
            }
            // Only trigger first matching shortcut
            // This prevents issues with overlapping shortcuts
            break;
        }
    }
}

std::vector<UIShortcutManager::ShortcutInfo> UIShortcutManager::get_all_shortcuts() const {
    std::vector<ShortcutInfo> result;
    result.reserve(m_shortcuts.size());

    for (const auto& [action_id, shortcut] : m_shortcuts) {
        result.push_back({action_id, shortcut.combo, shortcut.scope, shortcut.enabled});
    }

    // Sort by action_id for consistent display
    std::sort(result.begin(), result.end(),
              [](const ShortcutInfo& a, const ShortcutInfo& b) {
                  return a.action_id < b.action_id;
              });

    return result;
}

bool UIShortcutManager::is_combo_used(KeyCombo combo, const std::string& exclude_action) const {
    for (const auto& [action_id, shortcut] : m_shortcuts) {
        if (action_id != exclude_action && shortcut.combo == combo) {
            return true;
        }
    }
    return false;
}

} // namespace engine::ui
