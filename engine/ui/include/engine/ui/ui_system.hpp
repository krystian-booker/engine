#pragma once

#include <engine/ui/ui_types.hpp>

namespace engine::scene {
    class World;
}

namespace engine::render {
    struct CameraData;
}

namespace engine::ui {

class UIContext;
class UIRenderContext;

// UI system functions for engine integration
// Note: UI update requires input state, so it's called directly by Application
// rather than through the ECS scheduler

// Initialize UI input state from window events
// Call this at the start of each frame before processing events
void ui_input_begin_frame(UIInputState& state);

// Call after all window events have been processed to finalize input state
void ui_input_end_frame(UIInputState& state);

// ECS system functions for UI components

// Sync world canvas positions from entity transforms
// Call in PreRender phase after transforms are finalized
void ui_world_canvas_sync_system(scene::World& world, const render::CameraData& camera,
                                  uint32_t screen_width, uint32_t screen_height);

// Initialize and cleanup UI canvas components
// Call during Update phase
void ui_canvas_lifecycle_system(scene::World& world, UIContext& ctx);

// Update all ECS-managed canvases
void ui_update_ecs_canvases(scene::World& world, UIContext& ctx, float dt, const UIInputState& input);

// Render all world canvases from ECS components
void ui_render_ecs_world_canvases(scene::World& world, UIRenderContext& render_ctx);

} // namespace engine::ui
