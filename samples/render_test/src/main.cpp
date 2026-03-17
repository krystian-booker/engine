// Render Test Scene
// Deterministic scene that exercises all major rendering features:
//   PBR materials, shadows, SSAO, bloom, transparency
// No input handling, no animation — purely static and deterministic.
// Usage: render_test.exe --screenshot=output.png --screenshot-frame=60

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <engine/core/application.hpp>
#include <engine/core/log.hpp>
#include <engine/scene/scene.hpp>
#include <engine/render/light_probes.hpp>
#include <engine/render/renderer.hpp>
#include <engine/render/render_pipeline.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace engine::core;
using namespace engine::scene;
namespace render = engine::render;

class RenderTestApp : public Application {
protected:
    void on_init() override {
        log(LogLevel::Info, "[RenderTest] Initializing deterministic test scene...");

        auto* renderer = get_renderer();
        if (!renderer) {
            log(LogLevel::Error, "[RenderTest] Renderer not available");
            quit();
            return;
        }

        auto* world = get_world();
        if (!world) {
            log(LogLevel::Error, "[RenderTest] World not available");
            quit();
            return;
        }

        // Create primitive meshes
        m_sphere_mesh = renderer->create_primitive(render::PrimitiveMesh::Sphere, 1.0f);
        m_cube_mesh = renderer->create_primitive(render::PrimitiveMesh::Cube, 1.0f);
        m_plane_mesh = renderer->create_primitive(render::PrimitiveMesh::Plane, 1.0f);
        m_probe_primitives.clear();
        m_probe_lights.clear();

        // Configure the render pipeline
        auto* pipeline = get_render_pipeline();
        if (pipeline) {
            render::RenderPipelineConfig config;
            config.enabled_passes = render::RenderPassFlags::Shadows
                                  | render::RenderPassFlags::DepthPrepass
                                  | render::RenderPassFlags::GBuffer
                                  | render::RenderPassFlags::SSAO
                                  | render::RenderPassFlags::MainOpaque
                                  | render::RenderPassFlags::SSR
                                  | render::RenderPassFlags::Transparent
                                  | render::RenderPassFlags::PostProcess
                                  | render::RenderPassFlags::Final;

            // Tone mapping — AgX matches Blender's view transform.
            // The engine exposure parameter is a linear multiplier, so 1.0f is neutral
            // exposure (equivalent to Blender's 0 EV).
            config.tonemap_config.op = render::ToneMappingOperator::AgX;
            config.tonemap_config.exposure = 1.0f;

            // Bloom — matches Blender compositor Glare node
            config.bloom_config.enabled = true;
            config.bloom_config.threshold = 1.5f;
            config.bloom_config.intensity = 0.15f;

            // SSAO
            config.ssao_config.radius = 0.5f;
            config.ssao_config.intensity = 1.5f;

            // SSR
            config.ssr_config.use_hiz = false;
            config.ssr_config.temporal_enabled = false;
            config.ssr_config.jitter_enabled = false;
            config.ssr_config.resolution_scale = 1.0f;
            config.ssr_config.intensity = 1.0f;
            config.ssr_config.roughness_threshold = 0.75f;
            config.ssr_config.edge_fade_start = 0.85f;
            config.ssr_config.edge_fade_end = 0.98f;

            // Shadows
            config.shadow_config.cascade_resolution = 2048;
            config.shadow_config.cascade_count = 4;
            config.shadow_config.shadow_bias = 0.002f;
            config.shadow_config.normal_bias = 0.02f;

            // Canonical dark navy background from the Blender reference scene.
            config.clear_color = 0x1A1A2EFF;

            pipeline->set_config(config);
        }

        renderer->set_ibl_intensity(1.0f);
        renderer->set_hemisphere_ambient(
            Vec3{0.10f, 0.08f, 0.06f}, 0.0f,
            Vec3{0.02f, 0.02f, 0.03f}
        );

        // Create scene-appropriate dark IBL cubemaps matching the golden's
        // dark navy environment (sRGB ~26,26,46). The engine's fallback cubemaps
        // are much brighter (sRGB ~119,110,97), creating phantom metallic reflections.
        create_scene_ibl(renderer);

        create_camera(world);
        create_lights(world, renderer);
        create_ground(world, renderer);
        create_pbr_sphere_grid(world, renderer);
        create_shadow_casters(world, renderer);
        create_emissive_sphere(world, renderer);
        create_ssao_corner(world, renderer);
        create_glass_sphere(world, renderer);
        setup_light_probes();

        log(LogLevel::Info, "[RenderTest] Scene initialized.");
    }

