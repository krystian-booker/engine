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
#include <engine/core/math.hpp>
#include <engine/scene/scene.hpp>
#include <engine/render/renderer.hpp>
#include <engine/render/render_pipeline.hpp>
#include <engine/core/filesystem.hpp>

#include <cmath>
#include <cstring>

using namespace engine::core;
using namespace engine::scene;
using namespace engine::render;

// ============================================================================
// COLOR PIPELINE VALIDATION SAMPLE
// ============================================================================
//
// Validates the Linear vs sRGB texture pipeline by cycling through 6 debug
// visualization modes every 3 seconds.
//
// SCENE:
//   - Background quad with a 2x2 color chart texture (50% Grey, Red, Green, Blue)
//   - Sphere A (Dielectric): metallic=0.0, roughness=0.5, albedo=50% grey
//   - Sphere B (Metallic):   metallic=1.0, roughness=0.0, albedo=white
//   - Single white directional light, intensity=1.0
//
// DEBUG MODES (3 seconds each):
//   Mode 0: Raw Albedo (sRGB)       - texture data displayed as-is
//   Mode 1: Linear Albedo           - after pow(texel, 2.2); should look DARKER
//   Mode 2: Roughness Grayscale     - metallic_roughness.g as grayscale
//   Mode 3: Metalness Grayscale     - metallic_roughness.b as grayscale
//   Mode 4: Final Composite (No TM) - PBR lit, clamped at 1.0, gamma-corrected
//   Mode 5: Final Composite (ACES)  - PBR lit with ACES tonemapping
//
// ═══════════════════════════════════════════════════════════════════════════
// VALIDATION CHECKLIST
// ═══════════════════════════════════════════════════════════════════════════
//
// [1] The 0.5 Grey Check:
//     In Mode 1 (Linear Albedo), a 50% grey texture pixel (sRGB value 128/255)
//     should produce approximately 0.21 (= 0.5^2.2) in linear space.
//     If it reads as 0.5, sRGB-to-linear conversion is NOT happening in the shader.
//
// [2] The Black Reflection Check:
//     In Modes 4/5, Sphere B (100% Metal, White Albedo) should appear almost
//     completely BLACK in areas not reflecting the light source. Metals have zero
//     diffuse component; only specular highlights are visible.
//
// [3] The Normal/Roughness Raw Check:
//     In Mode 2 (Roughness), a "middle grey" roughness texture (green=128/255)
//     must produce a roughness value of 0.5, NOT 0.73 (= 0.5^(1/2.2)).
//     If it reads 0.73, the roughness map is incorrectly sampled as sRGB.
//
// TRAPS TO WATCH FOR:
//
// * "Double Gamma" Trap: If textures look washed out / milky, gamma correction
//   is applied TWICE (once via hardware sRGB flag + once via shader pow(2.2)).
//   Symptom: Mode 1 looks the SAME as or brighter than Mode 0.
//
// * Roughness/Metalness Distortion: If roughness maps are sampled as sRGB,
//   0.5 linear roughness becomes 0.73 in the shader. "Shiny" materials appear
//   dull and matte. The test MR texture encodes roughness=128 in the green
//   channel; Mode 2 must show exactly mid-grey, not bright grey.
//
// ============================================================================

