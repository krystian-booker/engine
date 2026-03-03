#include <engine/core/application.hpp>
#include <engine/core/log.hpp>
#include <engine/core/math.hpp>
#include <engine/scene/scene.hpp>
#include <engine/scene/transform.hpp>
#include <engine/render/renderer.hpp>
#include <engine/render/render_pipeline.hpp>
#include <engine/core/filesystem.hpp>

#include <stb_image.h>

#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>

using namespace engine::core;
using namespace engine::scene;
using namespace engine::render;

// ---------------------------------------------------------------------------
// Float-to-half conversion (IEEE 754)
// ---------------------------------------------------------------------------
static uint16_t float_to_half(float f) {
    uint32_t bits;
    std::memcpy(&bits, &f, 4);
    uint32_t sign = (bits >> 16) & 0x8000;
    int32_t  exponent = ((bits >> 23) & 0xFF) - 127;
    uint32_t mantissa = bits & 0x7FFFFF;

    if (exponent > 15) {
        return static_cast<uint16_t>(sign | 0x7C00); // Inf
    }
    if (exponent < -14) {
        // Denormalized or zero
        if (exponent < -24) return static_cast<uint16_t>(sign);
        mantissa |= 0x800000;
        uint32_t shift = static_cast<uint32_t>(-1 - exponent);
        mantissa >>= shift;
        return static_cast<uint16_t>(sign | (mantissa >> 13));
    }
    return static_cast<uint16_t>(sign | ((exponent + 15) << 10) | (mantissa >> 13));
}

// ---------------------------------------------------------------------------
// Cubemap face direction helpers
// ---------------------------------------------------------------------------
// Face order: +X, -X, +Y, -Y, +Z, -Z
static Vec3 cubemap_direction(int face, float u, float v) {
    // u, v in [-1, 1]
    switch (face) {
        case 0: return glm::normalize(Vec3( 1.0f,    -v,   -u)); // +X
        case 1: return glm::normalize(Vec3(-1.0f,    -v,    u)); // -X
        case 2: return glm::normalize(Vec3(   u,  1.0f,     v)); // +Y
        case 3: return glm::normalize(Vec3(   u, -1.0f,    -v)); // -Y
        case 4: return glm::normalize(Vec3(   u,    -v,  1.0f)); // +Z
        case 5: return glm::normalize(Vec3(  -u,    -v, -1.0f)); // -Z
        default: return Vec3(0.0f, 0.0f, 1.0f);
    }
}

// ---------------------------------------------------------------------------
// Sample equirectangular HDR image given a 3D direction
// ---------------------------------------------------------------------------
static Vec3 sample_equirect(const float* hdr_data, int w, int h, const Vec3& dir) {
    float theta = std::atan2(dir.z, dir.x);           // [-PI, PI]
    float phi   = std::asin(glm::clamp(dir.y, -1.0f, 1.0f)); // [-PI/2, PI/2]

    float u = (theta / glm::pi<float>()) * 0.5f + 0.5f; // [0, 1]
    float v = 0.5f - (phi / glm::pi<float>());           // [0, 1]

    // Bilinear sample
    float fx = u * static_cast<float>(w) - 0.5f;
    float fy = v * static_cast<float>(h) - 0.5f;
    int x0 = static_cast<int>(std::floor(fx));
    int y0 = static_cast<int>(std::floor(fy));
    float sx = fx - static_cast<float>(x0);
    float sy = fy - static_cast<float>(y0);

    auto sample_pixel = [&](int x, int y) -> Vec3 {
        x = ((x % w) + w) % w;
        y = glm::clamp(y, 0, h - 1);
        int idx = (y * w + x) * 4; // RGBA
        return Vec3(hdr_data[idx], hdr_data[idx + 1], hdr_data[idx + 2]);
    };

    Vec3 c00 = sample_pixel(x0, y0);
    Vec3 c10 = sample_pixel(x0 + 1, y0);
    Vec3 c01 = sample_pixel(x0, y0 + 1);
    Vec3 c11 = sample_pixel(x0 + 1, y0 + 1);

    return glm::mix(glm::mix(c00, c10, sx), glm::mix(c01, c11, sx), sy);
}

