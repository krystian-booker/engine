#include <engine/script/bindings.hpp>
#include <engine/script/script_context.hpp>
#include <engine/scene/render_components.hpp>
#include <engine/scene/world.hpp>
#include <engine/render/camera_effects.hpp>
#include <engine/render/post_process.hpp>
#include <engine/core/log.hpp>

namespace engine::script {

// Forward declaration for global post-process access
namespace {
    engine::render::PostProcessSystem* s_post_process_system = nullptr;
}

void set_post_process_system(engine::render::PostProcessSystem* system) {
    s_post_process_system = system;
}

void register_render_bindings(sol::state& lua) {
    using namespace engine::scene;
    using namespace engine::render;
    using namespace engine::core;

    // =========================================================================
    // Light Table - Entity-based light control
    // =========================================================================
    auto light = lua.create_named_table("Light");

    // Light type constants
    light["DIRECTIONAL"] = static_cast<int>(LightType::Directional);
    light["POINT"] = static_cast<int>(LightType::Point);
    light["SPOT"] = static_cast<int>(LightType::Spot);

    // Check if entity has light
    light.set_function("has", [](uint32_t entity_id) -> bool {
        auto* world = get_current_script_world();
        if (!world) return false;

        auto entity = static_cast<entt::entity>(entity_id);
        return world->registry().valid(entity) && world->has<Light>(entity);
    });

    // Get light enabled state
    light.set_function("is_enabled", [](uint32_t entity_id) -> bool {
        auto* world = get_current_script_world();
        if (!world) return false;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return false;

        auto* l = world->try_get<Light>(entity);
        return l ? l->enabled : false;
    });

    // Set light enabled state
    light.set_function("set_enabled", [](uint32_t entity_id, bool enabled) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* l = world->try_get<Light>(entity);
        if (l) {
            l->enabled = enabled;
        }
    });

    // Get light color
    light.set_function("get_color", [](uint32_t entity_id) -> Vec3 {
        auto* world = get_current_script_world();
        if (!world) return Vec3{1.0f};

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return Vec3{1.0f};

        auto* l = world->try_get<Light>(entity);
        return l ? l->color : Vec3{1.0f};
    });

