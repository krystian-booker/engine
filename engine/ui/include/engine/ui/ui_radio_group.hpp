#pragma once

#include <engine/ui/ui_element.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace engine::ui {

// Radio button option data
struct RadioOption {
    std::string id;              // Unique identifier
    std::string label;           // Display text
    std::string label_key;       // Localization key (optional)
    bool enabled = true;         // Whether option can be selected
};

// Selection changed callback
using RadioSelectionCallback = std::function<void(const std::string& id)>;

// Radio button group - mutually exclusive toggle selection
// Only one option can be selected at a time
class UIRadioGroup : public UIElement {
public:
    UIRadioGroup();
    ~UIRadioGroup() override = default;

    // Add options
    void add_option(const std::string& id, const std::string& label);
    void add_option(const RadioOption& option);
    void add_options(const std::vector<RadioOption>& options);

    // Remove options
    void remove_option(const std::string& id);
    void clear_options();

    // Get options
    const std::vector<RadioOption>& get_options() const { return m_options; }
    size_t get_option_count() const { return m_options.size(); }

    // Selection
    void set_selected(const std::string& id);
    const std::string& get_selected() const { return m_selected_id; }
    int get_selected_index() const;

    // Select by index
    void select_index(int index);

    // Enable/disable specific options
    void set_option_enabled(const std::string& id, bool enabled);
    bool is_option_enabled(const std::string& id) const;

    // Layout settings
    void set_orientation(LayoutDirection orientation) { m_orientation = orientation; mark_layout_dirty(); }
    LayoutDirection get_orientation() const { return m_orientation; }

    void set_spacing(float spacing) { m_spacing = spacing; mark_layout_dirty(); }
    float get_spacing() const { return m_spacing; }

    // Visual settings
    void set_radio_size(float size) { m_radio_size = size; mark_dirty(); }
    float get_radio_size() const { return m_radio_size; }

    void set_radio_color(const Vec4& color) { m_radio_color = color; }
    const Vec4& get_radio_color() const { return m_radio_color; }

    void set_radio_selected_color(const Vec4& color) { m_radio_selected_color = color; }
    const Vec4& get_radio_selected_color() const { return m_radio_selected_color; }

    void set_radio_border_color(const Vec4& color) { m_radio_border_color = color; }
    const Vec4& get_radio_border_color() const { return m_radio_border_color; }

    void set_radio_disabled_color(const Vec4& color) { m_radio_disabled_color = color; }
    const Vec4& get_radio_disabled_color() const { return m_radio_disabled_color; }

    // Callbacks
    RadioSelectionCallback on_selection_changed;

protected:
    void on_update(float dt, const UIInputState& input) override;
    void on_render(UIRenderContext& ctx) override;
    Vec2 on_measure(Vec2 available_size) override;
    void on_layout(const Rect& bounds) override;

private:
    struct OptionLayout {
        Rect radio_bounds;    // Circle/indicator bounds
        Rect label_bounds;    // Text bounds
        Rect total_bounds;    // Total clickable area
    };

    int get_option_at_position(Vec2 pos) const;
    std::string get_resolved_label(const RadioOption& option) const;
    void update_option_layouts();

    std::vector<RadioOption> m_options;
    std::vector<OptionLayout> m_option_layouts;
    std::string m_selected_id;

    LayoutDirection m_orientation = LayoutDirection::Vertical;
    float m_spacing = 8.0f;
    float m_radio_size = 18.0f;
    float m_label_padding = 8.0f;  // Space between radio and label

    Vec4 m_radio_color{0.2f, 0.2f, 0.2f, 1.0f};
    Vec4 m_radio_selected_color{0.3f, 0.5f, 0.9f, 1.0f};
    Vec4 m_radio_border_color{0.5f, 0.5f, 0.5f, 1.0f};
    Vec4 m_radio_disabled_color{0.4f, 0.4f, 0.4f, 0.5f};

    int m_hovered_index = -1;
};

// Factory function for creating a radio group with options
inline std::unique_ptr<UIRadioGroup> make_radio_group(
    const std::vector<std::pair<std::string, std::string>>& options,
    const std::string& default_selection = "") {

    auto group = std::make_unique<UIRadioGroup>();
    for (const auto& [id, label] : options) {
        group->add_option(id, label);
    }
    if (!default_selection.empty()) {
        group->set_selected(default_selection);
    } else if (!options.empty()) {
        group->set_selected(options[0].first);
    }
    return group;
}

} // namespace engine::ui
