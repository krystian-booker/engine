#pragma once

#include <engine/ui/ui_canvas.hpp>
#include <engine/ui/ui_world_canvas.hpp>
#include <engine/core/math.hpp>
#include <memory>

namespace engine::scene {
    class World;
}

namespace engine::ui {

using namespace engine::core;

// ECS component for screen-space UI canvas
// Entities with this component can have their own UI canvas
struct UICanvasComponent {
    std::shared_ptr<UICanvas> canvas;

    // Canvas properties (synced to canvas on update)
    int32_t sort_order = 0;       // Render order (higher = rendered later/on top)
    bool enabled = true;          // Whether canvas is updated and rendered

    // Canvas was created internally
    bool initialized = false;

    UICanvasComponent() = default;
    explicit UICanvasComponent(std::shared_ptr<UICanvas> c)
        : canvas(std::move(c)) {}

    // Convenience setters for fluent API
    UICanvasComponent& set_sort_order(int32_t order) { sort_order = order; return *this; }
    UICanvasComponent& set_enabled(bool e) { enabled = e; return *this; }
};

// ECS component for world-space UI canvas (health bars, interaction prompts, etc.)
// The canvas position is synchronized with the entity's transform
struct UIWorldCanvasComponent {
    std::shared_ptr<UIWorldCanvas> canvas;

    // Local offset from entity position
    Vec3 offset{0.0f, 0.0f, 0.0f};

    // Whether to sync position from entity's LocalTransform component
    bool use_entity_transform = true;

    // Billboard settings
    WorldCanvasBillboard billboard = WorldCanvasBillboard::FaceCamera;

    // Distance settings
    float max_distance = 100.0f;    // Maximum render distance
    float fade_range = 10.0f;       // Start fading at max_distance - fade_range

    // Screen size settings
    bool constant_screen_size = false;  // Keep same screen size regardless of distance
    float reference_distance = 10.0f;   // Reference distance for constant screen size
    float min_scale = 0.5f;             // Minimum scale multiplier
    float max_scale = 2.0f;             // Maximum scale multiplier

    bool enabled = true;
    bool initialized = false;

    UIWorldCanvasComponent() = default;
    explicit UIWorldCanvasComponent(std::shared_ptr<UIWorldCanvas> c)
        : canvas(std::move(c)) {}

    // Convenience setters
    UIWorldCanvasComponent& set_offset(const Vec3& o) { offset = o; return *this; }
    UIWorldCanvasComponent& set_billboard(WorldCanvasBillboard b) { billboard = b; return *this; }
    UIWorldCanvasComponent& set_max_distance(float d) { max_distance = d; return *this; }
    UIWorldCanvasComponent& set_fade_range(float r) { fade_range = r; return *this; }
    UIWorldCanvasComponent& set_constant_screen_size(bool c) { constant_screen_size = c; return *this; }
    UIWorldCanvasComponent& set_enabled(bool e) { enabled = e; return *this; }
};

// Forward declarations
class UIContext;

// System to sync world canvas positions from entity transforms
// Call in PreRender phase after transforms are finalized
void ui_world_canvas_sync_system(scene::World& world, const render::CameraData& camera,
                                  uint32_t screen_width, uint32_t screen_height);

// System to initialize and cleanup UI canvas components
// Call during Update phase
void ui_canvas_lifecycle_system(scene::World& world, UIContext& ctx);

// Factory functions for common configurations
inline UIWorldCanvasComponent make_health_bar_canvas(float width = 100.0f, float height = 12.0f) {
    UIWorldCanvasComponent comp;
    auto canvas = std::make_shared<UIWorldCanvas>();
    canvas->set_size(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    comp.canvas = canvas;
    comp.offset = Vec3(0.0f, 2.0f, 0.0f);  // Above entity
    comp.billboard = WorldCanvasBillboard::FaceCamera;
    comp.constant_screen_size = true;
    comp.reference_distance = 10.0f;
    comp.max_distance = 50.0f;
    comp.fade_range = 5.0f;
    return comp;
}

inline UIWorldCanvasComponent make_nameplate_canvas(float width = 150.0f, float height = 24.0f) {
    UIWorldCanvasComponent comp;
    auto canvas = std::make_shared<UIWorldCanvas>();
    canvas->set_size(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    comp.canvas = canvas;
    comp.offset = Vec3(0.0f, 2.2f, 0.0f);  // Above entity, above health bar
    comp.billboard = WorldCanvasBillboard::FaceCamera;
    comp.constant_screen_size = true;
    comp.reference_distance = 15.0f;
    comp.max_distance = 30.0f;
    comp.fade_range = 5.0f;
    return comp;
}

inline UIWorldCanvasComponent make_interaction_prompt_canvas(float width = 200.0f, float height = 50.0f) {
    UIWorldCanvasComponent comp;
    auto canvas = std::make_shared<UIWorldCanvas>();
    canvas->set_size(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    comp.canvas = canvas;
    comp.offset = Vec3(0.0f, 1.0f, 0.0f);
    comp.billboard = WorldCanvasBillboard::FaceCamera;
    comp.constant_screen_size = false;
    comp.max_distance = 5.0f;  // Only visible when close
    comp.fade_range = 1.0f;
    return comp;
}

} // namespace engine::ui
