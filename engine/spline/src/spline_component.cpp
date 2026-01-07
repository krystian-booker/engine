#include <engine/spline/spline_component.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>

namespace engine::spline {

Spline* SplineComponent::get_spline() const {
    if (!runtime_spline) {
        // Create spline from serialized data
        runtime_spline = create_spline(mode);
        runtime_spline->end_mode = end_mode;
        runtime_spline->tension = tension;
        runtime_spline->auto_tangents = auto_tangents;

        if (mode == SplineMode::CatmullRom) {
            auto* catmull = dynamic_cast<CatmullRomSpline*>(runtime_spline.get());
            if (catmull) {
                catmull->alpha = catmull_rom_alpha;
            }
        }

        runtime_spline->set_points(points);

        if (auto_tangents && mode == SplineMode::Bezier) {
            auto* bezier = dynamic_cast<BezierSpline*>(runtime_spline.get());
            if (bezier) {
                bezier->auto_generate_tangents();
            }
        }
    }
    return runtime_spline.get();
}

Vec3 SplineComponent::evaluate_position(float t) const {
    const Spline* spline = get_spline();
    return spline ? spline->evaluate_position(t) : Vec3(0.0f);
}

SplineEvalResult SplineComponent::evaluate(float t) const {
    const Spline* spline = get_spline();
    return spline ? spline->evaluate(t) : SplineEvalResult{};
}

float SplineComponent::get_length() const {
    const Spline* spline = get_spline();
    return spline ? spline->get_length() : 0.0f;
}

void SplineEventComponent::reset_triggers() {
    for (auto& event : distance_events) {
        event.triggered = false;
    }
    for (auto& event : point_events) {
        event.triggered = false;
    }
}

void spline_debug_draw_system(engine::scene::World& world, double /*dt*/) {
    // Get debug draw interface if available
    // This would integrate with engine::debug or engine::render debug draw

    auto view = world.view<SplineComponent, SplineDebugRenderComponent>();
    for (auto entity : view) {
        auto& spline_comp = view.get<SplineComponent>(entity);
        auto& debug_comp = view.get<SplineDebugRenderComponent>(entity);

        if (!debug_comp.enabled || !spline_comp.visible) continue;

        const Spline* spline = spline_comp.get_spline();
        if (!spline || spline->point_count() < 2) continue;

        // Get entity transform for local-to-world
        Mat4 transform(1.0f);
        auto* world_transform = world.try_get<scene::WorldTransform>(entity);
        if (world_transform) {
            transform = world_transform->matrix;
        }

        // Tessellate and draw curve
        if (debug_comp.render_curve) {
            std::vector<Vec3> points = spline->tessellate(spline_comp.tessellation);
            // Transform points and submit for debug draw
            // debug::draw_line_strip(points, debug_comp.curve_color);
        }

        // Draw control points
        if (debug_comp.render_points) {
            for (size_t i = 0; i < spline->point_count(); ++i) {
                const SplinePoint& pt = spline->get_point(i);
                Vec3 world_pos = Vec3(transform * Vec4(pt.position, 1.0f));
                // debug::draw_point(world_pos, debug_comp.point_size, debug_comp.point_color);
            }
        }

        // Draw tangents (for bezier)
        if (debug_comp.render_tangents && spline_comp.mode == SplineMode::Bezier) {
            for (size_t i = 0; i < spline->point_count(); ++i) {
                const SplinePoint& pt = spline->get_point(i);
                Vec3 world_pos = Vec3(transform * Vec4(pt.position, 1.0f));
                Vec3 tan_in = Vec3(transform * Vec4(pt.position + pt.tangent_in * debug_comp.tangent_scale, 1.0f));
                Vec3 tan_out = Vec3(transform * Vec4(pt.position + pt.tangent_out * debug_comp.tangent_scale, 1.0f));
                // debug::draw_line(world_pos, tan_in, debug_comp.tangent_color);
                // debug::draw_line(world_pos, tan_out, debug_comp.tangent_color);
            }
        }

        // Draw normals
        if (debug_comp.render_normals) {
            for (int i = 0; i <= 20; ++i) {
                float t = static_cast<float>(i) / 20.0f;
                SplineEvalResult eval = spline->evaluate(t);
                Vec3 world_pos = Vec3(transform * Vec4(eval.position, 1.0f));
                Vec3 normal_end = Vec3(transform * Vec4(eval.position + eval.normal * 0.5f, 1.0f));
                // debug::draw_line(world_pos, normal_end, debug_comp.normal_color);
            }
        }

        // Draw bounds
        if (debug_comp.render_bounds) {
            AABB bounds = spline->get_bounds();
            // debug::draw_aabb(bounds, transform, Vec4(1.0f, 1.0f, 0.0f, 0.5f));
        }
    }
}

void spline_mesh_system(engine::scene::World& world, double /*dt*/) {
    // This system would regenerate meshes when splines change
    // For now, it's a placeholder for mesh generation logic

    auto view = world.view<SplineComponent, SplineMeshComponent>();
    for (auto entity : view) {
        auto& spline_comp = view.get<SplineComponent>(entity);
        auto& mesh_comp = view.get<SplineMeshComponent>(entity);

        const Spline* spline = spline_comp.get_spline();
        if (!spline || spline->point_count() < 2) continue;

        // Mesh generation would happen here
        // The generated mesh would be attached as a MeshRenderer component
        (void)mesh_comp; // Suppress unused warning
    }
}

} // namespace engine::spline
