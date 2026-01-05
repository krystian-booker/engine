#include <engine/ui/ui_components.hpp>
#include <engine/ui/ui_context.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/render/render_pipeline.hpp>

namespace engine::ui {

void ui_world_canvas_sync_system(scene::World& world, const render::CameraData& camera,
                                  uint32_t screen_width, uint32_t screen_height) {
    // Iterate all entities with UIWorldCanvasComponent
    auto view = world.view<UIWorldCanvasComponent>();

    for (auto entity : view) {
        auto& comp = view.get<UIWorldCanvasComponent>(entity);

        if (!comp.canvas || !comp.enabled) {
            continue;
        }

        // Get world position from entity transform
        Vec3 world_pos{0.0f};

        if (comp.use_entity_transform) {
            // Try WorldTransform first (computed from hierarchy)
            if (auto* world_transform = world.try_get<scene::WorldTransform>(entity)) {
                world_pos = world_transform->position();
            }
            // Fall back to LocalTransform
            else if (auto* local_transform = world.try_get<scene::LocalTransform>(entity)) {
                world_pos = local_transform->position;
            }
        }

        // Apply local offset
        world_pos += comp.offset;

        // Sync component settings to canvas
        comp.canvas->set_world_position(world_pos);
        comp.canvas->set_billboard(comp.billboard);
        comp.canvas->set_max_distance(comp.max_distance);
        comp.canvas->set_fade_range(comp.fade_range);
        comp.canvas->set_constant_screen_size(comp.constant_screen_size);
        comp.canvas->set_reference_distance(comp.reference_distance);
        comp.canvas->set_min_scale(comp.min_scale);
        comp.canvas->set_max_scale(comp.max_scale);
        comp.canvas->set_enabled(comp.enabled);

        // Update canvas with camera (computes screen position, visibility, alpha)
        comp.canvas->update_for_camera(camera, screen_width, screen_height);

        comp.initialized = true;
    }
}

void ui_canvas_lifecycle_system(scene::World& world, UIContext& /*ctx*/) {
    // Process screen-space canvas components
    {
        auto view = world.view<UICanvasComponent>();

        for (auto entity : view) {
            auto& comp = view.get<UICanvasComponent>(entity);

            // Create canvas if needed
            if (!comp.canvas) {
                comp.canvas = std::make_shared<UICanvas>();
                comp.initialized = false;
            }

            // Sync settings from component to canvas
            comp.canvas->set_sort_order(comp.sort_order);
            comp.canvas->set_enabled(comp.enabled);

            comp.initialized = true;
        }
    }

    // Process world-space canvas components
    {
        auto view = world.view<UIWorldCanvasComponent>();

        for (auto entity : view) {
            auto& comp = view.get<UIWorldCanvasComponent>(entity);

            // Create canvas if needed
            if (!comp.canvas) {
                comp.canvas = std::make_shared<UIWorldCanvas>();
                comp.initialized = false;
            }

            // Settings are synced in ui_world_canvas_sync_system
            comp.initialized = true;
        }
    }
}

// Helper function to collect and render all world canvases from ECS
void ui_render_ecs_world_canvases(scene::World& world, UIRenderContext& render_ctx) {
    // Collect visible world canvases sorted by distance (back to front)
    struct CanvasEntry {
        UIWorldCanvas* canvas;
        float distance;
    };
    std::vector<CanvasEntry> visible_canvases;

    auto view = world.view<UIWorldCanvasComponent>();
    for (auto entity : view) {
        auto& comp = view.get<UIWorldCanvasComponent>(entity);

        if (!comp.canvas || !comp.enabled || !comp.canvas->is_visible()) {
            continue;
        }

        visible_canvases.push_back({comp.canvas.get(), comp.canvas->get_current_distance()});
    }

    // Sort back to front (farthest first)
    std::sort(visible_canvases.begin(), visible_canvases.end(),
              [](const CanvasEntry& a, const CanvasEntry& b) {
                  return a.distance > b.distance;
              });

    // Render each canvas
    for (const auto& entry : visible_canvases) {
        auto* canvas = entry.canvas;

        // Apply computed scale to canvas rendering
        float scale = canvas->get_computed_scale();
        float alpha = canvas->get_distance_alpha();

        // Get screen position (canvas is centered at this position)
        Vec2 screen_pos = canvas->get_screen_position();
        float canvas_w = canvas->get_width() * scale;
        float canvas_h = canvas->get_height() * scale;

        // Calculate canvas bounds centered at screen position
        float x = screen_pos.x - canvas_w * 0.5f;
        float y = screen_pos.y - canvas_h * 0.5f;

        // Apply transform for world canvas rendering
        render_ctx.push_transform(x, y, scale, alpha);
        canvas->render(render_ctx);
        render_ctx.pop_transform();
    }
}

// Helper to update all ECS canvas components
void ui_update_ecs_canvases(scene::World& world, UIContext& ctx, float dt, const UIInputState& input) {
    // Update screen-space canvases
    {
        auto view = world.view<UICanvasComponent>();
        for (auto entity : view) {
            auto& comp = view.get<UICanvasComponent>(entity);
            if (comp.canvas && comp.enabled) {
                comp.canvas->update(dt, input);
            }
        }
    }

    // Update world-space canvases
    {
        auto view = world.view<UIWorldCanvasComponent>();
        for (auto entity : view) {
            auto& comp = view.get<UIWorldCanvasComponent>(entity);
            if (comp.canvas && comp.enabled && comp.canvas->is_visible()) {
                comp.canvas->update(dt, input);
            }
        }
    }
}

} // namespace engine::ui