    void on_shutdown() override {
        log(LogLevel::Info, "[RenderTest] Shutting down...");

        if (m_probe_volume != render::INVALID_PROBE_VOLUME) {
            auto& probe_system = render::get_light_probe_system();
            if (probe_system.is_initialized()) {
                probe_system.destroy_volume(m_probe_volume);
            }
            m_probe_volume = render::INVALID_PROBE_VOLUME;
        }

        if (auto* renderer = get_renderer()) {
            renderer->destroy_mesh(m_sphere_mesh);
            renderer->destroy_mesh(m_cube_mesh);
            renderer->destroy_mesh(m_plane_mesh);
            if (m_ibl_irradiance.valid()) renderer->destroy_texture(m_ibl_irradiance);
            if (m_ibl_prefilter.valid()) renderer->destroy_texture(m_ibl_prefilter);
            if (m_ibl_brdf_lut.valid()) renderer->destroy_texture(m_ibl_brdf_lut);
            for (auto& mat : m_materials) {
                renderer->destroy_material(mat);
            }
        }
    }

private:
    enum class ProbeShape {
        Sphere,
        Box,
    };

    struct ProbeBakeMaterial {
        Vec3 albedo{1.0f};
        Vec3 emissive{0.0f};
        float metallic = 0.0f;
        float transmission = 0.0f;
    };

    struct ProbePrimitive {
        ProbeShape shape = ProbeShape::Sphere;
        Vec3 position{0.0f};
        Vec3 extents{1.0f};
        ProbeBakeMaterial material;
    };

    struct ProbeDirectionalLight {
        Vec3 direction{0.0f, -1.0f, 0.0f};
        Vec3 color{1.0f};
        float intensity = 1.0f;
        bool casts_shadows = false;
    };

    // ---- Camera ----
    void create_camera(World* world) {
        auto cam_entity = world->create("Camera");
        Vec3 cam_pos{0.0f, 6.0f, 14.0f};
        Vec3 look_target{0.0f, 1.0f, 0.0f};
        auto& tf = world->emplace<LocalTransform>(cam_entity, cam_pos);
        Quat rot = glm::quatLookAt(glm::normalize(look_target - cam_pos), Vec3(0.0f, 1.0f, 0.0f));
        tf.rotation = rot;
        world->emplace<WorldTransform>(cam_entity);

        Camera cam;
        cam.fov = 55.0f;
        cam.aspect_ratio = static_cast<float>(window_width()) / static_cast<float>(window_height());
        cam.near_plane = 0.1f;
        cam.far_plane = 200.0f;
        cam.active = true;
        world->emplace<Camera>(cam_entity, cam);
    }

    // ---- Lights ----
    void create_lights(World* world, render::IRenderer* /*renderer*/) {
        // Sun light — warm white, casts shadows
        {
            auto entity = world->create("Sun");
            Vec3 dir = glm::normalize(Vec3{-0.4f, -1.0f, -0.3f});
            Vec3 up = (std::abs(dir.y) > 0.99f) ? Vec3(0.0f, 0.0f, 1.0f) : Vec3(0.0f, 1.0f, 0.0f);
            Quat rot = glm::quatLookAt(dir, up);
            world->emplace<LocalTransform>(entity, Vec3{0.0f}, rot);
            world->emplace<WorldTransform>(entity);

            Light light;
            light.type = LightType::Directional;
            light.color = Vec3{1.0f, 0.95f, 0.9f};
            light.intensity = 2.0f;
            light.cast_shadows = true;
            light.enabled = true;
            world->emplace<Light>(entity, light);
            m_probe_lights.push_back({dir, light.color, light.intensity, light.cast_shadows});
        }

        // Fill light — cool blue, no shadows
        {
            auto entity = world->create("Fill");
            Vec3 dir = glm::normalize(Vec3{0.5f, -0.3f, 0.5f});
            Vec3 up = (std::abs(dir.y) > 0.99f) ? Vec3(0.0f, 0.0f, 1.0f) : Vec3(0.0f, 1.0f, 0.0f);
            Quat rot = glm::quatLookAt(dir, up);
            world->emplace<LocalTransform>(entity, Vec3{0.0f}, rot);
            world->emplace<WorldTransform>(entity);

            Light light;
            light.type = LightType::Directional;
            light.color = Vec3{0.6f, 0.7f, 1.0f};
            light.intensity = 0.3f;
            light.cast_shadows = false;
            light.enabled = true;
            world->emplace<Light>(entity, light);
            m_probe_lights.push_back({dir, light.color, light.intensity, light.cast_shadows});
        }
    }

