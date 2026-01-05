#include <engine/render/render_systems.hpp>
#include <engine/render/animation_state_machine.hpp>
#include <engine/render/ik.hpp>
#include <engine/render/lod.hpp>
#include <engine/render/camera_effects.hpp>
#include <engine/render/debug_draw.hpp>
#include <engine/render/particle_system.hpp>
#include <engine/render/decal_system.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/scene/render_components.hpp>
#include <unordered_map>

namespace engine::render {

// Global render context
static RenderContext s_render_context;

// Global particle system
static ParticleSystem s_particle_system;

// Map from entity to particle runtime (for lifecycle management)
static std::unordered_map<uint32_t, ParticleEmitterRuntime*> s_entity_particle_runtimes;

RenderContext& get_render_context() {
    return s_render_context;
}

ParticleSystem& get_particle_system() {
    return s_particle_system;
}

void init_render_systems(RenderPipeline* pipeline, IRenderer* renderer) {
    s_render_context.pipeline = pipeline;
    s_render_context.renderer = renderer;

    // Initialize particle system
    s_particle_system.init(renderer);

    // Initialize decal system
    DecalSystemConfig decal_config;
    decal_config.max_decals = 1024;
    decal_config.max_definitions = 64;
    get_decal_system().init(decal_config);
}

void shutdown_render_systems() {
    // Clean up particle runtimes
    for (auto& [entity_id, runtime] : s_entity_particle_runtimes) {
        s_particle_system.destroy_emitter_runtime(runtime);
    }
    s_entity_particle_runtimes.clear();

    // Shutdown particle system
    s_particle_system.shutdown();

    // Shutdown decal system
    get_decal_system().shutdown();

    s_render_context.pipeline = nullptr;
    s_render_context.renderer = nullptr;
    s_render_context.clear();
}

// ============================================================================
// ANIMATION UPDATE SYSTEM
// ============================================================================

void animation_update_system(scene::World& world, double dt) {
    using namespace scene;

    float fdt = static_cast<float>(dt);

    auto view = world.view<AnimatorComponent>();
    for (auto entity : view) {
        auto& animator = view.get<AnimatorComponent>(entity);

        if (!animator.state_machine || !animator.state_machine->is_running()) {
            continue;
        }

        // Update the animation state machine
        animator.state_machine->update(fdt);

        // Get the computed pose and update skeleton instance
        const auto& pose = animator.state_machine->get_pose();
        if (!pose.empty()) {
            animator.skeleton_instance.get_pose() = pose;
            animator.skeleton_instance.calculate_matrices();
        }

        // Handle root motion if enabled
        if (animator.apply_root_motion) {
            const auto& root_motion = animator.state_machine->get_root_motion();
            animator.accumulated_root_translation += root_motion.translation_delta;
            animator.accumulated_root_rotation = root_motion.rotation_delta * animator.accumulated_root_rotation;

            // Apply to local transform if entity has one
            auto* local_tf = world.try_get<LocalTransform>(entity);
            if (local_tf) {
                local_tf->position += root_motion.translation_delta;
                local_tf->rotation = root_motion.rotation_delta * local_tf->rotation;
            }
        }
    }
}

// ============================================================================
// IK UPDATE SYSTEM
// ============================================================================

void ik_update_system(scene::World& world, double dt) {
    using namespace scene;

    float fdt = static_cast<float>(dt);

    auto view = world.view<IKComponent, AnimatorComponent>();
    for (auto entity : view) {
        auto& ik = view.get<IKComponent>(entity);
        auto& animator = view.get<AnimatorComponent>(entity);

        if (!animator.state_machine || !animator.state_machine->is_running()) {
            continue;
        }

        const Skeleton* skeleton = animator.state_machine->get_skeleton();
        if (!skeleton) {
            continue;
        }

        // Get world transform for IK calculations
        Mat4 world_transform{1.0f};
        auto* wt = world.try_get<WorldTransform>(entity);
        if (wt) {
            world_transform = wt->matrix;
        }

        // Get mutable pose from animator
        auto& pose = animator.state_machine->get_pose();

        // Apply IK corrections
        ik.process(pose, *skeleton, world_transform, fdt);

        // Update skeleton instance with IK-corrected pose
        animator.skeleton_instance.get_pose() = pose;
        animator.skeleton_instance.calculate_matrices();
    }
}

// ============================================================================
// CAMERA CONTROLLER SYSTEM
// ============================================================================

void camera_controller_system(scene::World& world, double dt) {
    using namespace scene;

    float fdt = static_cast<float>(dt);
    auto& effects = get_camera_effects();

    // Update global camera effects (shakes, trauma decay)
    effects.update(fdt);

    auto view = world.view<CameraControllerComponent, Camera, LocalTransform>();
    for (auto entity : view) {
        auto& controller = view.get<CameraControllerComponent>(entity);
        auto& camera = view.get<Camera>(entity);
        auto& local_tf = view.get<LocalTransform>(entity);

        if (!camera.active) {
            continue;
        }

        Vec3 position = local_tf.position;
        Quat rotation = local_tf.rotation;

        switch (controller.mode) {
            case CameraControllerComponent::Mode::Follow: {
                // Get target entity transform
                if (controller.follow_target_entity != 0) {
                    Entity target = static_cast<Entity>(controller.follow_target_entity);
                    auto* target_wt = world.try_get<WorldTransform>(target);
                    if (target_wt) {
                        Vec3 target_pos = target_wt->position();
                        Quat target_rot = target_wt->rotation();
                        effects.set_follow_target(target_pos, target_rot);
                    }
                }
                effects.get_follow_settings() = controller.follow;
                effects.update_follow(fdt, position, rotation);
                break;
            }

            case CameraControllerComponent::Mode::Orbit: {
                effects.get_orbit_settings() = controller.orbit;
                effects.update_orbit(fdt, position, rotation);
                break;
            }

            case CameraControllerComponent::Mode::Fixed:
            case CameraControllerComponent::Mode::Free:
            default:
                break;
        }

        // Apply shake effects
        if (controller.enable_shake) {
            Vec3 shake_offset = effects.get_shake_offset() * controller.shake_multiplier;
            Vec3 shake_rotation = effects.get_shake_rotation() * controller.shake_multiplier;

            position += shake_offset;
            rotation = glm::quat(glm::radians(shake_rotation)) * rotation;
        }

        local_tf.position = position;
        local_tf.rotation = rotation;
    }
}

// ============================================================================
// LOD SELECTION SYSTEM
// ============================================================================

void lod_select_system(scene::World& world, double dt) {
    using namespace scene;

    auto& ctx = get_render_context();
    if (!ctx.has_active_camera) {
        return;
    }

    float fdt = static_cast<float>(dt);
    static LODSelector selector;

    auto view = world.view<LODComponent, MeshRenderer, WorldTransform>();
    for (auto entity : view) {
        auto& lod = view.get<LODComponent>(entity);
        auto& mesh_renderer = view.get<MeshRenderer>(entity);
        auto& world_tf = view.get<WorldTransform>(entity);

        if (!lod.enabled || lod.lod_group.empty()) {
            continue;
        }

        // Calculate bounds in world space
        AABB world_bounds;
        world_bounds.min = Vec3(world_tf.matrix * Vec4(-0.5f, -0.5f, -0.5f, 1.0f));
        world_bounds.max = Vec3(world_tf.matrix * Vec4(0.5f, 0.5f, 0.5f, 1.0f));

        // Apply custom bias if set
        if (lod.use_custom_bias) {
            selector.set_global_bias(lod.custom_bias);
        }

        // Select LOD level
        LODSelectionResult result = selector.select(lod.lod_group, world_bounds, ctx.camera);
        lod.last_result = result;

        // Update transition state
        if (result.target_lod != lod.state.target_lod) {
            lod.state.start_transition(result.target_lod, lod.lod_group.fade_duration);
        }
        lod.state.update(fdt);

        // Update mesh renderer with current LOD
        if (!result.is_culled && lod.state.current_lod >= 0) {
            size_t lod_idx = static_cast<size_t>(lod.state.current_lod);
            if (lod_idx < lod.lod_group.levels.size()) {
                const auto& level = lod.lod_group.levels[lod_idx];
                mesh_renderer.mesh.id = level.mesh.id;
                if (level.material.valid()) {
                    mesh_renderer.material.id = level.material.id;
                }
            }
        }

        mesh_renderer.visible = !result.is_culled;
    }
}

// ============================================================================
// SKYBOX GATHER SYSTEM
// ============================================================================

void skybox_gather_system(scene::World& world, double dt) {
    using namespace scene;

    auto& ctx = get_render_context();

    // Find skybox component (usually attached to camera or scene root)
    auto view = world.view<Skybox>();
    for (auto entity : view) {
        auto& skybox = view.get<Skybox>(entity);

        if (skybox.cubemap.valid()) {
            ctx.skybox.cubemap.id = skybox.cubemap.id;
            ctx.skybox.intensity = skybox.intensity;
            ctx.skybox.rotation = skybox.rotation;
            ctx.skybox.valid = true;

            // Set on the pipeline
            if (ctx.pipeline) {
                ctx.pipeline->set_skybox(
                    TextureHandle{skybox.cubemap.id},
                    skybox.intensity,
                    skybox.rotation
                );
            }

            // Only use first valid skybox
            break;
        }
    }

    // If no skybox found, clear it from the pipeline
    if (!ctx.skybox.valid && ctx.pipeline) {
        ctx.pipeline->clear_skybox();
    }
}

// ============================================================================
// LIGHT GATHER SYSTEM
// ============================================================================

void light_gather_system(scene::World& world, double dt) {
    using namespace scene;

    auto& ctx = get_render_context();

    auto view = world.view<Light, WorldTransform>();
    for (auto entity : view) {
        auto& light = view.get<Light>(entity);
        auto& world_tf = view.get<WorldTransform>(entity);

        if (!light.enabled) {
            continue;
        }

        LightData light_data;
        light_data.position = world_tf.position();
        light_data.color = light.color;
        light_data.intensity = light.intensity;
        light_data.range = light.range;
        light_data.cast_shadows = light.cast_shadows;

        // Calculate direction from rotation
        Quat rot = world_tf.rotation();
        light_data.direction = rot * Vec3(0.0f, 0.0f, -1.0f);

        switch (light.type) {
            case LightType::Directional:
                light_data.type = 0;
                break;
            case LightType::Point:
                light_data.type = 1;
                break;
            case LightType::Spot:
                light_data.type = 2;
                light_data.inner_angle = light.spot_inner_angle;
                light_data.outer_angle = light.spot_outer_angle;
                break;
        }

        ctx.lights.push_back(light_data);
    }
}

// ============================================================================
// BILLBOARD GATHER SYSTEM
// ============================================================================

void billboard_gather_system(scene::World& world, double dt) {
    using namespace scene;

    auto& ctx = get_render_context();

    // Group billboards by texture for efficient batching
    std::unordered_map<uint32_t, BillboardBatch> batches;

    auto view = world.view<Billboard, WorldTransform>();
    for (auto entity : view) {
        auto& billboard = view.get<Billboard>(entity);
        auto& world_tf = view.get<WorldTransform>(entity);

        if (!billboard.visible || !billboard.texture.valid()) {
            continue;
        }

        // Get or create batch for this texture
        uint32_t tex_id = billboard.texture.id;
        auto& batch = batches[tex_id];

        if (!batch.texture.valid()) {
            batch.texture.id = tex_id;
            batch.mode = static_cast<render::BillboardMode>(static_cast<uint8_t>(billboard.mode));
            batch.depth_test = billboard.depth_test;
        }

        // Add instance to batch
        BillboardInstance instance;
        instance.position = world_tf.position();
        instance.size = billboard.size;
        instance.color = billboard.color;
        instance.uv_offset = billboard.uv_offset;
        instance.uv_scale = billboard.uv_scale;
        instance.rotation = billboard.rotation;

        batch.instances.push_back(instance);
    }

    // Move batches to context
    for (auto& [tex_id, batch] : batches) {
        ctx.billboards.push_back(std::move(batch));
    }
}

// ============================================================================
// RENDER GATHER SYSTEM
// ============================================================================

void render_gather_system(scene::World& world, double dt) {
    using namespace scene;

    auto& ctx = get_render_context();
    ctx.dt = static_cast<float>(dt);

    // First, find and setup the active camera
    {
        auto camera_view = world.view<Camera, WorldTransform>();
        for (auto entity : camera_view) {
            auto& camera = camera_view.get<Camera>(entity);
            auto& world_tf = camera_view.get<WorldTransform>(entity);

            if (!camera.active) {
                continue;
            }

            // Found active camera
            ctx.has_active_camera = true;

            Vec3 pos = world_tf.position();
            Quat rot = world_tf.rotation();
            Vec3 forward = rot * Vec3(0.0f, 0.0f, -1.0f);
            Vec3 up = rot * Vec3(0.0f, 1.0f, 0.0f);
            Vec3 right = rot * Vec3(1.0f, 0.0f, 0.0f);

            ctx.camera.position = pos;
            ctx.camera.forward = forward;
            ctx.camera.up = up;
            ctx.camera.right = right;
            ctx.camera.fov_y = camera.fov;
            ctx.camera.aspect_ratio = camera.aspect_ratio;
            ctx.camera.near_plane = camera.near_plane;
            ctx.camera.far_plane = camera.far_plane;

            // Calculate matrices
            ctx.camera.view_matrix = glm::lookAt(pos, pos + forward, up);
            ctx.camera.projection_matrix = camera.projection();
            ctx.camera.view_projection = ctx.camera.projection_matrix * ctx.camera.view_matrix;
            ctx.camera.inverse_view = glm::inverse(ctx.camera.view_matrix);
            ctx.camera.inverse_projection = glm::inverse(ctx.camera.projection_matrix);
            ctx.camera.inverse_view_projection = glm::inverse(ctx.camera.view_projection);

            // Store previous frame matrix for TAA (should be tracked properly)
            ctx.camera.prev_view_projection = ctx.camera.view_projection;

            break;  // Only use first active camera
        }
    }

    if (!ctx.has_active_camera) {
        return;
    }

    // Gather mesh renderers
    {
        auto view = world.view<MeshRenderer, WorldTransform>();
        for (auto entity : view) {
            auto& mesh_renderer = view.get<MeshRenderer>(entity);
            auto& world_tf = view.get<WorldTransform>(entity);

            if (!mesh_renderer.visible || !mesh_renderer.mesh.valid()) {
                continue;
            }

            RenderObject obj;
            obj.mesh.id = mesh_renderer.mesh.id;
            obj.material.id = mesh_renderer.material.id;
            obj.transform = world_tf.matrix;
            obj.layer_mask = 1u << mesh_renderer.render_layer;
            obj.casts_shadows = mesh_renderer.cast_shadows;
            obj.receives_shadows = mesh_renderer.receive_shadows;

            // Check for previous transform for motion vectors
            auto* prev_tf = world.try_get<PreviousTransform>(entity);
            if (prev_tf) {
                obj.prev_transform = prev_tf->matrix;
            } else {
                obj.prev_transform = obj.transform;
            }

            // Check for skinned mesh
            auto* animator = world.try_get<AnimatorComponent>(entity);
            if (animator && animator->skeleton_instance.get_skeleton()) {
                obj.skinned = true;
                obj.bone_matrices = animator->skeleton_instance.get_bone_matrices().data();
                obj.bone_count = static_cast<uint32_t>(animator->skeleton_instance.get_bone_matrices().size());
            }

            ctx.objects.push_back(obj);
        }
    }
}

// ============================================================================
// RENDER SUBMIT SYSTEM
// ============================================================================

void render_submit_system(scene::World& world, double dt) {
    auto& ctx = get_render_context();

    if (!ctx.pipeline || !ctx.has_active_camera) {
        return;
    }

    // Execute the render pipeline
    ctx.pipeline->begin_frame();
    ctx.pipeline->render(ctx.camera, ctx.objects, ctx.lights);
    ctx.pipeline->end_frame();

    // Clear gathered data for next frame
    ctx.clear();
}

// ============================================================================
// DEBUG DRAW SYSTEM
// ============================================================================

void debug_draw_system(scene::World& world, double dt) {
    auto& ctx = get_render_context();

    if (!ctx.renderer) {
        return;
    }

    // Update debug draw time for expiration
    DebugDraw::update_time(static_cast<float>(dt));

    // Flush all debug draws to the renderer
    DebugDraw::flush(ctx.renderer);
}

// ============================================================================
// PARTICLE UPDATE SYSTEM
// ============================================================================

// Convert ParticleEmitter component to ParticleEmitterConfig
static ParticleEmitterConfig emitter_to_config(const scene::ParticleEmitter& emitter) {
    ParticleEmitterConfig config;
    config.max_particles = emitter.max_particles;
    config.emission_rate = emitter.emission_rate;
    config.lifetime = emitter.lifetime;
    config.initial_velocity = Vec3(0.0f, emitter.initial_speed, 0.0f);
    config.velocity_variance = emitter.initial_velocity_variance;
    config.initial_size = emitter.start_size;
    config.gravity = emitter.gravity;
    config.enabled = emitter.enabled;
    config.loop = true;

    // Set up color gradient
    config.color_over_life.keys.clear();
    config.color_over_life.keys.push_back({emitter.start_color, 0.0f});
    config.color_over_life.keys.push_back({emitter.end_color, 1.0f});

    // Set up size curve
    config.size_over_life.keys.clear();
    config.size_over_life.keys.push_back({1.0f, 0.0f});
    float size_ratio = emitter.end_size / std::max(emitter.start_size, 0.001f);
    config.size_over_life.keys.push_back({size_ratio, 1.0f});

    return config;
}

void particle_update_system(scene::World& world, double dt) {
    using namespace scene;

    float fdt = static_cast<float>(dt);

    auto view = world.view<ParticleEmitter, WorldTransform>();
    for (auto entity : view) {
        auto& emitter = view.get<ParticleEmitter>(entity);
        auto& world_tf = view.get<WorldTransform>(entity);

        uint32_t entity_id = static_cast<uint32_t>(entity);

        // Get or create runtime for this entity
        auto it = s_entity_particle_runtimes.find(entity_id);
        ParticleEmitterRuntime* runtime = nullptr;

        if (it == s_entity_particle_runtimes.end()) {
            // Create new runtime
            ParticleEmitterConfig config = emitter_to_config(emitter);
            runtime = s_particle_system.create_emitter_runtime(config);
            if (runtime) {
                s_entity_particle_runtimes[entity_id] = runtime;
            }
        } else {
            runtime = it->second;
        }

        if (!runtime) {
            continue;
        }

        // Update the emitter
        if (emitter.enabled) {
            ParticleEmitterConfig config = emitter_to_config(emitter);
            s_particle_system.update_emitter(runtime, config, world_tf.matrix, fdt);
        }
    }

    // Render particles if we have an active camera
    auto& ctx = get_render_context();
    if (ctx.has_active_camera) {
        s_particle_system.render(ctx.camera);
    }
}

// ============================================================================
// DECAL UPDATE SYSTEM
// ============================================================================

void decal_update_system(scene::World& world, double dt) {
    using namespace scene;

    float fdt = static_cast<float>(dt);

    // Update the global decal system timing
    get_decal_system().update(fdt);

    // Update decal positions for entities with DecalComponent
    auto view = world.view<DecalComponent, WorldTransform>();
    for (auto entity : view) {
        auto& decal = view.get<DecalComponent>(entity);
        auto& world_tf = view.get<WorldTransform>(entity);

        // Skip if no valid decal handle or not following entity
        if (decal.decal_handle == INVALID_DECAL || !decal.follow_entity) {
            continue;
        }

        // Get the decal instance and update its position
        DecalInstance* instance = get_decal_system().get_instance(decal.decal_handle);
        if (instance) {
            // Calculate world position with local offset
            Vec3 world_pos = world_tf.position();
            Quat world_rot = world_tf.rotation();

            // Apply local offset in entity space
            instance->position = world_pos + world_rot * decal.local_offset;
            instance->rotation = world_rot * decal.local_rotation;
        }
    }

    // Update camera position for decal culling
    auto& ctx = get_render_context();
    if (ctx.has_active_camera) {
        get_decal_system().set_camera_position(ctx.camera.position);
    }
}

// ============================================================================
// REGISTRATION
// ============================================================================

void register_render_systems(scene::Scheduler& scheduler) {
    using scene::Phase;

    // Animation systems (Update phase)
    scheduler.add(Phase::Update, animation_update_system, "render_animation", 5);
    scheduler.add(Phase::Update, ik_update_system, "render_ik", 4);
    scheduler.add(Phase::Update, particle_update_system, "render_particles", 3);
    scheduler.add(Phase::Update, decal_update_system, "render_decals", 2);

    // Camera controller (PostUpdate phase)
    scheduler.add(Phase::PostUpdate, camera_controller_system, "render_camera_controller", 5);

    // Pre-render systems (PreRender phase)
    scheduler.add(Phase::PreRender, lod_select_system, "render_lod", 10);
    scheduler.add(Phase::PreRender, light_gather_system, "render_lights", 8);
    scheduler.add(Phase::PreRender, skybox_gather_system, "render_skybox", 7);
    scheduler.add(Phase::PreRender, billboard_gather_system, "render_billboards", 6);
    scheduler.add(Phase::PreRender, render_gather_system, "render_gather", 5);

    // Main render (Render phase)
    scheduler.add(Phase::Render, render_submit_system, "render_submit", 10);

    // Post-render (PostRender phase)
    scheduler.add(Phase::PostRender, debug_draw_system, "render_debug", 5);
}

} // namespace engine::render