// ---------------------------------------------------------------------------
// Sample cubemap from face data
// ---------------------------------------------------------------------------
static Vec3 sample_cubemap(const std::vector<std::vector<Vec3>>& faces, int face_size, const Vec3& dir) {
    // Determine face and UV
    float abs_x = std::abs(dir.x), abs_y = std::abs(dir.y), abs_z = std::abs(dir.z);
    int face;
    float u, v, ma;

    if (abs_x >= abs_y && abs_x >= abs_z) {
        ma = abs_x;
        if (dir.x > 0) { face = 0; u = -dir.z; v = -dir.y; }
        else            { face = 1; u =  dir.z; v = -dir.y; }
    } else if (abs_y >= abs_x && abs_y >= abs_z) {
        ma = abs_y;
        if (dir.y > 0) { face = 2; u = dir.x;  v =  dir.z; }
        else            { face = 3; u = dir.x;  v = -dir.z; }
    } else {
        ma = abs_z;
        if (dir.z > 0) { face = 4; u =  dir.x; v = -dir.y; }
        else            { face = 5; u = -dir.x; v = -dir.y; }
    }

    // Map to [0, 1]
    float s = 0.5f * (u / ma + 1.0f);
    float t = 0.5f * (v / ma + 1.0f);

    // Sample
    float fx = s * static_cast<float>(face_size) - 0.5f;
    float fy = t * static_cast<float>(face_size) - 0.5f;
    int x0 = glm::clamp(static_cast<int>(std::floor(fx)), 0, face_size - 1);
    int y0 = glm::clamp(static_cast<int>(std::floor(fy)), 0, face_size - 1);
    int x1 = glm::clamp(x0 + 1, 0, face_size - 1);
    int y1 = glm::clamp(y0 + 1, 0, face_size - 1);
    float sx = fx - std::floor(fx);
    float sy = fy - std::floor(fy);

    const auto& fd = faces[face];
    Vec3 c00 = fd[y0 * face_size + x0];
    Vec3 c10 = fd[y0 * face_size + x1];
    Vec3 c01 = fd[y1 * face_size + x0];
    Vec3 c11 = fd[y1 * face_size + x1];

    return glm::mix(glm::mix(c00, c10, sx), glm::mix(c01, c11, sx), sy);
}

// ---------------------------------------------------------------------------
// Hammersley sequence for importance sampling
// ---------------------------------------------------------------------------
static float radical_inverse_vdc(uint32_t bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return static_cast<float>(bits) * 2.3283064365386963e-10f;
}

static Vec2 hammersley(uint32_t i, uint32_t N) {
    return Vec2(static_cast<float>(i) / static_cast<float>(N), radical_inverse_vdc(i));
}