class ColorPipelineSample : public Application {
protected:
    void on_init() override {
        log(LogLevel::Info, "Color Pipeline Validation Sample Initializing...");

        auto* renderer = get_renderer();
        auto* world = get_world();
        auto* pipeline = get_render_pipeline();

        if (!renderer || !world || !pipeline) return;

        // Configure pipeline for initial mode
        configure_pipeline_for_mode(pipeline, 0);

        // ── Create Meshes ──
        m_sphere_mesh = renderer->create_primitive(PrimitiveMesh::Sphere, 1.0f);
        m_quad_mesh = renderer->create_primitive(PrimitiveMesh::Quad, 10.0f);

        // ── Create Procedural Textures ──
        create_color_chart_texture(renderer);
        create_mr_test_texture(renderer);

        // ── Load Debug Shader ──
        // Pairs vs_pbr (for texcoord output) with fs_color_debug (for channel viz).
        // Falls back to standard PBR rendering if compiled binaries are unavailable.
        load_debug_shader(renderer);

        // ── Create Materials ──

        // Dielectric sphere: 50% grey, roughness 0.5, metallic 0.0
        MaterialData dielectric_mat;
        dielectric_mat.albedo = Vec4(0.5f, 0.5f, 0.5f, 1.0f);
        dielectric_mat.roughness = 0.5f;
        dielectric_mat.metallic = 0.0f;
        m_dielectric_material = renderer->create_material(dielectric_mat);

        // Metallic sphere: white albedo, roughness 0.0 (mirror), metallic 1.0
        MaterialData metallic_mat;
        metallic_mat.albedo = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
        metallic_mat.roughness = 0.001f; // Near-zero roughness (avoid exact 0 div issues)
        metallic_mat.metallic = 1.0f;
        m_metallic_material = renderer->create_material(metallic_mat);

        // Color chart quad: textured with procedural color chart + MR map
        MaterialData chart_mat;
        chart_mat.albedo = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
        chart_mat.roughness = 1.0f;
        chart_mat.metallic = 0.0f;
        chart_mat.albedo_map = m_color_chart_texture;
        chart_mat.metallic_roughness_map = m_mr_test_texture;
        m_chart_material = renderer->create_material(chart_mat);

        // Debug materials for modes 0-3 (use debug shader if available)
        if (m_debug_shader.valid()) {
            for (int mode = 0; mode < 4; ++mode) {
                MaterialData debug_mat;
                debug_mat.shader = m_debug_shader;
                debug_mat.albedo = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
                // Pack mode index into the 'ao' field → u_pbrParams.z in the shader.
                // The debug shader reads u_pbrParams.z to determine visualization mode.
                debug_mat.ao = static_cast<float>(mode);
                debug_mat.roughness = 1.0f;  // Unused by debug shader, but set for consistency
                debug_mat.metallic = 1.0f;   // Unused by debug shader
                debug_mat.albedo_map = m_color_chart_texture;
                debug_mat.metallic_roughness_map = m_mr_test_texture;

                m_debug_materials[mode] = renderer->create_material(debug_mat);
            }
            log(LogLevel::Info, "Debug materials created for modes 0-3.");
        } else {
            log(LogLevel::Warn,
                "Debug shader unavailable. Modes 0-3 will use standard PBR fallback.");
        }

        // ── Create Camera ──
        m_camera_entity = world->create("Camera");
        m_camera_pos = Vec3(0.0f, 1.0f, 5.0f);
        world->emplace<LocalTransform>(m_camera_entity, m_camera_pos);
        world->emplace<WorldTransform>(m_camera_entity);

        Camera cam;
        cam.fov = 60.0f;
        cam.aspect_ratio = static_cast<float>(window_width()) / static_cast<float>(window_height());
        cam.near_plane = 0.1f;
        cam.far_plane = 100.0f;
        cam.active = true;
        world->emplace<Camera>(m_camera_entity, cam);

        // ── Create Light ──
        // Single white directional light at intensity 1.0 (as specified in requirements)
        auto light_entity = world->create("DirectionalLight");
        Vec3 dir = glm::normalize(Vec3(-1.0f, -2.0f, -1.0f));
        Vec3 up = Vec3(0.0f, 1.0f, 0.0f);
        world->emplace<LocalTransform>(light_entity, Vec3(0.0f), glm::quatLookAt(dir, up));
        world->emplace<WorldTransform>(light_entity);

        Light light;
        light.type = LightType::Directional;
        light.color = Vec3(1.0f, 1.0f, 1.0f);
        light.intensity = 1.0f;
        light.cast_shadows = false;
        light.enabled = true;
        world->emplace<Light>(light_entity, light);

        // ── Sphere A: Dielectric (0.0 metallic, 0.5 roughness, 0.5 grey albedo) ──
        m_sphere_dielectric = world->create("SphereDielectric");
        world->emplace<LocalTransform>(m_sphere_dielectric, Vec3(-1.5f, 0.5f, 0.0f));
        world->emplace<WorldTransform>(m_sphere_dielectric);
        world->emplace<PreviousTransform>(m_sphere_dielectric);
        world->emplace<MeshRenderer>(m_sphere_dielectric, MeshRenderer{
            engine::scene::MeshHandle{m_sphere_mesh.id},
            engine::scene::MaterialHandle{m_dielectric_material.id},
            0, true, true, true
        });

        // ── Sphere B: Metallic (1.0 metallic, 0.0 roughness, 1.0 white albedo) ──
        m_sphere_metallic = world->create("SphereMetallic");
        world->emplace<LocalTransform>(m_sphere_metallic, Vec3(1.5f, 0.5f, 0.0f));
        world->emplace<WorldTransform>(m_sphere_metallic);
        world->emplace<PreviousTransform>(m_sphere_metallic);
        world->emplace<MeshRenderer>(m_sphere_metallic, MeshRenderer{
            engine::scene::MeshHandle{m_sphere_mesh.id},
            engine::scene::MaterialHandle{m_metallic_material.id},
            0, true, true, true
        });

        // ── Color Chart Quad ──
        // Placed behind spheres. The Quad primitive is in the XY plane with normal (0,0,1),
        // which faces toward the camera at z=5. No rotation needed.
        m_chart_quad = world->create("ColorChartQuad");
        world->emplace<LocalTransform>(m_chart_quad, Vec3(0.0f, 0.0f, -3.0f));
        world->emplace<WorldTransform>(m_chart_quad);
        world->emplace<PreviousTransform>(m_chart_quad);
        world->emplace<MeshRenderer>(m_chart_quad, MeshRenderer{
            engine::scene::MeshHandle{m_quad_mesh.id},
            engine::scene::MaterialHandle{m_chart_material.id},
            0, true, false, true
        });

        log(LogLevel::Info, "Color Pipeline Validation Sample Ready. Cycling modes every 3 seconds.");
    }