    void add_probe_box(const Vec3& position, const Vec3& scale, const render::MaterialData& material) {
        ProbePrimitive primitive;
        primitive.shape = ProbeShape::Box;
        primitive.position = position;
        primitive.extents = scale * 0.5f;
        primitive.material.albedo = Vec3(material.albedo);
        primitive.material.emissive = material.emissive;
        primitive.material.metallic = material.metallic;
        primitive.material.transmission = material.transmission;
        m_probe_primitives.push_back(primitive);
    }

    void add_probe_sphere(const Vec3& position, float radius, const render::MaterialData& material) {
        ProbePrimitive primitive;
        primitive.shape = ProbeShape::Sphere;
        primitive.position = position;
        primitive.extents = Vec3(radius);
        primitive.material.albedo = Vec3(material.albedo);
        primitive.material.emissive = material.emissive;
        primitive.material.metallic = material.metallic;
        primitive.material.transmission = material.transmission;
        m_probe_primitives.push_back(primitive);
    }

    bool intersect_sphere(const Ray& ray, const ProbePrimitive& primitive, float max_distance,
                          float& out_distance, Vec3& out_normal) const {
        Vec3 offset = ray.origin - primitive.position;
        float radius = primitive.extents.x;
        float a = dot(ray.direction, ray.direction);
        float b = 2.0f * dot(offset, ray.direction);
        float c = dot(offset, offset) - radius * radius;
        float discriminant = b * b - 4.0f * a * c;
        if (discriminant < 0.0f) {
            return false;
        }

        float sqrt_discriminant = std::sqrt(discriminant);
        float t0 = (-b - sqrt_discriminant) / (2.0f * a);
        float t1 = (-b + sqrt_discriminant) / (2.0f * a);
        float distance = (t0 > 0.001f) ? t0 : t1;
        if (distance <= 0.001f || distance >= max_distance) {
            return false;
        }

        Vec3 hit_position = ray.origin + ray.direction * distance;
        out_distance = distance;
        out_normal = glm::normalize(hit_position - primitive.position);
        return true;
    }

    bool intersect_box(const Ray& ray, const ProbePrimitive& primitive, float max_distance,
                       float& out_distance, Vec3& out_normal) const {
        Vec3 box_min = primitive.position - primitive.extents;
        Vec3 box_max = primitive.position + primitive.extents;

        float t_min = 0.001f;
        float t_max = max_distance;
        int hit_axis = -1;
        float hit_sign = 1.0f;

        for (int axis = 0; axis < 3; ++axis) {
            float origin = ray.origin[axis];
            float direction = ray.direction[axis];

            if (std::abs(direction) < 1e-5f) {
                if (origin < box_min[axis] || origin > box_max[axis]) {
                    return false;
                }
                continue;
            }

            float inv_dir = 1.0f / direction;
            float t0 = (box_min[axis] - origin) * inv_dir;
            float t1 = (box_max[axis] - origin) * inv_dir;
            float axis_sign = -1.0f;
            if (t0 > t1) {
                std::swap(t0, t1);
                axis_sign = 1.0f;
            }

            if (t0 > t_min) {
                t_min = t0;
                hit_axis = axis;
                hit_sign = axis_sign;
            }
            t_max = std::min(t_max, t1);
            if (t_min > t_max) {
                return false;
            }
        }

        if (hit_axis < 0 || t_min >= max_distance) {
            return false;
        }

        out_distance = t_min;
        out_normal = Vec3(0.0f);
        out_normal[hit_axis] = hit_sign;
        return true;
    }

