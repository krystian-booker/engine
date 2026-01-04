#pragma once

#include <engine/ui/ui_element.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace engine::ui {

// Forward declaration
class UIPopupMenu;

// Menu item types
enum class MenuItemType : uint8_t {
    Normal,     // Regular clickable item
    Separator,  // Horizontal divider line
    Submenu     // Item that opens a submenu
};

// Single menu item
struct PopupMenuItem {
    std::string id;              // Unique identifier for callbacks
    std::string label;           // Display text
    std::string label_key;       // Localization key (optional)
    std::string shortcut_text;   // Shortcut hint (e.g., "Ctrl+C") - display only
    MenuItemType type = MenuItemType::Normal;
    bool enabled = true;
    bool checked = false;        // For toggle items

    // Submenu items (only used if type == Submenu)
    std::vector<PopupMenuItem> submenu_items;

    // Convenience constructors
    PopupMenuItem() = default;

    PopupMenuItem(const std::string& item_id, const std::string& item_label)
        : id(item_id), label(item_label) {}

    PopupMenuItem(const std::string& item_id, const std::string& item_label, const std::string& shortcut)
        : id(item_id), label(item_label), shortcut_text(shortcut) {}

    // Create separator
    static PopupMenuItem separator() {
        PopupMenuItem item;
        item.type = MenuItemType::Separator;
        return item;
    }

    // Create submenu
    static PopupMenuItem submenu(const std::string& label, std::vector<PopupMenuItem> items) {
        PopupMenuItem item;
        item.label = label;
        item.type = MenuItemType::Submenu;
        item.submenu_items = std::move(items);
        return item;
    }
};

// Callback for menu item selection
using MenuItemCallback = std::function<void(const std::string& id)>;

// Popup/context menu that appears on demand
class UIPopupMenu : public UIElement {
public:
    UIPopupMenu();
    ~UIPopupMenu() override = default;

    // Add items
    void add_item(const std::string& id, const std::string& label);
    void add_item(const std::string& id, const std::string& label, const std::string& shortcut);
    void add_item(const PopupMenuItem& item);
    void add_separator();
    void add_submenu(const std::string& label, std::vector<PopupMenuItem> items);

    // Clear all items
    void clear_items();

    // Get items
    const std::vector<PopupMenuItem>& get_items() const { return m_items; }

    // Set/get item enabled state
    void set_item_enabled(const std::string& id, bool enabled);
    bool is_item_enabled(const std::string& id) const;

    // Set/get item checked state (for toggle items)
    void set_item_checked(const std::string& id, bool checked);
    bool is_item_checked(const std::string& id) const;

    // Show/hide menu
    void show_at(Vec2 screen_position);
    void show_at(float x, float y) { show_at(Vec2(x, y)); }
    void hide();
    bool is_visible() const { return m_visible; }

    // Visual settings
    void set_item_height(float height) { m_item_height = height; }
    float get_item_height() const { return m_item_height; }

    void set_min_width(float width) { m_min_width = width; }
    float get_min_width() const { return m_min_width; }

    void set_background_color(const Vec4& color) { m_bg_color = color; }
    void set_hover_color(const Vec4& color) { m_hover_color = color; }
    void set_separator_color(const Vec4& color) { m_separator_color = color; }
    void set_text_color(const Vec4& color) { m_text_color = color; }
    void set_disabled_text_color(const Vec4& color) { m_disabled_text_color = color; }
    void set_shortcut_color(const Vec4& color) { m_shortcut_color = color; }

    // Callback when item is selected
    MenuItemCallback on_item_selected;

    // Callback when menu is dismissed without selection
    std::function<void()> on_dismissed;

    void render(UIRenderContext& ctx) override;

protected:
    void on_update(float dt, const UIInputState& input) override;
    void on_render(UIRenderContext& ctx) override;
    Vec2 on_measure(Vec2 available_size) override;

private:
    struct ItemLayout {
        Rect bounds;
        bool is_separator;
    };

    void calculate_layout();
    int get_item_at_position(Vec2 pos) const;
    std::string get_resolved_label(const PopupMenuItem& item) const;
    void close_submenu();
    void open_submenu(int index, const PopupMenuItem& item);

    std::vector<PopupMenuItem> m_items;
    std::vector<ItemLayout> m_item_layouts;

    Vec2 m_position{0.0f};
    bool m_visible = false;
    int m_hovered_index = -1;

    // Submenu handling
    std::unique_ptr<UIPopupMenu> m_active_submenu;
    int m_submenu_parent_index = -1;

    // Visual settings
    float m_item_height = 28.0f;
    float m_separator_height = 9.0f;
    float m_min_width = 150.0f;
    float m_padding = 4.0f;
    float m_submenu_arrow_width = 16.0f;

    Vec4 m_bg_color{0.2f, 0.2f, 0.2f, 0.95f};
    Vec4 m_hover_color{0.3f, 0.5f, 0.9f, 1.0f};
    Vec4 m_separator_color{0.4f, 0.4f, 0.4f, 1.0f};
    Vec4 m_text_color{1.0f, 1.0f, 1.0f, 1.0f};
    Vec4 m_disabled_text_color{0.5f, 0.5f, 0.5f, 1.0f};
    Vec4 m_shortcut_color{0.7f, 0.7f, 0.7f, 1.0f};
    Vec4 m_check_color{0.3f, 0.8f, 0.3f, 1.0f};

    float m_border_radius = 4.0f;
};

// Helper to create a context menu quickly
inline std::unique_ptr<UIPopupMenu> make_context_menu(
    const std::vector<std::pair<std::string, std::string>>& items) {

    auto menu = std::make_unique<UIPopupMenu>();
    for (const auto& [id, label] : items) {
        if (id == "-" || label == "-") {
            menu->add_separator();
        } else {
            menu->add_item(id, label);
        }
    }
    return menu;
}

} // namespace engine::ui