    void on_update(double dt) override {
        m_time += static_cast<float>(dt);

        auto* world = get_world();
        auto* pipeline = get_render_pipeline();
        auto* renderer = get_renderer();
        if (!world || !pipeline || !renderer) return;

        // Static camera looking at scene center
        auto& tf_cam = world->get<LocalTransform>(m_camera_entity);
        tf_cam.position = m_camera_pos;
        tf_cam.rotation = glm::quatLookAt(
            glm::normalize(Vec3(0.0f, 0.5f, 0.0f) - m_camera_pos),
            Vec3(0.0f, 1.0f, 0.0f));

        // ── Cycle Debug Mode Every 3 Seconds ──
        int mode = static_cast<int>(m_time / 3.0f) % 6;

        configure_pipeline_for_mode(pipeline, mode);

        // ── Swap Materials Based on Mode ──
        auto& mr_quad = world->get<MeshRenderer>(m_chart_quad);
        auto& mr_dielectric = world->get<MeshRenderer>(m_sphere_dielectric);
        auto& mr_metallic = world->get<MeshRenderer>(m_sphere_metallic);

        if (mode < 4 && m_debug_shader.valid()) {
            // Modes 0-3: Use debug shader on the color chart quad
            mr_quad.material = engine::scene::MaterialHandle{m_debug_materials[mode].id};
        } else {
            // Modes 4-5 (or fallback): Standard PBR material on quad
            mr_quad.material = engine::scene::MaterialHandle{m_chart_material.id};
        }

        // Spheres always use their standard PBR materials
        mr_dielectric.material = engine::scene::MaterialHandle{m_dielectric_material.id};
        mr_metallic.material = engine::scene::MaterialHandle{m_metallic_material.id};

        // ── Log Mode Transitions ──
        if (mode != m_last_mode) {
            static const char* mode_names[] = {
                "Raw Albedo (sRGB)",
                "Linear Albedo",
                "Roughness Map",
                "Metalness Map",
                "Final Composite (No Tonemapping)",
                "Final Composite (ACES Tonemapping)"
            };
            log(LogLevel::Info, "Mode {}: {}", mode, mode_names[mode]);
            m_last_mode = mode;
        }

        // ── Auto-Capture Screenshots ──
        static const char* screenshot_names[] = {
            "color_pipeline_mode0_raw_albedo.png",
            "color_pipeline_mode1_linear_albedo.png",
            "color_pipeline_mode2_roughness.png",
            "color_pipeline_mode3_metalness.png",
            "color_pipeline_mode4_no_tonemap.png",
            "color_pipeline_mode5_aces.png"
        };

        for (int i = 0; i < 6; ++i) {
            float capture_time = 1.0f + i * 3.0f;
            if (m_time > capture_time && !m_captured[i]) {
                m_captured[i] = true;
                renderer->save_screenshot(screenshot_names[i], pipeline->get_final_texture());
                log(LogLevel::Info, "Screenshot captured: {}", screenshot_names[i]);
            }
        }

        // Quit after all modes have been shown and captured
        if (m_time > 21.0f) {
            quit();
        }
    }