    const ProbePrimitive* trace_probe_scene(const Ray& ray, float max_distance,
                                            Vec3& out_position, Vec3& out_normal,
                                            float& out_distance) const {
        const ProbePrimitive* hit_primitive = nullptr;
        float closest_distance = max_distance;
        Vec3 hit_normal{0.0f};

        for (const ProbePrimitive& primitive : m_probe_primitives) {
            float candidate_distance = max_distance;
            Vec3 candidate_normal{0.0f};
            bool hit = false;

            if (primitive.shape == ProbeShape::Sphere) {
                hit = intersect_sphere(ray, primitive, closest_distance, candidate_distance, candidate_normal);
            } else {
                hit = intersect_box(ray, primitive, closest_distance, candidate_distance, candidate_normal);
            }

            if (!hit) {
                continue;
            }

            closest_distance = candidate_distance;
            hit_normal = candidate_normal;
            hit_primitive = &primitive;
        }

        if (!hit_primitive) {
            return nullptr;
        }

        out_distance = closest_distance;
        out_position = ray.origin + ray.direction * closest_distance;
        out_normal = hit_normal;
        return hit_primitive;
    }

    bool is_occluded(const Vec3& origin, const Vec3& direction, float max_distance) const {
        Vec3 hit_position{0.0f};
        Vec3 hit_normal{0.0f};
        float hit_distance = max_distance;
        return trace_probe_scene(Ray{origin, direction}, max_distance, hit_position, hit_normal, hit_distance) != nullptr;
    }

    Vec3 shade_probe_surface(const ProbePrimitive& primitive, const Vec3& position, const Vec3& normal) const {
        Vec3 radiance = primitive.material.emissive;
        Vec3 diffuse_albedo = primitive.material.albedo *
            (1.0f - primitive.material.metallic) *
            (1.0f - primitive.material.transmission);

        for (const ProbeDirectionalLight& light : m_probe_lights) {
            Vec3 to_light = glm::normalize(-light.direction);
            float n_dot_l = glm::max(dot(normal, to_light), 0.0f);
            if (n_dot_l <= 0.0f) {
                continue;
            }

            if (light.casts_shadows) {
                Vec3 shadow_origin = position + normal * 0.02f;
                if (is_occluded(shadow_origin, to_light, 1000.0f)) {
                    continue;
                }
            }

            radiance += diffuse_albedo * light.color * light.intensity * n_dot_l * (1.0f / glm::pi<float>());
        }

        return radiance;
    }

    render::ProbeRayHit bake_probe_ray(const Vec3& origin, const Vec3& direction) const {
        render::ProbeRayHit hit{};
        Vec3 hit_position{0.0f};
        Vec3 hit_normal{0.0f};
        float hit_distance = std::numeric_limits<float>::max();
        const ProbePrimitive* primitive = trace_probe_scene(
            Ray{origin, direction}, 1000.0f, hit_position, hit_normal, hit_distance);

        if (!primitive) {
            return hit;
        }

        hit.position = hit_position;
        hit.normal = hit_normal;
        hit.color = shade_probe_surface(*primitive, hit_position, hit_normal);
        hit.distance = hit_distance;
        hit.hit = true;
        return hit;
    }

    void setup_light_probes() {
        auto& probe_system = render::get_light_probe_system();
        if (!probe_system.is_initialized()) {
            probe_system.init();
        }

        const Vec3 sky_color = Vec3{0.0203f, 0.0203f, 0.0410f};
        probe_system.set_sky_color(sky_color);
        probe_system.set_sky_sh(render::LightProbeUtils::create_ambient_sh(sky_color));

        m_probe_volume = probe_system.create_volume(
            Vec3{-8.5f, 0.0f, -6.5f},
            Vec3{8.5f, 4.5f, 6.5f},
            5, 3, 5);
        if (m_probe_volume == render::INVALID_PROBE_VOLUME) {
            log(LogLevel::Warn, "[RenderTest] Failed to allocate light probe volume");
            return;
        }

        render::LightProbeBakeSettings bake_settings;
        bake_settings.samples_per_probe = 128;
        bake_settings.include_sky = true;
        bake_settings.include_emissives = true;
        bake_settings.intensity_multiplier = 1.0f;

        probe_system.bake_volume(m_probe_volume, bake_settings,
            [this](const Vec3& origin, const Vec3& direction) {
                return bake_probe_ray(origin, direction);
            });
        probe_system.upload_to_gpu();
    }