// ---------------------------------------------------------------------------
// GGX importance sampling
// ---------------------------------------------------------------------------
static Vec3 importance_sample_ggx(Vec2 xi, Vec3 N, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0f * glm::pi<float>() * xi.x;
    float cos_theta = std::sqrt((1.0f - xi.y) / (1.0f + (a * a - 1.0f) * xi.y));
    float sin_theta = std::sqrt(1.0f - cos_theta * cos_theta);

    Vec3 H(std::cos(phi) * sin_theta, std::sin(phi) * sin_theta, cos_theta);

    // Tangent space to world
    Vec3 up = (std::abs(N.z) < 0.999f) ? Vec3(0.0f, 0.0f, 1.0f) : Vec3(1.0f, 0.0f, 0.0f);
    Vec3 tangent = glm::normalize(glm::cross(up, N));
    Vec3 bitangent = glm::cross(N, tangent);

    return glm::normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

// ---------------------------------------------------------------------------
// Write half-float pixel to buffer
// ---------------------------------------------------------------------------
static void write_half4(std::vector<uint8_t>& buf, size_t offset, const Vec3& color, float alpha = 1.0f) {
    uint16_t h[4] = {
        float_to_half(color.x),
        float_to_half(color.y),
        float_to_half(color.z),
        float_to_half(alpha)
    };
    std::memcpy(buf.data() + offset, h, 8);
}

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr int NUM_MODES = 4;
static constexpr float MODE_CYCLE_SEC = 4.0f;

static const char* MODE_NAMES[] = {
    "Full IBL + Analytical",
    "IBL Specular Only",
    "IBL Diffuse Only",
    "Metal Mirror Test"
};

static constexpr int CUBEMAP_SIZE = 256;
static constexpr int IRRADIANCE_SIZE = 32;
static constexpr int PREFILTER_SIZE = 128;
// bgfx requires the FULL mip chain when hasMips=true: 1 + log2(128) = 8
static constexpr int PREFILTER_MIP_LEVELS = 8; // 128 -> 64 -> 32 -> 16 -> 8 -> 4 -> 2 -> 1
static constexpr int BRDF_LUT_SIZE = 256;
static constexpr uint32_t IBL_SAMPLE_COUNT = 256;
static constexpr uint32_t IRRADIANCE_SAMPLE_COUNT = 512;

// ---------------------------------------------------------------------------
// Application
// ---------------------------------------------------------------------------
class IBLValidationSample : public Application {
protected:
    void on_init() override {
        log(LogLevel::Info, "IBL Validation Initializing...");

        auto* renderer = get_renderer();
        auto* world = get_world();
        auto* pipeline = get_render_pipeline();
        if (!renderer || !world || !pipeline) return;

        // --- Pipeline config ---
        RenderPipelineConfig config;
        config.quality = RenderQuality::High;
        config.enabled_passes = RenderPassFlags::DepthPrepass
            | RenderPassFlags::Skybox
            | RenderPassFlags::MainOpaque
            | RenderPassFlags::PostProcess
            | RenderPassFlags::Final;
        config.show_debug_overlay = false;
        config.debug_view_mode = DebugViewMode::None;
        config.bloom_config.enabled = false;
        config.tonemap_config.op = ToneMappingOperator::AgX;
        config.tonemap_config.exposure = 1.0f;
        config.tonemap_config.gamma = 2.2f;
        pipeline->set_config(config);

        // --- Load HDR environment ---
        log(LogLevel::Info, "Loading HDR environment map...");
        int hdr_w, hdr_h, hdr_c;
        stbi_set_flip_vertically_on_load(0);
        float* hdr_data = stbi_loadf("pisa.hdr", &hdr_w, &hdr_h, &hdr_c, 4);
        if (!hdr_data) {
            log(LogLevel::Error, "Failed to load pisa.hdr");
            return;
        }
        log(LogLevel::Info, "HDR loaded: {}x{}", hdr_w, hdr_h);

        // --- Step 1: Equirectangular to Cubemap ---
        log(LogLevel::Info, "Converting equirect to cubemap ({}x{})...", CUBEMAP_SIZE, CUBEMAP_SIZE);
        std::vector<std::vector<Vec3>> env_faces(6);
        {
            const size_t face_pixels = CUBEMAP_SIZE * CUBEMAP_SIZE;
            std::vector<uint8_t> cubemap_data(6 * face_pixels * 8); // RGBA16F

            for (int face = 0; face < 6; ++face) {
                env_faces[face].resize(face_pixels);
                for (int y = 0; y < CUBEMAP_SIZE; ++y) {
                    for (int x = 0; x < CUBEMAP_SIZE; ++x) {
                        float u = (static_cast<float>(x) + 0.5f) / CUBEMAP_SIZE * 2.0f - 1.0f;
                        float v = (static_cast<float>(y) + 0.5f) / CUBEMAP_SIZE * 2.0f - 1.0f;
                        Vec3 dir = cubemap_direction(face, u, v);
                        Vec3 color = sample_equirect(hdr_data, hdr_w, hdr_h, dir);

                        env_faces[face][y * CUBEMAP_SIZE + x] = color;
                        size_t offset = (face * face_pixels + y * CUBEMAP_SIZE + x) * 8;
                        write_half4(cubemap_data, offset, color);
                    }
                }
            }

            TextureData tex_data;
            tex_data.width = CUBEMAP_SIZE;
            tex_data.height = CUBEMAP_SIZE;
            tex_data.format = TextureFormat::RGBA16F;
            tex_data.is_cubemap = true;
            tex_data.mip_levels = 1;
            tex_data.pixels = std::move(cubemap_data);
            m_skybox_cubemap = renderer->create_texture(tex_data);
        }

        // --- Step 2: Irradiance Convolution ---
        log(LogLevel::Info, "Computing irradiance map ({}x{})...", IRRADIANCE_SIZE, IRRADIANCE_SIZE);
        {
            const size_t face_pixels = IRRADIANCE_SIZE * IRRADIANCE_SIZE;
            std::vector<uint8_t> irr_data(6 * face_pixels * 8);

            for (int face = 0; face < 6; ++face) {
                for (int y = 0; y < IRRADIANCE_SIZE; ++y) {
                    for (int x = 0; x < IRRADIANCE_SIZE; ++x) {
                        float u = (static_cast<float>(x) + 0.5f) / IRRADIANCE_SIZE * 2.0f - 1.0f;
                        float v = (static_cast<float>(y) + 0.5f) / IRRADIANCE_SIZE * 2.0f - 1.0f;
                        Vec3 N = cubemap_direction(face, u, v);

                        // Hemisphere sampling for irradiance
                        Vec3 up = (std::abs(N.z) < 0.999f) ? Vec3(0.0f, 0.0f, 1.0f) : Vec3(1.0f, 0.0f, 0.0f);
                        Vec3 right = glm::normalize(glm::cross(up, N));
                        up = glm::cross(N, right);

                        Vec3 irradiance(0.0f);
                        for (uint32_t s = 0; s < IRRADIANCE_SAMPLE_COUNT; ++s) {
                            Vec2 xi = hammersley(s, IRRADIANCE_SAMPLE_COUNT);
                            float cos_theta = xi.x;
                            float sin_theta = std::sqrt(1.0f - cos_theta * cos_theta);
                            float phi = 2.0f * glm::pi<float>() * xi.y;

                            Vec3 sample_dir = right * (std::cos(phi) * sin_theta)
                                            + up * (std::sin(phi) * sin_theta)
                                            + N * cos_theta;

                            Vec3 env_color = sample_cubemap(env_faces, CUBEMAP_SIZE, sample_dir);
                            irradiance += env_color * cos_theta * sin_theta;
                        }
                        irradiance *= glm::pi<float>() / static_cast<float>(IRRADIANCE_SAMPLE_COUNT);

                        size_t offset = (face * face_pixels + y * IRRADIANCE_SIZE + x) * 8;
                        write_half4(irr_data, offset, irradiance);
                    }
                }
            }

            TextureData tex_data;
            tex_data.width = IRRADIANCE_SIZE;
            tex_data.height = IRRADIANCE_SIZE;
            tex_data.format = TextureFormat::RGBA16F;
            tex_data.is_cubemap = true;
            tex_data.mip_levels = 1;
            tex_data.pixels = std::move(irr_data);
            m_irradiance_map = renderer->create_texture(tex_data);
        }

        // --- Step 3: GGX Prefiltered Specular Map ---
        log(LogLevel::Info, "Computing prefiltered specular map ({}x{}, {} mips)...",
            PREFILTER_SIZE, PREFILTER_SIZE, PREFILTER_MIP_LEVELS);
        {
            // Calculate total buffer size for all mip levels
            size_t total_size = 0;
            for (int mip = 0; mip < PREFILTER_MIP_LEVELS; ++mip) {
                int mip_size = PREFILTER_SIZE >> mip;
                total_size += 6 * mip_size * mip_size * 8;
            }
            std::vector<uint8_t> prefilter_data(total_size);

            size_t data_offset = 0;
            for (int mip = 0; mip < PREFILTER_MIP_LEVELS; ++mip) {
                int mip_size = PREFILTER_SIZE >> mip;
                float roughness = static_cast<float>(mip) / static_cast<float>(PREFILTER_MIP_LEVELS - 1);

                for (int face = 0; face < 6; ++face) {
                    for (int y = 0; y < mip_size; ++y) {
                        for (int x = 0; x < mip_size; ++x) {
                            float u = (static_cast<float>(x) + 0.5f) / mip_size * 2.0f - 1.0f;
                            float v = (static_cast<float>(y) + 0.5f) / mip_size * 2.0f - 1.0f;
                            Vec3 N = cubemap_direction(face, u, v);
                            Vec3 R = N;
                            Vec3 V = R;

                            Vec3 prefiltered(0.0f);
                            float total_weight = 0.0f;

                            for (uint32_t s = 0; s < IBL_SAMPLE_COUNT; ++s) {
                                Vec2 xi = hammersley(s, IBL_SAMPLE_COUNT);
                                Vec3 H = importance_sample_ggx(xi, N, roughness);
                                Vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);

                                float NdotL = glm::dot(N, L);
                                if (NdotL > 0.0f) {
                                    Vec3 env_color = sample_cubemap(env_faces, CUBEMAP_SIZE, L);
                                    prefiltered += env_color * NdotL;
                                    total_weight += NdotL;
                                }
                            }
                            if (total_weight > 0.0f)
                                prefiltered /= total_weight;

                            write_half4(prefilter_data, data_offset, prefiltered);
                            data_offset += 8;
                        }
                    }
                }
            }

            TextureData tex_data;
            tex_data.width = PREFILTER_SIZE;
            tex_data.height = PREFILTER_SIZE;
            tex_data.format = TextureFormat::RGBA16F;
            tex_data.is_cubemap = true;
            tex_data.mip_levels = PREFILTER_MIP_LEVELS;
            tex_data.pixels = std::move(prefilter_data);
            m_prefilter_map = renderer->create_texture(tex_data);
        }

        // --- Step 4: BRDF Integration LUT ---
        log(LogLevel::Info, "Computing BRDF LUT ({}x{})...", BRDF_LUT_SIZE, BRDF_LUT_SIZE);
        {
            std::vector<uint8_t> brdf_data(BRDF_LUT_SIZE * BRDF_LUT_SIZE * 8);

            for (int y = 0; y < BRDF_LUT_SIZE; ++y) {
                for (int x = 0; x < BRDF_LUT_SIZE; ++x) {
                    float NdotV = (static_cast<float>(x) + 0.5f) / BRDF_LUT_SIZE;
                    float roughness = (static_cast<float>(y) + 0.5f) / BRDF_LUT_SIZE;
                    NdotV = std::max(NdotV, 0.001f);

                    Vec3 V(std::sqrt(1.0f - NdotV * NdotV), 0.0f, NdotV);
                    Vec3 N(0.0f, 0.0f, 1.0f);

                    float A = 0.0f, B = 0.0f;
                    for (uint32_t s = 0; s < IBL_SAMPLE_COUNT; ++s) {
                        Vec2 xi = hammersley(s, IBL_SAMPLE_COUNT);
                        Vec3 H = importance_sample_ggx(xi, N, roughness);
                        Vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);

                        float NdotL = std::max(L.z, 0.0f);
                        float NdotH = std::max(H.z, 0.0f);
                        float VdotH = std::max(glm::dot(V, H), 0.0f);

                        if (NdotL > 0.0f) {
                            float a = roughness * roughness;
                            float k = a / 2.0f; // IBL geometry term
                            auto G_schlick = [](float NdotX, float k_) -> float {
                                return NdotX / (NdotX * (1.0f - k_) + k_);
                            };
                            float G = G_schlick(NdotV, k) * G_schlick(NdotL, k);
                            float G_vis = (G * VdotH) / (NdotH * NdotV);
                            float Fc = std::pow(1.0f - VdotH, 5.0f);

                            A += (1.0f - Fc) * G_vis;
                            B += Fc * G_vis;
                        }
                    }
                    A /= static_cast<float>(IBL_SAMPLE_COUNT);
                    B /= static_cast<float>(IBL_SAMPLE_COUNT);

                    size_t offset = (y * BRDF_LUT_SIZE + x) * 8;
                    write_half4(brdf_data, offset, Vec3(A, B, 0.0f), 1.0f);
                }
            }

            TextureData tex_data;
            tex_data.width = BRDF_LUT_SIZE;
            tex_data.height = BRDF_LUT_SIZE;
            tex_data.format = TextureFormat::RGBA16F;
            tex_data.is_cubemap = false;
            tex_data.mip_levels = 1;
            tex_data.pixels = std::move(brdf_data);
            m_brdf_lut = renderer->create_texture(tex_data);
        }

        stbi_image_free(hdr_data);
        log(LogLevel::Info, "IBL textures generated successfully.");

        // --- Set IBL textures on renderer ---
        renderer->set_ibl_textures(m_irradiance_map, m_prefilter_map, m_brdf_lut,
                                    PREFILTER_MIP_LEVELS - 1);
        renderer->set_ibl_intensity(1.0f);

        // --- Set skybox ---
        pipeline->set_skybox(m_skybox_cubemap, 1.0f, 0.0f);

        // --- Load shader ---
        std::string shader_path = renderer->get_shader_path();
        ShaderData shader_data;
        shader_data.vertex_binary = FileSystem::read_binary(shader_path + "vs_pbr.sc.bin");
        shader_data.fragment_binary = FileSystem::read_binary(shader_path + "fs_ibl_validation.sc.bin");
        m_shader = renderer->create_shader(shader_data);

        // --- Create sphere mesh ---
        m_sphere_mesh = renderer->create_primitive(PrimitiveMesh::Sphere, 1.0f);

        // --- Create materials for all modes and spheres ---
        for (int m = 0; m < NUM_MODES; ++m) {
            for (int row = 0; row < 2; ++row) {
                for (int col = 0; col < 5; ++col) {
                    int index = row * 5 + col;

                    float metallic, roughness;
                    float shader_mode;

                    if (m == 3) {
                        // Mode 3: all chrome mirrors
                        metallic = 1.0f;
                        roughness = 0.05f;
                        shader_mode = 0.0f; // Full IBL+Analytical
                    } else {
                        metallic = (row == 0) ? 0.0f : 1.0f;
                        roughness = glm::mix(0.05f, 1.0f, static_cast<float>(col) / 4.0f);
                        shader_mode = static_cast<float>(m);
                    }

                    MaterialData mat;
                    mat.shader = m_shader;
                    mat.albedo = Vec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
                    mat.metallic = metallic;
                    mat.roughness = roughness;
                    mat.ao = 1.0f;
                    mat.alpha_cutoff = shader_mode; // Packed into u_pbrParams.w

                    m_materials[m][index] = renderer->create_material(mat);
                }
            }
        }

        // --- Spawn spheres ---
        for (int row = 0; row < 2; ++row) {
            for (int col = 0; col < 5; ++col) {
                int index = row * 5 + col;
                Entity ent = world->create("Sphere_R" + std::to_string(row) + "_C" + std::to_string(col));

                float x = (col - 2.0f) * 2.5f;
                float y = (row - 0.5f) * -2.5f;

                world->registry().emplace<LocalTransform>(ent, Vec3(x, y, 0.0f));
                world->registry().emplace<WorldTransform>(ent);
                world->registry().emplace<PreviousTransform>(ent);
                world->registry().emplace<MeshRenderer>(ent, MeshRenderer{
                    engine::scene::MeshHandle{m_sphere_mesh.id},
                    engine::scene::MaterialHandle{m_materials[0][index].id},
                    0, true, false, false
                });
                m_spheres[index] = ent;
            }
        }

        // --- Camera ---
        m_camera = world->create("Camera");
        world->registry().emplace<LocalTransform>(m_camera, Vec3(0.0f, 0.0f, 12.0f),
            glm::quatLookAt(glm::normalize(Vec3(0.0f) - Vec3(0.0f, 0.0f, 12.0f)), Vec3(0.0f, 1.0f, 0.0f)));
        world->registry().emplace<WorldTransform>(m_camera);

        Camera cam_comp;
        cam_comp.fov = 60.0f;
        cam_comp.aspect_ratio = static_cast<float>(window_width()) / static_cast<float>(window_height());
        cam_comp.near_plane = 0.1f;
        cam_comp.far_plane = 100.0f;
        cam_comp.active = true;
        world->registry().emplace<Camera>(m_camera, cam_comp);

        // --- Directional light ---
        m_light = world->create("Directional Light");
        Light l;
        l.type = LightType::Directional;
        l.color = Vec3(1.0f);
        l.intensity = 3.0f;
        l.cast_shadows = false;
        l.enabled = true;
        world->registry().emplace<Light>(m_light, l);

        world->registry().emplace<LocalTransform>(m_light, Vec3(0.0f),
            glm::quatLookAt(glm::normalize(Vec3(-1.0f, -0.5f, -1.0f)), Vec3(0.0f, 1.0f, 0.0f)));
        world->registry().emplace<WorldTransform>(m_light);

        log(LogLevel::Info, "IBL Validation ready. Modes cycle every %.1f s.", MODE_CYCLE_SEC);
    }

    void on_update(double dt) override {
        auto* active_world = get_world();
        if (active_world) {
            engine::scene::transform_system(*active_world, dt);
        }

        m_time += static_cast<float>(dt);

        // Rotate light
        auto* world = get_world();
        if (world) {
            Vec3 dir(-sinf(m_time), -0.5f, -cosf(m_time));
            dir = glm::normalize(dir);
            auto& lt = world->registry().get<LocalTransform>(m_light);
            lt.rotation = glm::quatLookAt(dir, Vec3(0.0f, 1.0f, 0.0f));
        }

        // Cycle modes
        int current_mode = (static_cast<int>(m_time / MODE_CYCLE_SEC)) % NUM_MODES;
        if (current_mode != m_last_mode) {
            m_last_mode = current_mode;
            log(LogLevel::Info, "[IBL Validation] Mode {}: {}", current_mode, MODE_NAMES[current_mode]);

            if (world) {
                for (int i = 0; i < 10; ++i) {
                    auto* comp = world->registry().try_get<MeshRenderer>(m_spheres[i]);
                    if (comp) {
                        comp->material = engine::scene::MaterialHandle{ m_materials[current_mode][i].id };
                    }
                }
            }
        }
    }

    void on_shutdown() override {
        auto* renderer = get_renderer();
        if (!renderer) return;

        for (int m = 0; m < NUM_MODES; ++m) {
            for (int i = 0; i < 10; ++i) {
                if (m_materials[m][i].valid()) renderer->destroy_material(m_materials[m][i]);
            }
        }
        if (m_sphere_mesh.valid()) renderer->destroy_mesh(m_sphere_mesh);
        if (m_skybox_cubemap.valid()) renderer->destroy_texture(m_skybox_cubemap);
        if (m_irradiance_map.valid()) renderer->destroy_texture(m_irradiance_map);
        if (m_prefilter_map.valid()) renderer->destroy_texture(m_prefilter_map);
        if (m_brdf_lut.valid()) renderer->destroy_texture(m_brdf_lut);
    }

private:
    float m_time = 0.0f;
    int   m_last_mode = -1;

    // Render resources
    engine::render::MeshHandle    m_sphere_mesh;
    engine::render::ShaderHandle  m_shader;
    engine::render::MaterialHandle m_materials[NUM_MODES][10];

    // IBL textures
    engine::render::TextureHandle m_skybox_cubemap;
    engine::render::TextureHandle m_irradiance_map;
    engine::render::TextureHandle m_prefilter_map;
    engine::render::TextureHandle m_brdf_lut;

    // Scene entities
    Entity m_spheres[10];
    Entity m_camera = NullEntity;
    Entity m_light = NullEntity;
};

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    IBLValidationSample app;
    return app.run(__argc, __argv);
}
#else
int main(int argc, char** argv) {
    IBLValidationSample app;
    return app.run(argc, argv);
}
#endif