    void on_shutdown() override {
        if (auto* renderer = get_renderer()) {
            if (m_dielectric_material.valid()) renderer->destroy_material(m_dielectric_material);
            if (m_metallic_material.valid()) renderer->destroy_material(m_metallic_material);
            if (m_chart_material.valid()) renderer->destroy_material(m_chart_material);
            for (auto& mat : m_debug_materials) {
                if (mat.valid()) renderer->destroy_material(mat);
            }

            if (m_sphere_mesh.valid()) renderer->destroy_mesh(m_sphere_mesh);
            if (m_quad_mesh.valid()) renderer->destroy_mesh(m_quad_mesh);

            if (m_color_chart_texture.valid()) renderer->destroy_texture(m_color_chart_texture);
            if (m_mr_test_texture.valid()) renderer->destroy_texture(m_mr_test_texture);

            if (m_debug_shader.valid()) renderer->destroy_shader(m_debug_shader);
        }
    }

private:
    // ════════════════════════════════════════════════════════════════════════
    // Pipeline Configuration Per Mode
    // ════════════════════════════════════════════════════════════════════════

    void configure_pipeline_for_mode(RenderPipeline* pipeline, int mode) {
        RenderPipelineConfig config;
        config.quality = RenderQuality::High;
        config.enabled_passes = RenderPassFlags::DepthPrepass
                              | RenderPassFlags::MainOpaque
                              | RenderPassFlags::PostProcess
                              | RenderPassFlags::Final;
        config.show_debug_overlay = false;
        config.debug_view_mode = DebugViewMode::None;
        config.bloom_config.enabled = false;

        switch (mode) {
            case 0: // Raw Albedo (sRGB) — passthrough, no gamma correction
            case 1: // Linear Albedo     — passthrough, no gamma correction
            case 2: // Roughness         — passthrough, no gamma correction
            case 3: // Metalness         — passthrough, no gamma correction
                // These modes output debug values from the shader.
                // We bypass tonemapping and gamma so values pass through unchanged
                // to the sRGB display. This means:
                //   - Mode 0: sRGB value 0.5 → framebuffer 0.5 → display shows middle grey
                //   - Mode 1: linear 0.218   → framebuffer 0.218 → display shows dark grey
                config.tonemap_config.op = ToneMappingOperator::None;
                config.tonemap_config.gamma = 1.0f;
                config.tonemap_config.exposure = 1.0f;
                break;

            case 4: // Final Composite (No Tonemapping) — clamped to [0,1], gamma-encoded
                config.tonemap_config.op = ToneMappingOperator::None;
                config.tonemap_config.gamma = 2.2f;
                config.tonemap_config.exposure = 1.0f;
                break;

            case 5: // Final Composite (ACES Tonemapping) — full HDR-to-LDR pipeline
                config.tonemap_config.op = ToneMappingOperator::ACES;
                config.tonemap_config.gamma = 2.2f;
                config.tonemap_config.exposure = 1.0f;
                break;
        }

        pipeline->set_config(config);
    }

    // ════════════════════════════════════════════════════════════════════════
    // Procedural Texture Creation
    // ════════════════════════════════════════════════════════════════════════