    // ---- Ground Plane ----
    void create_ground(World* world, render::IRenderer* renderer) {
        render::MaterialData mat_data;
        mat_data.albedo = Vec4{0.5f, 0.5f, 0.52f, 1.0f};
        mat_data.roughness = 0.95f;
        mat_data.metallic = 0.0f;
        auto mat = renderer->create_material(mat_data);
        m_materials.push_back(mat);

        auto entity = world->create("Ground");
        auto& tf = world->emplace<LocalTransform>(entity, Vec3{0.0f, 0.0f, 0.0f});
        tf.scale = Vec3{20.0f, 0.1f, 20.0f};
        world->emplace<WorldTransform>(entity);
        world->emplace<PreviousTransform>(entity);
        world->emplace<MeshRenderer>(entity, MeshRenderer{
            MeshHandle{m_cube_mesh.id}, MaterialHandle{mat.id}, 0, true, false, true
        });
        add_probe_box(Vec3{0.0f, 0.0f, 0.0f}, Vec3{20.0f, 0.1f, 20.0f}, mat_data);
    }

    // ---- 5x5 PBR Sphere Grid ----
    // Metallic 0->1 across X, roughness 0.1->1.0 across Z
    void create_pbr_sphere_grid(World* world, render::IRenderer* renderer) {
        constexpr int GRID_SIZE = 5;
        constexpr float SPACING = 2.0f;
        constexpr float START_X = -(GRID_SIZE - 1) * SPACING * 0.5f;
        constexpr float START_Z = -(GRID_SIZE - 1) * SPACING * 0.5f;

        for (int ix = 0; ix < GRID_SIZE; ++ix) {
            for (int iz = 0; iz < GRID_SIZE; ++iz) {
                float metallic = static_cast<float>(ix) / static_cast<float>(GRID_SIZE - 1);
                float roughness = 0.1f + 0.9f * static_cast<float>(iz) / static_cast<float>(GRID_SIZE - 1);

                // Gold-ish albedo for metallic rows, neutral for dielectric
                Vec3 albedo = (metallic > 0.3f)
                    ? Vec3{1.0f, 0.86f, 0.57f}   // Gold
                    : Vec3{0.9f, 0.1f, 0.1f};     // Red dielectric

                render::MaterialData mat_data;
                mat_data.albedo = Vec4{albedo.x, albedo.y, albedo.z, 1.0f};
                mat_data.roughness = roughness;
                mat_data.metallic = metallic;
                auto mat = renderer->create_material(mat_data);
                m_materials.push_back(mat);

                float x = START_X + ix * SPACING;
                float z = START_Z + iz * SPACING;

                auto entity = world->create("PBRSphere_" + std::to_string(ix) + "_" + std::to_string(iz));
                auto& tf = world->emplace<LocalTransform>(entity, Vec3{x, 1.0f, z});
                tf.scale = Vec3{0.8f};
                world->emplace<WorldTransform>(entity);
                world->emplace<PreviousTransform>(entity);
                world->emplace<MeshRenderer>(entity, MeshRenderer{
                    MeshHandle{m_sphere_mesh.id}, MaterialHandle{mat.id}, 0, true, true, true
                });
                add_probe_sphere(Vec3{x, 1.0f, z}, 0.8f, mat_data);
            }
        }
    }

