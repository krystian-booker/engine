#include <engine/ui/ui_canvas.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace engine::ui {

UICanvas::UICanvas() = default;
UICanvas::~UICanvas() = default;

void UICanvas::set_root(std::unique_ptr<UIElement> root) {
    m_root = std::move(root);
    m_layout_dirty = true;
}

void UICanvas::set_size(uint32_t width, uint32_t height) {
    if (m_width != width || m_height != height) {
        m_width = width;
        m_height = height;
        m_layout_dirty = true;
    }
}

void UICanvas::set_reference_resolution(uint32_t width, uint32_t height) {
    m_reference_width = width;
    m_reference_height = height;
    m_layout_dirty = true;
}

void UICanvas::update(float dt, const UIInputState& input) {
    if (!m_enabled || !m_root) return;

    // Layout if needed
    if (m_layout_dirty) {
        layout_root();
    }

    // Track hovered element
    UIElement* new_hovered = m_root->find_element_at(input.mouse_position);
    if (new_hovered != m_hovered_element) {
        m_hovered_element = new_hovered;
    }

    // Update element tree
    m_root->update(dt, input);
}

void UICanvas::render(UIRenderContext& ctx) {
    if (!m_enabled || !m_root) return;

    m_root->render(ctx);
}

void UICanvas::set_focused_element(UIElement* element) {
    if (m_focused_element == element) return;

    if (m_focused_element) {
        m_focused_element->release_focus();
    }

    m_focused_element = element;

    if (m_focused_element) {
        m_focused_element->request_focus();
    }
}

UIElement* UICanvas::find_element_at(Vec2 point) {
    if (!m_root) return nullptr;
    return m_root->find_element_at(point);
}

void UICanvas::layout_root() {
    if (!m_root) return;

    // Calculate scale based on scale mode
    float scale_x = 1.0f;
    float scale_y = 1.0f;

    if (m_scale_mode == ScaleMode::ScaleWithScreen) {
        if (m_reference_width > 0 && m_reference_height > 0) {
            scale_x = static_cast<float>(m_width) / m_reference_width;
            scale_y = static_cast<float>(m_height) / m_reference_height;
        }
    }

    // Create root bounds
    Rect root_bounds(0, 0,
                     static_cast<float>(m_width) / scale_x,
                     static_cast<float>(m_height) / scale_y);

    m_root->layout(root_bounds);
    m_layout_dirty = false;
}

void UICanvas::collect_focusable_elements(UIElement* element, std::vector<UIElement*>& out) {
    if (!element || !element->is_visible() || !element->is_enabled()) return;

    if (element->is_focusable()) {
        out.push_back(element);
    }

    for (const auto& child : element->get_children()) {
        collect_focusable_elements(child.get(), out);
    }
}

void UICanvas::navigate_focus(NavDirection direction) {
    if (direction == NavDirection::None) return;

    std::vector<UIElement*> focusable;
    collect_focusable_elements(m_root.get(), focusable);

    if (focusable.empty()) return;

    // If no current focus, focus the first element
    if (!m_focused_element) {
        set_focused_element(focusable.front());
        return;
    }

    // Find nearest element in the given direction
    UIElement* next = find_nearest_in_direction(m_focused_element, direction);
    if (next) {
        set_focused_element(next);
    }
}

UIElement* UICanvas::find_nearest_in_direction(UIElement* from, NavDirection dir) {
    if (!from) return nullptr;

    std::vector<UIElement*> focusable;
    collect_focusable_elements(m_root.get(), focusable);

    if (focusable.size() <= 1) return nullptr;

    const Rect& from_bounds = from->get_bounds();
    Vec2 from_center = from_bounds.center();

    UIElement* best = nullptr;
    float best_score = std::numeric_limits<float>::max();

    for (UIElement* candidate : focusable) {
        if (candidate == from) continue;

        const Rect& to_bounds = candidate->get_bounds();
        Vec2 to_center = to_bounds.center();

        // Calculate direction vector
        Vec2 delta = to_center - from_center;

        // Check if candidate is in the correct direction
        bool valid = false;
        float alignment_score = 0.0f;

        switch (dir) {
            case NavDirection::Up:
                valid = to_center.y < from_center.y;
                alignment_score = std::abs(delta.x);
                break;
            case NavDirection::Down:
                valid = to_center.y > from_center.y;
                alignment_score = std::abs(delta.x);
                break;
            case NavDirection::Left:
                valid = to_center.x < from_center.x;
                alignment_score = std::abs(delta.y);
                break;
            case NavDirection::Right:
                valid = to_center.x > from_center.x;
                alignment_score = std::abs(delta.y);
                break;
            default:
                break;
        }

        if (!valid) continue;

        // Score based on distance and alignment
        float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        float score = distance + alignment_score * 2.0f;  // Penalize misalignment

        if (score < best_score) {
            best_score = score;
            best = candidate;
        }
    }

    return best;
}

void UICanvas::focus_next() {
    std::vector<UIElement*> focusable;
    collect_focusable_elements(m_root.get(), focusable);

    if (focusable.empty()) return;

    // Sort by tab index
    std::sort(focusable.begin(), focusable.end(),
        [](UIElement* a, UIElement* b) {
            return a->get_tab_index() < b->get_tab_index();
        });

    if (!m_focused_element) {
        set_focused_element(focusable.front());
        return;
    }

    // Find current and move to next
    auto it = std::find(focusable.begin(), focusable.end(), m_focused_element);
    if (it != focusable.end()) {
        ++it;
        if (it == focusable.end()) {
            it = focusable.begin();  // Wrap around
        }
        set_focused_element(*it);
    }
}

void UICanvas::focus_previous() {
    std::vector<UIElement*> focusable;
    collect_focusable_elements(m_root.get(), focusable);

    if (focusable.empty()) return;

    // Sort by tab index
    std::sort(focusable.begin(), focusable.end(),
        [](UIElement* a, UIElement* b) {
            return a->get_tab_index() < b->get_tab_index();
        });

    if (!m_focused_element) {
        set_focused_element(focusable.back());
        return;
    }

    // Find current and move to previous
    auto it = std::find(focusable.begin(), focusable.end(), m_focused_element);
    if (it != focusable.end()) {
        if (it == focusable.begin()) {
            it = focusable.end();  // Wrap around
        }
        --it;
        set_focused_element(*it);
    }
}

void UICanvas::activate_focused() {
    if (m_focused_element && m_focused_element->on_click) {
        m_focused_element->on_click();
    }
}

} // namespace engine::ui