    // Color Chart: 256x256 RGBA8, 2x2 grid of color patches (sRGB-encoded values)
    //
    //   ┌────────────┬────────────┐
    //   │  50% Grey  │  Pure Red  │
    //   │ (128,128,  │ (255,0,0)  │
    //   │  128)      │            │
    //   ├────────────┼────────────┤
    //   │ Pure Green │ Pure Blue  │
    //   │ (0,255,0)  │ (0,0,255)  │
    //   └────────────┴────────────┘
    //
    void create_color_chart_texture(IRenderer* renderer) {
        const uint32_t w = 256;
        const uint32_t h = 256;
        std::vector<uint8_t> pixels(w * h * 4);

        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                uint32_t idx = (y * w + x) * 4;
                bool right_half = (x >= w / 2);
                bool bottom_half = (y >= h / 2);

                uint8_t r, g, b;
                if (!right_half && !bottom_half) {
                    r = 128; g = 128; b = 128;  // 50% Grey (sRGB)
                } else if (right_half && !bottom_half) {
                    r = 255; g = 0; b = 0;      // Pure Red
                } else if (!right_half && bottom_half) {
                    r = 0; g = 255; b = 0;       // Pure Green
                } else {
                    r = 0; g = 0; b = 255;       // Pure Blue
                }

                pixels[idx + 0] = r;
                pixels[idx + 1] = g;
                pixels[idx + 2] = b;
                pixels[idx + 3] = 255;
            }
        }

        TextureData tex_data;
        tex_data.width = w;
        tex_data.height = h;
        tex_data.format = TextureFormat::RGBA8;
        tex_data.pixels = std::move(pixels);
        m_color_chart_texture = renderer->create_texture(tex_data);
    }

    // Metallic-Roughness Test Texture: 256x256 RGBA8 (glTF convention)
    //   Green channel = Roughness, Blue channel = Metallic
    //
    //   ┌──────────────────────────┐
    //   │   Roughness=0.5 (G=128) │  ← Top half: Metallic=0.0 (B=0)
    //   │   Metallic=0.0  (B=0)   │     i.e. dielectric at 50% roughness
    //   ├──────────────────────────┤
    //   │   Roughness=0.5 (G=128) │  ← Bottom half: Metallic=1.0 (B=255)
    //   │   Metallic=1.0  (B=255) │     i.e. metal at 50% roughness
    //   └──────────────────────────┘
    //
    // VALIDATION: In Mode 2, both halves should show identical mid-grey (roughness=0.5).
    //             If the top half appears brighter than expected, sRGB sampling is active.
    //
    void create_mr_test_texture(IRenderer* renderer) {
        const uint32_t w = 256;
        const uint32_t h = 256;
        std::vector<uint8_t> pixels(w * h * 4);

        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                uint32_t idx = (y * w + x) * 4;
                bool bottom_half = (y >= h / 2);

                pixels[idx + 0] = 0;                          // R: unused
                pixels[idx + 1] = 128;                        // G: roughness = 0.5
                pixels[idx + 2] = bottom_half ? 255 : 0;     // B: metallic (0 or 1)
                pixels[idx + 3] = 255;                        // A: unused
            }
        }

        TextureData tex_data;
        tex_data.width = w;
        tex_data.height = h;
        tex_data.format = TextureFormat::RGBA8;
        tex_data.pixels = std::move(pixels);
        m_mr_test_texture = renderer->create_texture(tex_data);
    }

    // ════════════════════════════════════════════════════════════════════════
    // Debug Shader Loading
    // ════════════════════════════════════════════════════════════════════════

    // Loads vs_pbr + fs_color_debug as a shader program pair.
    // Both share varying_pbr.def.sc, so the vertex shader provides v_texcoord0
    // for the debug fragment shader to sample textures.
    void load_debug_shader(IRenderer* renderer) {
        std::string shader_path = renderer->get_shader_path();

        ShaderData debug_shader_data;
        debug_shader_data.vertex_binary =
            FileSystem::read_binary(shader_path + "vs_pbr.sc.bin");
        debug_shader_data.fragment_binary =
            FileSystem::read_binary(shader_path + "fs_color_debug.sc.bin");

        if (!debug_shader_data.vertex_binary.empty()
            && !debug_shader_data.fragment_binary.empty())
        {
            m_debug_shader = renderer->create_shader(debug_shader_data);
            if (m_debug_shader.valid()) {
                log(LogLevel::Info, "Color debug shader loaded successfully.");
            } else {
                log(LogLevel::Warn,
                    "Failed to create color debug shader program. "
                    "Debug modes 0-3 will use standard PBR fallback.");
            }
        } else {
            log(LogLevel::Warn,
                "Color debug shader binaries not found (fs_color_debug.sc.bin). "
                "Rebuild with ENGINE_COMPILE_SHADERS=ON to compile. "
                "Debug modes 0-3 will use standard PBR materials.");
        }
    }

    // ════════════════════════════════════════════════════════════════════════
    // Member State
    // ════════════════════════════════════════════════════════════════════════

    float m_time = 0.0f;
    int m_last_mode = -1;
    Vec3 m_camera_pos;

    // Meshes
    engine::render::MeshHandle m_sphere_mesh;
    engine::render::MeshHandle m_quad_mesh;

    // Textures
    engine::render::TextureHandle m_color_chart_texture;
    engine::render::TextureHandle m_mr_test_texture;

    // Materials
    engine::render::MaterialHandle m_dielectric_material;
    engine::render::MaterialHandle m_metallic_material;
    engine::render::MaterialHandle m_chart_material;
    engine::render::MaterialHandle m_debug_materials[4]; // Modes 0-3

    // Shader
    engine::render::ShaderHandle m_debug_shader;

    // Entities
    Entity m_camera_entity = NullEntity;
    Entity m_sphere_dielectric = NullEntity;
    Entity m_sphere_metallic = NullEntity;
    Entity m_chart_quad = NullEntity;

    // Screenshot capture flags
    bool m_captured[6] = {};
};

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int) {
    int argc = __argc;
    char** argv = __argv;
    ColorPipelineSample app;
    return app.run(argc, argv);
}
#else
int main(int argc, char** argv) {
    ColorPipelineSample app;
    return app.run(argc, argv);
}
#endif