    // ---- Shadow Casters (tall cubes) ----
    void create_shadow_casters(World* world, render::IRenderer* renderer) {
        // Left cube — neutral gray
        render::MaterialData left_mat_data;
        left_mat_data.albedo = Vec4{0.3f, 0.3f, 0.35f, 1.0f};
        left_mat_data.roughness = 0.6f;
        left_mat_data.metallic = 0.0f;
        auto left_mat = renderer->create_material(left_mat_data);
        m_materials.push_back(left_mat);

        // Right cube — neutral gray (matching left cube / golden reference)
        render::MaterialData right_mat_data;
        right_mat_data.albedo = Vec4{0.3f, 0.3f, 0.35f, 1.0f};
        right_mat_data.roughness = 0.6f;
        right_mat_data.metallic = 0.0f;
        auto right_mat = renderer->create_material(right_mat_data);
        m_materials.push_back(right_mat);

        struct CubeInfo { Vec3 pos; render::MaterialHandle mat; };
        CubeInfo cubes[] = {
            {{-6.0f, 2.0f, 1.0f}, left_mat},
            {{6.0f, 2.0f, -2.0f}, right_mat}
        };
        for (int i = 0; i < 2; ++i) {
            auto entity = world->create("ShadowCube_" + std::to_string(i));
            auto& tf = world->emplace<LocalTransform>(entity, cubes[i].pos);
            tf.scale = Vec3{1.0f, 4.0f, 1.0f};
            world->emplace<WorldTransform>(entity);
            world->emplace<PreviousTransform>(entity);
            world->emplace<MeshRenderer>(entity, MeshRenderer{
                MeshHandle{m_cube_mesh.id}, MaterialHandle{cubes[i].mat.id}, 0, true, true, true
            });
            add_probe_box(cubes[i].pos, Vec3{1.0f, 4.0f, 1.0f},
                i == 0 ? left_mat_data : right_mat_data);
        }
    }

    // ---- Emissive Sphere (bloom test) ----
    void create_emissive_sphere(World* world, render::IRenderer* renderer) {
        render::MaterialData mat_data;
        mat_data.albedo = Vec4{1.0f, 0.3f, 0.1f, 1.0f};
        mat_data.roughness = 0.3f;
        mat_data.metallic = 0.0f;
        mat_data.emissive = Vec3{8.0f, 2.0f, 0.5f};
        auto mat = renderer->create_material(mat_data);
        m_materials.push_back(mat);

        auto entity = world->create("EmissiveSphere");
        auto& tf = world->emplace<LocalTransform>(entity, Vec3{6.0f, 1.5f, 2.0f});
        tf.scale = Vec3{1.0f};
        world->emplace<WorldTransform>(entity);
        world->emplace<PreviousTransform>(entity);
        world->emplace<MeshRenderer>(entity, MeshRenderer{
            MeshHandle{m_sphere_mesh.id}, MaterialHandle{mat.id}, 0, true, true, true
        });
        add_probe_sphere(Vec3{6.0f, 1.5f, 2.0f}, 1.0f, mat_data);
    }

    // ---- SSAO Corner (concave crevice) ----
    void create_ssao_corner(World* world, render::IRenderer* renderer) {
        render::MaterialData mat_data;
        mat_data.albedo = Vec4{0.7f, 0.7f, 0.72f, 1.0f};
        mat_data.roughness = 0.9f;
        mat_data.metallic = 0.0f;
        auto mat = renderer->create_material(mat_data);
        m_materials.push_back(mat);

        // Large cube
        {
            auto entity = world->create("SSAOCubeLarge");
            auto& tf = world->emplace<LocalTransform>(entity, Vec3{-6.0f, 1.5f, -3.0f});
            tf.scale = Vec3{3.0f};
            world->emplace<WorldTransform>(entity);
            world->emplace<PreviousTransform>(entity);
            world->emplace<MeshRenderer>(entity, MeshRenderer{
                MeshHandle{m_cube_mesh.id}, MaterialHandle{mat.id}, 0, true, true, true
            });
            add_probe_box(Vec3{-6.0f, 1.5f, -3.0f}, Vec3{3.0f, 3.0f, 3.0f}, mat_data);
        }
        // Small nested cube tucked in the corner
        {
            auto entity = world->create("SSAOCubeSmall");
            auto& tf = world->emplace<LocalTransform>(entity, Vec3{-4.8f, 0.4f, -1.8f});
            tf.scale = Vec3{0.8f};
            world->emplace<WorldTransform>(entity);
            world->emplace<PreviousTransform>(entity);
            world->emplace<MeshRenderer>(entity, MeshRenderer{
                MeshHandle{m_cube_mesh.id}, MaterialHandle{mat.id}, 0, true, true, true
            });
            add_probe_box(Vec3{-4.8f, 0.4f, -1.8f}, Vec3{0.8f, 0.8f, 0.8f}, mat_data);
        }
    }

