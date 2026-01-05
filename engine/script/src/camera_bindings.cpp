#include <engine/script/bindings.hpp>
#include <engine/script/script_context.hpp>
#include <engine/scene/render_components.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/core/log.hpp>

namespace engine::script {

void register_camera_bindings(sol::state& lua) {
    using namespace engine::scene;
    using namespace engine::core;

    // Camera component usertype
    lua.new_usertype<Camera>("CameraComponent",
        sol::constructors<>(),
        "fov", &Camera::fov,
        "near_plane", &Camera::near_plane,
        "far_plane", &Camera::far_plane,
        "aspect_ratio", &Camera::aspect_ratio,
        "priority", &Camera::priority,
        "active", &Camera::active,
        "orthographic", &Camera::orthographic,
        "ortho_size", &Camera::ortho_size,
        "projection", &Camera::projection
    );

    // Create Camera namespace table
    auto cam = lua.create_named_table("Camera");

    // Get active camera (highest priority active camera)
    cam.set_function("get_active", []() -> uint32_t {
        auto* world = get_current_script_world();
        if (!world) {
            return static_cast<uint32_t>(NullEntity);
        }

        Entity best_camera = NullEntity;
        uint8_t highest_priority = 0;

        auto view = world->view<Camera>();
        for (auto entity : view) {
            const auto& camera = view.get<Camera>(entity);
            if (camera.active && (best_camera == NullEntity || camera.priority > highest_priority)) {
                best_camera = entity;
                highest_priority = camera.priority;
            }
        }

        return static_cast<uint32_t>(best_camera);
    });

    // Get camera component from entity
    cam.set_function("get", [&lua](uint32_t entity_id) -> sol::object {
        auto* world = get_current_script_world();
        if (!world) {
            return sol::nil;
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) {
            return sol::nil;
        }

        auto* camera = world->try_get<Camera>(entity);
        if (!camera) {
            return sol::nil;
        }

        return sol::make_object(lua, std::ref(*camera));
    });

    // Check if entity has camera
    cam.set_function("has", [](uint32_t entity_id) -> bool {
        auto* world = get_current_script_world();
        if (!world) {
            return false;
        }

        auto entity = static_cast<entt::entity>(entity_id);
        return world->registry().valid(entity) && world->has<Camera>(entity);
    });

    // Set camera active state
    cam.set_function("set_active", [](uint32_t entity_id, bool active) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* camera = world->try_get<Camera>(entity);
        if (camera) {
            camera->active = active;
        }
    });

    // Set camera priority
    cam.set_function("set_priority", [](uint32_t entity_id, uint8_t priority) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* camera = world->try_get<Camera>(entity);
        if (camera) {
            camera->priority = priority;
        }
    });

    // Set FOV
    cam.set_function("set_fov", [](uint32_t entity_id, float fov) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* camera = world->try_get<Camera>(entity);
        if (camera) {
            camera->fov = fov;
        }
    });

    // Get FOV
    cam.set_function("get_fov", [](uint32_t entity_id) -> float {
        auto* world = get_current_script_world();
        if (!world) return 60.0f;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return 60.0f;

        auto* camera = world->try_get<Camera>(entity);
        return camera ? camera->fov : 60.0f;
    });

    // Set near/far planes
    cam.set_function("set_clip_planes", [](uint32_t entity_id, float near_plane, float far_plane) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* camera = world->try_get<Camera>(entity);
        if (camera) {
            camera->near_plane = near_plane;
            camera->far_plane = far_plane;
        }
    });

    // Set orthographic mode
    cam.set_function("set_orthographic", [](uint32_t entity_id, bool ortho, sol::optional<float> size) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* camera = world->try_get<Camera>(entity);
        if (camera) {
            camera->orthographic = ortho;
            if (size.has_value()) {
                camera->ortho_size = size.value();
            }
        }
    });

    // World to screen position
    cam.set_function("world_to_screen", [](uint32_t entity_id, const Vec3& world_pos,
                                           uint32_t screen_width, uint32_t screen_height) -> sol::optional<Vec2> {
        auto* world = get_current_script_world();
        if (!world) return sol::nullopt;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return sol::nullopt;

        auto* camera = world->try_get<Camera>(entity);
        auto* world_transform = world->try_get<WorldTransform>(entity);
        if (!camera || !world_transform) return sol::nullopt;

        Mat4 view = glm::inverse(world_transform->matrix);
        Mat4 proj = camera->projection();
        Mat4 vp = proj * view;

        Vec4 clip = vp * Vec4(world_pos, 1.0f);
        if (clip.w <= 0.0f) return sol::nullopt; // Behind camera

        Vec3 ndc = Vec3(clip) / clip.w;
        if (ndc.x < -1.0f || ndc.x > 1.0f || ndc.y < -1.0f || ndc.y > 1.0f) {
            return sol::nullopt; // Outside screen
        }

        Vec2 screen;
        screen.x = (ndc.x * 0.5f + 0.5f) * static_cast<float>(screen_width);
        screen.y = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(screen_height); // Y flipped
        return screen;
    });

    // Screen to world ray
    cam.set_function("screen_to_ray", [](uint32_t entity_id, const Vec2& screen_pos,
                                         uint32_t screen_width, uint32_t screen_height)
                                        -> sol::optional<std::tuple<Vec3, Vec3>> {
        auto* world = get_current_script_world();
        if (!world) return sol::nullopt;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return sol::nullopt;

        auto* camera = world->try_get<Camera>(entity);
        auto* world_transform = world->try_get<WorldTransform>(entity);
        if (!camera || !world_transform) return sol::nullopt;

        // Convert screen to NDC
        float ndc_x = (screen_pos.x / static_cast<float>(screen_width)) * 2.0f - 1.0f;
        float ndc_y = 1.0f - (screen_pos.y / static_cast<float>(screen_height)) * 2.0f; // Y flipped

        Mat4 view = glm::inverse(world_transform->matrix);
        Mat4 proj = camera->projection();
        Mat4 inv_vp = glm::inverse(proj * view);

        Vec4 near_point = inv_vp * Vec4(ndc_x, ndc_y, -1.0f, 1.0f);
        Vec4 far_point = inv_vp * Vec4(ndc_x, ndc_y, 1.0f, 1.0f);

        Vec3 ray_origin = Vec3(near_point) / near_point.w;
        Vec3 ray_end = Vec3(far_point) / far_point.w;
        Vec3 ray_dir = glm::normalize(ray_end - ray_origin);

        return std::make_tuple(ray_origin, ray_dir);
    });

    // Get camera forward direction
    cam.set_function("get_forward", [](uint32_t entity_id) -> Vec3 {
        auto* world = get_current_script_world();
        if (!world) return Vec3{0.0f, 0.0f, -1.0f};

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return Vec3{0.0f, 0.0f, -1.0f};

        auto* transform = world->try_get<LocalTransform>(entity);
        return transform ? transform->forward() : Vec3{0.0f, 0.0f, -1.0f};
    });

    // Get camera position
    cam.set_function("get_position", [](uint32_t entity_id) -> Vec3 {
        auto* world = get_current_script_world();
        if (!world) return Vec3{0.0f};

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return Vec3{0.0f};

        auto* world_transform = world->try_get<WorldTransform>(entity);
        return world_transform ? world_transform->position() : Vec3{0.0f};
    });
}

} // namespace engine::script
