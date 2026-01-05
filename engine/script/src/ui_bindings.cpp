#include <engine/script/bindings.hpp>
#include <engine/ui/ui_context.hpp>
#include <engine/ui/ui_canvas.hpp>
#include <engine/ui/ui_elements.hpp>
#include <engine/core/log.hpp>
#include <unordered_map>

namespace engine::script {

// Static storage for Lua callbacks (keyed by canvas/element name)
static std::unordered_map<std::string, sol::function> s_click_callbacks;
static std::unordered_map<std::string, sol::function> s_hover_callbacks;
static std::unordered_map<std::string, sol::function> s_value_changed_callbacks;
static std::unordered_map<std::string, sol::function> s_text_changed_callbacks;

// Helper to get UIContext safely
static engine::ui::UIContext* get_ui_ctx() {
    auto* ctx = engine::ui::get_ui_context();
    if (!ctx) {
        engine::core::log(engine::core::LogLevel::Warn, "UI function called without UI context");
    }
    return ctx;
}

// Helper to find element
static engine::ui::UIElement* find_element(const std::string& canvas_name, const std::string& element_name) {
    auto* ctx = get_ui_ctx();
    if (!ctx) return nullptr;

    auto* canvas = ctx->get_canvas(canvas_name);
    if (!canvas || !canvas->get_root()) return nullptr;

    if (canvas->get_root()->get_name() == element_name) {
        return canvas->get_root();
    }
    return canvas->get_root()->find_child(element_name);
}

// Helper to make callback key
static std::string make_callback_key(const std::string& canvas, const std::string& element) {
    return canvas + "/" + element;
}

void register_ui_bindings(sol::state& lua) {
    using namespace engine::ui;
    using namespace engine::core;

    // Create UI table
    auto ui = lua.create_named_table("UI");

    // --- Canvas Management ---

    ui.set_function("create_canvas", [](const std::string& name) -> bool {
        auto* ctx = get_ui_ctx();
        if (!ctx) return false;
        return ctx->create_canvas(name) != nullptr;
    });

    ui.set_function("destroy_canvas", [](const std::string& name) {
        auto* ctx = get_ui_ctx();
        if (ctx) {
            ctx->destroy_canvas(name);
        }
    });

    ui.set_function("has_canvas", [](const std::string& name) -> bool {
        auto* ctx = get_ui_ctx();
        return ctx && ctx->get_canvas(name) != nullptr;
    });

    ui.set_function("show_canvas", [](const std::string& name, bool visible) {
        auto* ctx = get_ui_ctx();
        if (!ctx) return;
        auto* canvas = ctx->get_canvas(name);
        if (canvas) {
            canvas->set_enabled(visible);
        }
    });

    ui.set_function("is_canvas_visible", [](const std::string& name) -> bool {
        auto* ctx = get_ui_ctx();
        if (!ctx) return false;
        auto* canvas = ctx->get_canvas(name);
        return canvas && canvas->is_enabled();
    });

    ui.set_function("set_canvas_sort_order", [](const std::string& name, int order) {
        auto* ctx = get_ui_ctx();
        if (!ctx) return;
        auto* canvas = ctx->get_canvas(name);
        if (canvas) {
            canvas->set_sort_order(order);
        }
    });

    // --- Element Creation ---

    ui.set_function("create_panel", [](const std::string& canvas_name, const std::string& element_name) -> bool {
        auto* ctx = get_ui_ctx();
        if (!ctx) return false;
        auto* canvas = ctx->get_canvas(canvas_name);
        if (!canvas) return false;

        auto panel = std::make_unique<UIPanel>();
        panel->set_name(element_name);

        if (!canvas->get_root()) {
            canvas->set_root(std::move(panel));
        } else {
            canvas->get_root()->add_child(std::move(panel));
        }
        return true;
    });

    ui.set_function("create_label", [](const std::string& canvas_name, const std::string& parent_name,
                                       const std::string& element_name, const std::string& text) -> bool {
        auto* ctx = get_ui_ctx();
        if (!ctx) return false;
        auto* canvas = ctx->get_canvas(canvas_name);
        if (!canvas || !canvas->get_root()) return false;

        UIElement* parent = canvas->get_root()->find_child(parent_name);
        if (!parent) parent = canvas->get_root();

        auto label = std::make_unique<UILabel>(text);
        label->set_name(element_name);
        parent->add_child(std::move(label));
        return true;
    });

    ui.set_function("create_button", [](const std::string& canvas_name, const std::string& parent_name,
                                        const std::string& element_name, const std::string& text) -> bool {
        auto* ctx = get_ui_ctx();
        if (!ctx) return false;
        auto* canvas = ctx->get_canvas(canvas_name);
        if (!canvas || !canvas->get_root()) return false;

        UIElement* parent = canvas->get_root()->find_child(parent_name);
        if (!parent) parent = canvas->get_root();

        auto button = std::make_unique<UIButton>(text);
        button->set_name(element_name);
        button->set_interactive(true);
        parent->add_child(std::move(button));
        return true;
    });

    ui.set_function("create_image", [](const std::string& canvas_name, const std::string& parent_name,
                                       const std::string& element_name) -> bool {
        auto* ctx = get_ui_ctx();
        if (!ctx) return false;
        auto* canvas = ctx->get_canvas(canvas_name);
        if (!canvas || !canvas->get_root()) return false;

        UIElement* parent = canvas->get_root()->find_child(parent_name);
        if (!parent) parent = canvas->get_root();

        auto image = std::make_unique<UIImage>();
        image->set_name(element_name);
        parent->add_child(std::move(image));
        return true;
    });

    ui.set_function("create_progress_bar", [](const std::string& canvas_name, const std::string& parent_name,
                                              const std::string& element_name) -> bool {
        auto* ctx = get_ui_ctx();
        if (!ctx) return false;
        auto* canvas = ctx->get_canvas(canvas_name);
        if (!canvas || !canvas->get_root()) return false;

        UIElement* parent = canvas->get_root()->find_child(parent_name);
        if (!parent) parent = canvas->get_root();

        auto bar = std::make_unique<UIProgressBar>();
        bar->set_name(element_name);
        parent->add_child(std::move(bar));
        return true;
    });

    ui.set_function("create_slider", [](const std::string& canvas_name, const std::string& parent_name,
                                        const std::string& element_name,
                                        sol::optional<float> min_val,
                                        sol::optional<float> max_val) -> bool {
        auto* ctx = get_ui_ctx();
        if (!ctx) return false;
        auto* canvas = ctx->get_canvas(canvas_name);
        if (!canvas || !canvas->get_root()) return false;

        UIElement* parent = canvas->get_root()->find_child(parent_name);
        if (!parent) parent = canvas->get_root();

        auto slider = std::make_unique<UISlider>();
        slider->set_name(element_name);
        slider->set_interactive(true);
        if (min_val.has_value() && max_val.has_value()) {
            slider->set_range(min_val.value(), max_val.value());
        }
        parent->add_child(std::move(slider));
        return true;
    });

    ui.set_function("create_toggle", [](const std::string& canvas_name, const std::string& parent_name,
                                        const std::string& element_name, const std::string& label) -> bool {
        auto* ctx = get_ui_ctx();
        if (!ctx) return false;
        auto* canvas = ctx->get_canvas(canvas_name);
        if (!canvas || !canvas->get_root()) return false;

        UIElement* parent = canvas->get_root()->find_child(parent_name);
        if (!parent) parent = canvas->get_root();

        auto toggle = std::make_unique<UIToggle>(label);
        toggle->set_name(element_name);
        toggle->set_interactive(true);
        parent->add_child(std::move(toggle));
        return true;
    });

    ui.set_function("create_text_input", [](const std::string& canvas_name, const std::string& parent_name,
                                            const std::string& element_name,
                                            sol::optional<std::string> placeholder) -> bool {
        auto* ctx = get_ui_ctx();
        if (!ctx) return false;
        auto* canvas = ctx->get_canvas(canvas_name);
        if (!canvas || !canvas->get_root()) return false;

        UIElement* parent = canvas->get_root()->find_child(parent_name);
        if (!parent) parent = canvas->get_root();

        auto input = std::make_unique<UITextInput>(placeholder.value_or(""));
        input->set_name(element_name);
        input->set_interactive(true);
        input->set_focusable(true);
        parent->add_child(std::move(input));
        return true;
    });

    // --- Element Properties ---

    ui.set_function("set_visible", [](const std::string& canvas, const std::string& element, bool visible) {
        if (auto* el = find_element(canvas, element)) {
            el->set_visible(visible);
        }
    });

    ui.set_function("is_visible", [](const std::string& canvas, const std::string& element) -> bool {
        if (auto* el = find_element(canvas, element)) {
            return el->is_visible();
        }
        return false;
    });

    ui.set_function("set_enabled", [](const std::string& canvas, const std::string& element, bool enabled) {
        if (auto* el = find_element(canvas, element)) {
            el->set_enabled(enabled);
        }
    });

    ui.set_function("is_enabled", [](const std::string& canvas, const std::string& element) -> bool {
        if (auto* el = find_element(canvas, element)) {
            return el->is_enabled();
        }
        return false;
    });

    ui.set_function("set_position", [](const std::string& canvas, const std::string& element, float x, float y) {
        if (auto* el = find_element(canvas, element)) {
            el->set_position(Vec2{x, y});
        }
    });

    ui.set_function("get_position", [](const std::string& canvas, const std::string& element) -> std::tuple<float, float> {
        if (auto* el = find_element(canvas, element)) {
            Vec2 pos = el->get_position();
            return {pos.x, pos.y};
        }
        return {0.0f, 0.0f};
    });

    ui.set_function("set_size", [](const std::string& canvas, const std::string& element, float w, float h) {
        if (auto* el = find_element(canvas, element)) {
            el->set_size(Vec2{w, h});
        }
    });

    ui.set_function("get_size", [](const std::string& canvas, const std::string& element) -> std::tuple<float, float> {
        if (auto* el = find_element(canvas, element)) {
            Vec2 size = el->get_size();
            return {size.x, size.y};
        }
        return {0.0f, 0.0f};
    });

    // --- Text Control ---

    ui.set_function("set_text", [](const std::string& canvas, const std::string& element, const std::string& text) {
        if (auto* el = find_element(canvas, element)) {
            if (auto* label = dynamic_cast<UILabel*>(el)) {
                label->set_text(text);
            } else if (auto* button = dynamic_cast<UIButton*>(el)) {
                button->set_text(text);
            } else if (auto* input = dynamic_cast<UITextInput*>(el)) {
                input->set_text(text);
            }
        }
    });

    ui.set_function("get_text", [](const std::string& canvas, const std::string& element) -> std::string {
        if (auto* el = find_element(canvas, element)) {
            if (auto* label = dynamic_cast<UILabel*>(el)) {
                return label->get_text();
            } else if (auto* button = dynamic_cast<UIButton*>(el)) {
                return button->get_text();
            } else if (auto* input = dynamic_cast<UITextInput*>(el)) {
                return input->get_text();
            }
        }
        return "";
    });

    ui.set_function("set_text_key", [](const std::string& canvas, const std::string& element, const std::string& key) {
        if (auto* el = find_element(canvas, element)) {
            if (auto* label = dynamic_cast<UILabel*>(el)) {
                label->set_text_key(key);
            } else if (auto* button = dynamic_cast<UIButton*>(el)) {
                button->set_text_key(key);
            }
        }
    });

    // --- Value Controls ---

    ui.set_function("set_progress", [](const std::string& canvas, const std::string& element, float value) {
        if (auto* el = find_element(canvas, element)) {
            if (auto* bar = dynamic_cast<UIProgressBar*>(el)) {
                bar->set_value(value);
            }
        }
    });

    ui.set_function("get_progress", [](const std::string& canvas, const std::string& element) -> float {
        if (auto* el = find_element(canvas, element)) {
            if (auto* bar = dynamic_cast<UIProgressBar*>(el)) {
                return bar->get_value();
            }
        }
        return 0.0f;
    });

    ui.set_function("set_slider_value", [](const std::string& canvas, const std::string& element, float value) {
        if (auto* el = find_element(canvas, element)) {
            if (auto* slider = dynamic_cast<UISlider*>(el)) {
                slider->set_value(value);
            }
        }
    });

    ui.set_function("get_slider_value", [](const std::string& canvas, const std::string& element) -> float {
        if (auto* el = find_element(canvas, element)) {
            if (auto* slider = dynamic_cast<UISlider*>(el)) {
                return slider->get_value();
            }
        }
        return 0.0f;
    });

    ui.set_function("set_toggle_checked", [](const std::string& canvas, const std::string& element, bool checked) {
        if (auto* el = find_element(canvas, element)) {
            if (auto* toggle = dynamic_cast<UIToggle*>(el)) {
                toggle->set_checked(checked);
            }
        }
    });

    ui.set_function("is_toggle_checked", [](const std::string& canvas, const std::string& element) -> bool {
        if (auto* el = find_element(canvas, element)) {
            if (auto* toggle = dynamic_cast<UIToggle*>(el)) {
                return toggle->is_checked();
            }
        }
        return false;
    });

    // --- Callbacks ---

    ui.set_function("on_click", [](const std::string& canvas, const std::string& element, sol::function callback) {
        if (auto* el = find_element(canvas, element)) {
            std::string key = make_callback_key(canvas, element);
            s_click_callbacks[key] = callback;
            el->on_click = [key]() {
                auto it = s_click_callbacks.find(key);
                if (it != s_click_callbacks.end() && it->second.valid()) {
                    try {
                        it->second();
                    } catch (const sol::error& e) {
                        core::log(core::LogLevel::Error, "Lua UI click callback error: {}", e.what());
                    }
                }
            };
        }
    });

    ui.set_function("on_hover", [](const std::string& canvas, const std::string& element, sol::function callback) {
        if (auto* el = find_element(canvas, element)) {
            std::string key = make_callback_key(canvas, element);
            s_hover_callbacks[key] = callback;
            el->on_hover = [key](bool hovering) {
                auto it = s_hover_callbacks.find(key);
                if (it != s_hover_callbacks.end() && it->second.valid()) {
                    try {
                        it->second(hovering);
                    } catch (const sol::error& e) {
                        core::log(core::LogLevel::Error, "Lua UI hover callback error: {}", e.what());
                    }
                }
            };
        }
    });

    ui.set_function("on_slider_changed", [](const std::string& canvas, const std::string& element, sol::function callback) {
        if (auto* el = find_element(canvas, element)) {
            if (auto* slider = dynamic_cast<UISlider*>(el)) {
                std::string key = make_callback_key(canvas, element);
                s_value_changed_callbacks[key] = callback;
                slider->on_value_changed = [key](float value) {
                    auto it = s_value_changed_callbacks.find(key);
                    if (it != s_value_changed_callbacks.end() && it->second.valid()) {
                        try {
                            it->second(value);
                        } catch (const sol::error& e) {
                            core::log(core::LogLevel::Error, "Lua UI slider callback error: {}", e.what());
                        }
                    }
                };
            }
        }
    });

    ui.set_function("on_text_changed", [](const std::string& canvas, const std::string& element, sol::function callback) {
        if (auto* el = find_element(canvas, element)) {
            if (auto* input = dynamic_cast<UITextInput*>(el)) {
                std::string key = make_callback_key(canvas, element);
                s_text_changed_callbacks[key] = callback;
                input->on_text_changed = [key](const std::string& text) {
                    auto it = s_text_changed_callbacks.find(key);
                    if (it != s_text_changed_callbacks.end() && it->second.valid()) {
                        try {
                            it->second(text);
                        } catch (const sol::error& e) {
                            core::log(core::LogLevel::Error, "Lua UI text changed callback error: {}", e.what());
                        }
                    }
                };
            }
        }
    });

    // --- Element Removal ---

    ui.set_function("remove_element", [](const std::string& canvas_name, const std::string& element_name) {
        auto* ctx = get_ui_ctx();
        if (!ctx) return;
        auto* canvas = ctx->get_canvas(canvas_name);
        if (!canvas || !canvas->get_root()) return;

        UIElement* element = canvas->get_root()->find_child(element_name);
        if (element && element->get_parent()) {
            // Clean up callbacks
            std::string key = make_callback_key(canvas_name, element_name);
            s_click_callbacks.erase(key);
            s_hover_callbacks.erase(key);
            s_value_changed_callbacks.erase(key);
            s_text_changed_callbacks.erase(key);

            element->get_parent()->remove_child(element);
        }
    });

    // --- Focus Control ---

    ui.set_function("focus", [](const std::string& canvas_name, const std::string& element_name) {
        auto* ctx = get_ui_ctx();
        if (!ctx) return;
        auto* canvas = ctx->get_canvas(canvas_name);
        if (!canvas) return;

        if (auto* el = find_element(canvas_name, element_name)) {
            canvas->set_focused_element(el);
        }
    });

    ui.set_function("clear_focus", [](const std::string& canvas_name) {
        auto* ctx = get_ui_ctx();
        if (!ctx) return;
        auto* canvas = ctx->get_canvas(canvas_name);
        if (canvas) {
            canvas->set_focused_element(nullptr);
        }
    });

    // --- Screen Info ---

    ui.set_function("get_screen_size", []() -> std::tuple<uint32_t, uint32_t> {
        auto* ctx = get_ui_ctx();
        if (!ctx) return {0, 0};
        return {ctx->get_screen_width(), ctx->get_screen_height()};
    });

    ui.set_function("get_dpi_scale", []() -> float {
        auto* ctx = get_ui_ctx();
        return ctx ? ctx->get_dpi_scale() : 1.0f;
    });

    // --- Tooltip ---

    ui.set_function("set_tooltip", [](const std::string& canvas, const std::string& element, const std::string& text) {
        if (auto* el = find_element(canvas, element)) {
            el->set_tooltip(text);
        }
    });

    // --- Element Query ---

    ui.set_function("has_element", [](const std::string& canvas, const std::string& element) -> bool {
        return find_element(canvas, element) != nullptr;
    });

    ui.set_function("is_hovered", [](const std::string& canvas, const std::string& element) -> bool {
        if (auto* el = find_element(canvas, element)) {
            return el->is_hovered();
        }
        return false;
    });

    ui.set_function("is_pressed", [](const std::string& canvas, const std::string& element) -> bool {
        if (auto* el = find_element(canvas, element)) {
            return el->is_pressed();
        }
        return false;
    });

    ui.set_function("is_focused", [](const std::string& canvas, const std::string& element) -> bool {
        if (auto* el = find_element(canvas, element)) {
            return el->is_focused();
        }
        return false;
    });
}

} // namespace engine::script