    // ---- Glass Sphere (transparency test) ----
    void create_glass_sphere(World* world, render::IRenderer* renderer) {
        render::MaterialData mat_data;
        mat_data.albedo = Vec4{0.6f, 0.8f, 1.0f, 0.35f};
        mat_data.roughness = 0.1f;
        mat_data.metallic = 0.0f;
        mat_data.ior = 1.45f;
        mat_data.transmission = 0.65f;
        mat_data.transparent = true;
        mat_data.alpha_cutoff = 0.0f; // Disable alpha test to prevent discard
        auto mat = renderer->create_material(mat_data);
        m_materials.push_back(mat);

        auto entity = world->create("GlassSphere");
        auto& tf = world->emplace<LocalTransform>(entity, Vec3{3.0f, 1.2f, 5.0f});
        tf.scale = Vec3{1.2f};
        world->emplace<WorldTransform>(entity);
        world->emplace<PreviousTransform>(entity);
        world->emplace<MeshRenderer>(entity, MeshRenderer{
            MeshHandle{m_sphere_mesh.id}, MaterialHandle{mat.id}, 0, true, false, true, 2
        });
        add_probe_sphere(Vec3{3.0f, 1.2f, 5.0f}, 1.2f, mat_data);
    }

    // ---- Scene IBL (dark cubemaps matching golden environment) ----
    void create_scene_ibl(render::IRenderer* renderer) {
        // Golden scene has dark navy background (sRGB ~26,26,46).
        // Create 1x1 cubemaps with matching environment colors.
        // Face order: +X, -X, +Y, -Y, +Z, -Z. Pixel format: RGBA8.

        // Irradiance (diffuse ambient) — very dark, matching background
        {
            render::TextureData td;
            td.width = 1;
            td.height = 1;
            td.format = render::TextureFormat::RGBA8;
            td.is_cubemap = true;
            td.mip_levels = 1;
            // 6 faces × 1×1 × 4 bytes (RGBA)
            uint8_t faces[] = {
                24, 23, 35, 255,   // +X horizon (dark blue-gray)
                24, 23, 35, 255,   // -X
                26, 26, 46, 255,   // +Y sky (dark navy — matches golden background)
                20, 18, 15, 255,   // -Y ground (warm dark)
                24, 23, 35, 255,   // +Z
                24, 23, 35, 255,   // -Z
            };
            td.pixels.assign(faces, faces + sizeof(faces));
            m_ibl_irradiance = renderer->create_texture(td);
        }

        // Prefilter (specular reflections) — moderate brightness for metallic reflections
        {
            render::TextureData td;
            td.width = 1;
            td.height = 1;
            td.format = render::TextureFormat::RGBA8;
            td.is_cubemap = true;
            td.mip_levels = 1;
            uint8_t faces[] = {
                30, 28, 40, 255,   // +X horizon
                30, 28, 40, 255,   // -X
                35, 33, 50, 255,   // +Y sky
                25, 22, 18, 255,   // -Y ground
                30, 28, 40, 255,   // +Z
                30, 28, 40, 255,   // -Z
            };
            td.pixels.assign(faces, faces + sizeof(faces));
            m_ibl_prefilter = renderer->create_texture(td);
        }

        // Use the renderer's default BRDF LUT. The old 1x1 placeholder flattened
        // view/roughness-dependent specular response and muted the metallic row.
        renderer->set_ibl_textures(m_ibl_irradiance, m_ibl_prefilter, {}, 0);
    }

    render::MeshHandle m_sphere_mesh;
    render::MeshHandle m_cube_mesh;
    render::MeshHandle m_plane_mesh;
    render::TextureHandle m_ibl_irradiance;
    render::TextureHandle m_ibl_prefilter;
    render::TextureHandle m_ibl_brdf_lut;
    render::LightProbeVolumeHandle m_probe_volume = render::INVALID_PROBE_VOLUME;
    std::vector<render::MaterialHandle> m_materials;
    std::vector<ProbePrimitive> m_probe_primitives;
    std::vector<ProbeDirectionalLight> m_probe_lights;
};

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int) {
    // Parse command line for --screenshot support
    int argc = __argc;
    char** argv = __argv;
    RenderTestApp app;
    return app.run(argc, argv);
}
#else
int main(int argc, char** argv) {
    RenderTestApp app;
    return app.run(argc, argv);
}
#endif