    // Set light color
    light.set_function("set_color", [](uint32_t entity_id, const Vec3& color) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* l = world->try_get<Light>(entity);
        if (l) {
            l->color = color;
        }
    });

    // Get light intensity
    light.set_function("get_intensity", [](uint32_t entity_id) -> float {
        auto* world = get_current_script_world();
        if (!world) return 1.0f;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return 1.0f;

        auto* l = world->try_get<Light>(entity);
        return l ? l->intensity : 1.0f;
    });

    // Set light intensity
    light.set_function("set_intensity", [](uint32_t entity_id, float intensity) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* l = world->try_get<Light>(entity);
        if (l) {
            l->intensity = intensity;
        }
    });

    // Get light range (point/spot)
    light.set_function("get_range", [](uint32_t entity_id) -> float {
        auto* world = get_current_script_world();
        if (!world) return 10.0f;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return 10.0f;

        auto* l = world->try_get<Light>(entity);
        return l ? l->range : 10.0f;
    });

    // Set light range
    light.set_function("set_range", [](uint32_t entity_id, float range) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* l = world->try_get<Light>(entity);
        if (l) {
            l->range = range;
        }
    });

    // Get light type as string
    light.set_function("get_type", [](uint32_t entity_id) -> std::string {
        auto* world = get_current_script_world();
        if (!world) return "point";

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return "point";

        auto* l = world->try_get<Light>(entity);
        if (!l) return "point";

        switch (l->type) {
            case LightType::Directional: return "directional";
            case LightType::Point: return "point";
            case LightType::Spot: return "spot";
            default: return "point";
        }
    });

    // Set spot light angles (inner and outer cone in degrees)
    light.set_function("set_spot_angles", [](uint32_t entity_id, float inner_deg, float outer_deg) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* l = world->try_get<Light>(entity);
        if (l && l->type == LightType::Spot) {
            l->spot_inner_angle = inner_deg;
            l->spot_outer_angle = outer_deg;
        }
    });

    // Set light cast shadows
    light.set_function("set_cast_shadows", [](uint32_t entity_id, bool cast) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* l = world->try_get<Light>(entity);
        if (l) {
            l->cast_shadows = cast;
        }
    });

    // =========================================================================
    // Render Table - MeshRenderer and material properties
    // =========================================================================
    auto render = lua.create_named_table("Render");

    // Check if entity has mesh renderer
    render.set_function("has_mesh_renderer", [](uint32_t entity_id) -> bool {
        auto* world = get_current_script_world();
        if (!world) return false;

        auto entity = static_cast<entt::entity>(entity_id);
        return world->registry().valid(entity) && world->has<MeshRenderer>(entity);
    });

    // Get visibility
    render.set_function("is_visible", [](uint32_t entity_id) -> bool {
        auto* world = get_current_script_world();
        if (!world) return false;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return false;

        auto* mr = world->try_get<MeshRenderer>(entity);
        return mr ? mr->visible : false;
    });

    // Set visibility
    render.set_function("set_visible", [](uint32_t entity_id, bool visible) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* mr = world->try_get<MeshRenderer>(entity);
        if (mr) {
            mr->visible = visible;
        }
    });

    // Set render layer
    render.set_function("set_render_layer", [](uint32_t entity_id, uint8_t layer) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* mr = world->try_get<MeshRenderer>(entity);
        if (mr) {
            mr->render_layer = layer;
        }
    });

    // Set cast shadows
    render.set_function("set_cast_shadows", [](uint32_t entity_id, bool cast) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* mr = world->try_get<MeshRenderer>(entity);
        if (mr) {
            mr->cast_shadows = cast;
        }
    });

    // Set receive shadows
    render.set_function("set_receive_shadows", [](uint32_t entity_id, bool receive) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* mr = world->try_get<MeshRenderer>(entity);
        if (mr) {
            mr->receive_shadows = receive;
        }
    });

    // Skybox intensity
    render.set_function("set_skybox_intensity", [](uint32_t entity_id, float intensity) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* skybox = world->try_get<Skybox>(entity);
        if (skybox) {
            skybox->intensity = intensity;
        }
    });

    // Skybox rotation
    render.set_function("set_skybox_rotation", [](uint32_t entity_id, float rotation) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* skybox = world->try_get<Skybox>(entity);
        if (skybox) {
            skybox->rotation = rotation;
        }
    });

    // =========================================================================
    // Billboard Table
    // =========================================================================
    auto billboard = lua.create_named_table("Billboard");

    billboard.set_function("has", [](uint32_t entity_id) -> bool {
        auto* world = get_current_script_world();
        if (!world) return false;

        auto entity = static_cast<entt::entity>(entity_id);
        return world->registry().valid(entity) && world->has<Billboard>(entity);
    });

    billboard.set_function("set_size", [](uint32_t entity_id, const Vec2& size) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* bb = world->try_get<Billboard>(entity);
        if (bb) {
            bb->size = size;
        }
    });

    billboard.set_function("set_color", [](uint32_t entity_id, const Vec4& color) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* bb = world->try_get<Billboard>(entity);
        if (bb) {
            bb->color = color;
        }
    });

    billboard.set_function("set_visible", [](uint32_t entity_id, bool visible) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* bb = world->try_get<Billboard>(entity);
        if (bb) {
            bb->visible = visible;
        }
    });

    billboard.set_function("set_mode", [](uint32_t entity_id, const std::string& mode) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* bb = world->try_get<Billboard>(entity);
        if (bb) {
            if (mode == "screen") {
                bb->mode = BillboardMode::ScreenAligned;
            } else if (mode == "axis") {
                bb->mode = BillboardMode::AxisAligned;
            } else if (mode == "fixed") {
                bb->mode = BillboardMode::Fixed;
            }
        }
    });

    // =========================================================================
    // PostProcess Table - Global post-processing settings
    // =========================================================================
    auto postprocess = lua.create_named_table("PostProcess");

    // Bloom settings
    postprocess.set_function("set_bloom_enabled", [](bool enabled) {
        if (!s_post_process_system) return;
        auto config = s_post_process_system->get_config();
        config.bloom.enabled = enabled;
        s_post_process_system->set_config(config);
    });

    postprocess.set_function("set_bloom_intensity", [](float intensity) {
        if (!s_post_process_system) return;
        auto config = s_post_process_system->get_config();
        config.bloom.intensity = intensity;
        s_post_process_system->set_config(config);
    });

    postprocess.set_function("set_bloom_threshold", [](float threshold) {
        if (!s_post_process_system) return;
        auto config = s_post_process_system->get_config();
        config.bloom.threshold = threshold;
        s_post_process_system->set_config(config);
    });

    // Exposure settings
    postprocess.set_function("set_exposure", [](float exposure) {
        if (!s_post_process_system) return;
        auto config = s_post_process_system->get_config();
        config.tonemapping.exposure = exposure;
        s_post_process_system->set_config(config);
    });

    postprocess.set_function("get_exposure", []() -> float {
        if (!s_post_process_system) return 1.0f;
        return s_post_process_system->get_config().tonemapping.exposure;
    });

    postprocess.set_function("set_auto_exposure", [](bool enabled) {
        if (!s_post_process_system) return;
        auto config = s_post_process_system->get_config();
        config.tonemapping.auto_exposure = enabled;
        s_post_process_system->set_config(config);
    });

    // Tonemapping mode
    postprocess.set_function("set_tonemapping", [](const std::string& mode) {
        if (!s_post_process_system) return;
        auto config = s_post_process_system->get_config();

        if (mode == "none") {
            config.tonemapping.op = ToneMappingOperator::None;
        } else if (mode == "reinhard") {
            config.tonemapping.op = ToneMappingOperator::Reinhard;
        } else if (mode == "aces") {
            config.tonemapping.op = ToneMappingOperator::ACES;
        } else if (mode == "uncharted2") {
            config.tonemapping.op = ToneMappingOperator::Uncharted2;
        } else if (mode == "agx") {
            config.tonemapping.op = ToneMappingOperator::AgX;
        }

        s_post_process_system->set_config(config);
    });

    // Vignette settings
    postprocess.set_function("set_vignette_enabled", [](bool enabled) {
        if (!s_post_process_system) return;
        auto config = s_post_process_system->get_config();
        config.vignette_enabled = enabled;
        s_post_process_system->set_config(config);
    });

    postprocess.set_function("set_vignette_intensity", [](float intensity) {
        if (!s_post_process_system) return;
        auto config = s_post_process_system->get_config();
        config.vignette_intensity = intensity;
        s_post_process_system->set_config(config);
    });

    postprocess.set_function("set_vignette_smoothness", [](float smoothness) {
        if (!s_post_process_system) return;
        auto config = s_post_process_system->get_config();
        config.vignette_smoothness = smoothness;
        s_post_process_system->set_config(config);
    });

    // Chromatic aberration
    postprocess.set_function("set_chromatic_aberration_enabled", [](bool enabled) {
        if (!s_post_process_system) return;
        auto config = s_post_process_system->get_config();
        config.chromatic_aberration = enabled;
        s_post_process_system->set_config(config);
    });

    postprocess.set_function("set_chromatic_aberration_intensity", [](float intensity) {
        if (!s_post_process_system) return;
        auto config = s_post_process_system->get_config();
        config.ca_intensity = intensity;
        s_post_process_system->set_config(config);
    });

    // =========================================================================
    // CameraFX Table - Camera shake and effects
    // =========================================================================
    auto camerafx = lua.create_named_table("CameraFX");

    // Shake type constants
    camerafx["SHAKE_PERLIN"] = static_cast<int>(ShakeType::Perlin);
    camerafx["SHAKE_RANDOM"] = static_cast<int>(ShakeType::Random);
    camerafx["SHAKE_SINE"] = static_cast<int>(ShakeType::Sine);
    camerafx["SHAKE_DIRECTIONAL"] = static_cast<int>(ShakeType::Directional);

    // Add trauma (Vlambeer-style)
    camerafx.set_function("add_trauma", [](float amount) {
        get_camera_effects().add_trauma(amount);
    });

    // Set trauma directly
    camerafx.set_function("set_trauma", [](float amount) {
        get_camera_effects().set_trauma(amount);
    });

    // Get current trauma
    camerafx.set_function("get_trauma", []() -> float {
        return get_camera_effects().get_trauma();
    });

    // Add custom shake
    camerafx.set_function("add_shake", [](int type, float amplitude, float frequency, float duration) -> uint32_t {
        CameraShake shake;
        shake.type = static_cast<ShakeType>(type);
        shake.position_amplitude = Vec3(amplitude);
        shake.rotation_amplitude = Vec3(amplitude * 10.0f); // Scale rotation
        shake.frequency = frequency;
        shake.duration = duration;
        return get_camera_effects().add_shake(shake);
    });

    // Remove shake by ID
    camerafx.set_function("remove_shake", [](uint32_t id) {
        get_camera_effects().remove_shake(id);
    });

    // Clear all shakes
    camerafx.set_function("clear_shakes", []() {
        get_camera_effects().clear_shakes();
    });

    // Preset shakes
    camerafx.set_function("explosion_shake", [](sol::optional<float> intensity) -> uint32_t {
        auto shake = CameraEffects::create_explosion_shake(intensity.value_or(1.0f));
        return get_camera_effects().add_shake(shake);
    });

    camerafx.set_function("impact_shake", [](sol::optional<float> intensity) -> uint32_t {
        auto shake = CameraEffects::create_impact_shake(intensity.value_or(1.0f));
        return get_camera_effects().add_shake(shake);
    });

    camerafx.set_function("footstep_shake", [](sol::optional<float> intensity) -> uint32_t {
        auto shake = CameraEffects::create_footstep_shake(intensity.value_or(0.2f));
        return get_camera_effects().add_shake(shake);
    });

    camerafx.set_function("continuous_shake", [](sol::optional<float> intensity, sol::optional<float> frequency) -> uint32_t {
        auto shake = CameraEffects::create_continuous_shake(
            intensity.value_or(0.5f),
            frequency.value_or(5.0f)
        );
        return get_camera_effects().add_shake(shake);
    });

    // Get current shake offset
    camerafx.set_function("get_shake_offset", []() -> Vec3 {
        return get_camera_effects().get_shake_offset();
    });

    // Get current shake rotation
    camerafx.set_function("get_shake_rotation", []() -> Vec3 {
        return get_camera_effects().get_shake_rotation();
    });
}

} // namespace engine::script
