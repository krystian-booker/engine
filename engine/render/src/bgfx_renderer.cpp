#include <engine/render/renderer.hpp>
#include <engine/render/pbr_material.hpp>
#include <engine/render/debug_draw.hpp>
#include <engine/core/log.hpp>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <algorithm>
#include <unordered_map>
#include <cmath>
#include <fstream>
#include <array>
#include <chrono>

namespace engine::render {

using namespace engine::core;

// Vertex layout for our Vertex struct
static bgfx::VertexLayout s_vertex_layout;

// PBR Uniform handles (created once, reused)
struct PBRUniforms {
    bgfx::UniformHandle u_cameraPos = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_albedoColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_pbrParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_emissiveColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_lights = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_lightCount = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_iblParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_time = BGFX_INVALID_HANDLE;

    // Shadow uniforms
    bgfx::UniformHandle u_shadowParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_cascadeSplits = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadowMatrix0 = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadowMatrix1 = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadowMatrix2 = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadowMatrix3 = BGFX_INVALID_HANDLE;

    // Texture samplers
    bgfx::UniformHandle s_albedo = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_normal = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_metallicRoughness = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_ao = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_emissive = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_irradiance = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_prefilter = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_brdfLUT = BGFX_INVALID_HANDLE;

    // Shadow map samplers
    bgfx::UniformHandle s_shadowMap0 = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_shadowMap1 = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_shadowMap2 = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_shadowMap3 = BGFX_INVALID_HANDLE;

    // Blit sampler (for blit_to_screen)
    bgfx::UniformHandle s_blit_texture = BGFX_INVALID_HANDLE;

    void create() {
        u_cameraPos = bgfx::createUniform("u_cameraPos", bgfx::UniformType::Vec4);
        u_albedoColor = bgfx::createUniform("u_albedoColor", bgfx::UniformType::Vec4);
        u_pbrParams = bgfx::createUniform("u_pbrParams", bgfx::UniformType::Vec4);
        u_emissiveColor = bgfx::createUniform("u_emissiveColor", bgfx::UniformType::Vec4);
        u_lights = bgfx::createUniform("u_lights", bgfx::UniformType::Vec4, 32);  // 8 lights * 4 vec4s
        u_lightCount = bgfx::createUniform("u_lightCount", bgfx::UniformType::Vec4);
        u_iblParams = bgfx::createUniform("u_iblParams", bgfx::UniformType::Vec4);
        u_time = bgfx::createUniform("u_time", bgfx::UniformType::Vec4);

        // Shadow uniforms
        u_shadowParams = bgfx::createUniform("u_shadowParams", bgfx::UniformType::Vec4);
        u_cascadeSplits = bgfx::createUniform("u_cascadeSplits", bgfx::UniformType::Vec4);
        u_shadowMatrix0 = bgfx::createUniform("u_shadowMatrix0", bgfx::UniformType::Mat4);
        u_shadowMatrix1 = bgfx::createUniform("u_shadowMatrix1", bgfx::UniformType::Mat4);
        u_shadowMatrix2 = bgfx::createUniform("u_shadowMatrix2", bgfx::UniformType::Mat4);
        u_shadowMatrix3 = bgfx::createUniform("u_shadowMatrix3", bgfx::UniformType::Mat4);

        s_albedo = bgfx::createUniform("s_albedo", bgfx::UniformType::Sampler);
        s_normal = bgfx::createUniform("s_normal", bgfx::UniformType::Sampler);
        s_metallicRoughness = bgfx::createUniform("s_metallicRoughness", bgfx::UniformType::Sampler);
        s_ao = bgfx::createUniform("s_ao", bgfx::UniformType::Sampler);
        s_emissive = bgfx::createUniform("s_emissive", bgfx::UniformType::Sampler);
        s_irradiance = bgfx::createUniform("s_irradiance", bgfx::UniformType::Sampler);
        s_prefilter = bgfx::createUniform("s_prefilter", bgfx::UniformType::Sampler);
        s_brdfLUT = bgfx::createUniform("s_brdfLUT", bgfx::UniformType::Sampler);

        // Shadow map samplers
        s_shadowMap0 = bgfx::createUniform("s_shadowMap0", bgfx::UniformType::Sampler);
        s_shadowMap1 = bgfx::createUniform("s_shadowMap1", bgfx::UniformType::Sampler);
        s_shadowMap2 = bgfx::createUniform("s_shadowMap2", bgfx::UniformType::Sampler);
        s_shadowMap3 = bgfx::createUniform("s_shadowMap3", bgfx::UniformType::Sampler);

        // Blit sampler
        s_blit_texture = bgfx::createUniform("s_texture", bgfx::UniformType::Sampler);
    }

    void destroy() {
        if (bgfx::isValid(u_cameraPos)) bgfx::destroy(u_cameraPos);
        if (bgfx::isValid(u_albedoColor)) bgfx::destroy(u_albedoColor);
        if (bgfx::isValid(u_pbrParams)) bgfx::destroy(u_pbrParams);
        if (bgfx::isValid(u_emissiveColor)) bgfx::destroy(u_emissiveColor);
        if (bgfx::isValid(u_lights)) bgfx::destroy(u_lights);
        if (bgfx::isValid(u_lightCount)) bgfx::destroy(u_lightCount);
        if (bgfx::isValid(u_iblParams)) bgfx::destroy(u_iblParams);
        if (bgfx::isValid(u_time)) bgfx::destroy(u_time);

        // Shadow uniforms
        if (bgfx::isValid(u_shadowParams)) bgfx::destroy(u_shadowParams);
        if (bgfx::isValid(u_cascadeSplits)) bgfx::destroy(u_cascadeSplits);
        if (bgfx::isValid(u_shadowMatrix0)) bgfx::destroy(u_shadowMatrix0);
        if (bgfx::isValid(u_shadowMatrix1)) bgfx::destroy(u_shadowMatrix1);
        if (bgfx::isValid(u_shadowMatrix2)) bgfx::destroy(u_shadowMatrix2);
        if (bgfx::isValid(u_shadowMatrix3)) bgfx::destroy(u_shadowMatrix3);

        if (bgfx::isValid(s_albedo)) bgfx::destroy(s_albedo);
        if (bgfx::isValid(s_normal)) bgfx::destroy(s_normal);
        if (bgfx::isValid(s_metallicRoughness)) bgfx::destroy(s_metallicRoughness);
        if (bgfx::isValid(s_ao)) bgfx::destroy(s_ao);
        if (bgfx::isValid(s_emissive)) bgfx::destroy(s_emissive);
        if (bgfx::isValid(s_irradiance)) bgfx::destroy(s_irradiance);
        if (bgfx::isValid(s_prefilter)) bgfx::destroy(s_prefilter);
        if (bgfx::isValid(s_brdfLUT)) bgfx::destroy(s_brdfLUT);

        // Shadow map samplers
        if (bgfx::isValid(s_shadowMap0)) bgfx::destroy(s_shadowMap0);
        if (bgfx::isValid(s_shadowMap1)) bgfx::destroy(s_shadowMap1);
        if (bgfx::isValid(s_shadowMap2)) bgfx::destroy(s_shadowMap2);
        if (bgfx::isValid(s_shadowMap3)) bgfx::destroy(s_shadowMap3);

        // Blit sampler
        if (bgfx::isValid(s_blit_texture)) bgfx::destroy(s_blit_texture);
    }
};

// PBRUniforms instance is now a member of BGFXRenderer (m_pbr_uniforms)

// Helper function to load shader binary from file
static bgfx::ShaderHandle load_shader_from_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        log(LogLevel::Error, ("Failed to open shader file: " + path).c_str());
        return BGFX_INVALID_HANDLE;
    }

    std::streampos pos = file.tellg();
    if (pos == std::streampos(-1) || pos <= 0) {
        log(LogLevel::Error, ("Failed to get shader file size: " + path).c_str());
        return BGFX_INVALID_HANDLE;
    }

    auto size = static_cast<std::streamsize>(pos);
    file.seekg(0, std::ios::beg);

    if (!file.good()) {
        log(LogLevel::Error, ("Failed to seek in shader file: " + path).c_str());
        return BGFX_INVALID_HANDLE;
    }

    const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(size) + 1);
    file.read(reinterpret_cast<char*>(mem->data), size);

    if (file.gcount() != size) {
        log(LogLevel::Error, ("Failed to read complete shader file: " + path).c_str());
        // Note: bgfx::alloc memory is owned by bgfx and will be freed internally
        return BGFX_INVALID_HANDLE;
    }

    mem->data[size] = '\0';
    return bgfx::createShader(mem);
}

class BGFXRenderer : public IRenderer {
public:
    ~BGFXRenderer() override {
        if (m_initialized) {
            shutdown();
        }
    }

    bool init(void* native_window_handle, uint32_t width, uint32_t height) override {
        m_width = width;
        m_height = height;

        bgfx::Init init;
        init.platformData.nwh = native_window_handle;
        init.type = bgfx::RendererType::Count;  // Auto-select
        init.resolution.width = width;
        init.resolution.height = height;
        init.resolution.reset = m_vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;

        if (!bgfx::init(init)) {
            log(LogLevel::Error, "Failed to initialize BGFX");
            return false;
        }

        // Initialize vertex layout
        s_vertex_layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Tangent, 3, bgfx::AttribType::Float)
            .end();

        // Set view 0 as default
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
        bgfx::setViewRect(0, 0, 0, uint16_t(width), uint16_t(height));

        // Load default shader based on renderer type
        std::string shader_path;
        auto renderer_type = bgfx::getRendererType();
        switch (renderer_type) {
            case bgfx::RendererType::Direct3D11: shader_path = "shaders/dx11/"; break;
            case bgfx::RendererType::Direct3D12: shader_path = "shaders/dx11/"; break;
            case bgfx::RendererType::Vulkan:     shader_path = "shaders/spirv/"; break;
            case bgfx::RendererType::OpenGL:     shader_path = "shaders/glsl/"; break;
            default:                             shader_path = "shaders/spirv/"; break;
        }

        bgfx::ShaderHandle vsh = load_shader_from_file(shader_path + "vs_default.sc.bin");
        bgfx::ShaderHandle fsh = load_shader_from_file(shader_path + "fs_default.sc.bin");

        if (bgfx::isValid(vsh) && bgfx::isValid(fsh)) {
            m_default_program = bgfx::createProgram(vsh, fsh, true);
            log(LogLevel::Info, "Default shader program loaded successfully");
        } else {
            log(LogLevel::Warn, "Failed to load default shader program");
        }

        // Load PBR shader
        bgfx::ShaderHandle pbr_vsh = load_shader_from_file(shader_path + "vs_pbr.sc.bin");
        bgfx::ShaderHandle pbr_fsh = load_shader_from_file(shader_path + "fs_pbr.sc.bin");

        if (bgfx::isValid(pbr_vsh) && bgfx::isValid(pbr_fsh)) {
            m_pbr_program = bgfx::createProgram(pbr_vsh, pbr_fsh, true);
            log(LogLevel::Info, "PBR shader program loaded successfully");
        } else {
            log(LogLevel::Warn, "Failed to load PBR shader program - using default");
            m_pbr_program = m_default_program;
        }

        // Load shadow shader
        bgfx::ShaderHandle shadow_vsh = load_shader_from_file(shader_path + "vs_shadow.sc.bin");
        bgfx::ShaderHandle shadow_fsh = load_shader_from_file(shader_path + "fs_shadow.sc.bin");

        if (bgfx::isValid(shadow_vsh) && bgfx::isValid(shadow_fsh)) {
            m_shadow_program = bgfx::createProgram(shadow_vsh, shadow_fsh, true);
            log(LogLevel::Info, "Shadow shader program loaded successfully");
        } else {
            log(LogLevel::Warn, "Failed to load shadow shader program");
        }

        // Load debug shader
        bgfx::ShaderHandle debug_vsh = load_shader_from_file(shader_path + "vs_debug.sc.bin");
        bgfx::ShaderHandle debug_fsh = load_shader_from_file(shader_path + "fs_debug.sc.bin");

        if (bgfx::isValid(debug_vsh) && bgfx::isValid(debug_fsh)) {
            m_debug_program = bgfx::createProgram(debug_vsh, debug_fsh, true);
            log(LogLevel::Info, "Debug shader program loaded successfully");
        } else {
            log(LogLevel::Warn, "Failed to load debug shader program");
        }

        // Initialize debug vertex layout
        m_debug_vertex_layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)  // Normalized
            .end();

        // Load skinned PBR shader
        bgfx::ShaderHandle skinned_vsh = load_shader_from_file(shader_path + "vs_skinned_pbr.sc.bin");
        // Skinned PBR uses the same fragment shader as regular PBR
        bgfx::ShaderHandle skinned_fsh = load_shader_from_file(shader_path + "fs_pbr.sc.bin");

        if (bgfx::isValid(skinned_vsh) && bgfx::isValid(skinned_fsh)) {
            m_skinned_pbr_program = bgfx::createProgram(skinned_vsh, skinned_fsh, true);
            log(LogLevel::Info, "Skinned PBR shader program loaded successfully");
        } else {
            log(LogLevel::Warn, "Failed to load skinned PBR shader program");
        }

        // Create bone matrices uniform (128 bones * 4 vec4s per matrix = 512 vec4s)
        m_u_boneMatrices = bgfx::createUniform("u_boneMatrices", bgfx::UniformType::Vec4, 512);

        // Load skybox shader
        bgfx::ShaderHandle skybox_vsh = load_shader_from_file(shader_path + "vs_skybox.sc.bin");
        bgfx::ShaderHandle skybox_fsh = load_shader_from_file(shader_path + "fs_skybox.sc.bin");

        if (bgfx::isValid(skybox_vsh) && bgfx::isValid(skybox_fsh)) {
            m_skybox_program = bgfx::createProgram(skybox_vsh, skybox_fsh, true);
            log(LogLevel::Info, "Skybox shader program loaded successfully");
        } else {
            log(LogLevel::Warn, "Failed to load skybox shader program");
        }

        // Create skybox uniforms
        m_u_skyboxParams = bgfx::createUniform("u_skyboxParams", bgfx::UniformType::Vec4);
        m_u_customInvViewProj = bgfx::createUniform("u_customInvViewProj", bgfx::UniformType::Mat4);
        m_s_skybox = bgfx::createUniform("s_skybox", bgfx::UniformType::Sampler);

        // Create fullscreen triangle vertex buffer for skybox
        // Uses a single triangle that covers the entire screen (NDC -1 to 3)
        m_skybox_vertex_layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .end();

        struct SkyboxVertex { float x, y, z; };
        static SkyboxVertex fullscreen_triangle[] = {
            {-1.0f, -1.0f, 0.0f},
            { 3.0f, -1.0f, 0.0f},
            {-1.0f,  3.0f, 0.0f}
        };
        m_fullscreen_triangle_vb = bgfx::createVertexBuffer(
            bgfx::makeRef(fullscreen_triangle, sizeof(fullscreen_triangle)),
            m_skybox_vertex_layout
        );

        // Load blit (fullscreen passthrough) shader
        bgfx::ShaderHandle blit_vsh = load_shader_from_file(shader_path + "vs_blit.sc.bin");
        bgfx::ShaderHandle blit_fsh = load_shader_from_file(shader_path + "fs_blit.sc.bin");

        if (bgfx::isValid(blit_vsh) && bgfx::isValid(blit_fsh)) {
            m_blit_program = bgfx::createProgram(blit_vsh, blit_fsh, true);
            log(LogLevel::Info, "Blit shader program loaded successfully");
        } else {
            // Clean up any partially loaded shaders
            if (bgfx::isValid(blit_vsh)) bgfx::destroy(blit_vsh);
            if (bgfx::isValid(blit_fsh)) bgfx::destroy(blit_fsh);
            log(LogLevel::Warn, "Failed to load blit shader program - blit_to_screen will be unavailable");
        }

        // Load billboard shader
        bgfx::ShaderHandle billboard_vsh = load_shader_from_file(shader_path + "vs_billboard.sc.bin");
        bgfx::ShaderHandle billboard_fsh = load_shader_from_file(shader_path + "fs_billboard.sc.bin");

        if (bgfx::isValid(billboard_vsh) && bgfx::isValid(billboard_fsh)) {
            m_billboard_program = bgfx::createProgram(billboard_vsh, billboard_fsh, true);
            log(LogLevel::Info, "Billboard shader program loaded successfully");
        } else {
            log(LogLevel::Warn, "Failed to load billboard shader program");
        }

        // Create billboard uniforms
        m_u_billboardColor = bgfx::createUniform("u_billboardColor", bgfx::UniformType::Vec4);
        m_u_billboardUV = bgfx::createUniform("u_billboardUV", bgfx::UniformType::Vec4);
        m_u_billboardParams = bgfx::createUniform("u_billboardParams", bgfx::UniformType::Vec4);
        m_s_billboard = bgfx::createUniform("s_billboard", bgfx::UniformType::Sampler);

        // Create PBR uniforms
        m_pbr_uniforms.create();

        // Create default 1x1 white texture for missing textures
        uint32_t white_pixel = 0xFFFFFFFF;
        m_white_texture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8,
            BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT,
            bgfx::copy(&white_pixel, sizeof(white_pixel)));

        // Create default 1x1 normal texture (flat normal pointing up)
        uint32_t normal_pixel = 0xFFFF8080;  // (128, 128, 255) = (0, 0, 1) in tangent space
        m_default_normal = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8,
            BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT,
            bgfx::copy(&normal_pixel, sizeof(normal_pixel)));

        // Create 1x1 dummy shadow texture (D32F format for comparison sampling support)
        // Note: D32F render targets initialize to 0.0 (near depth), so all shadow tests pass (no shadow)
        // BGFX_TEXTURE_RT creates a samplable render target (don't use RT_WRITE_ONLY - that can't be sampled)
        m_dummy_shadow_texture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::D32F,
            BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

        // Create 1x1 white cubemap for IBL fallback (irradiance + prefilter)
        // 6 faces, each 1x1 RGBA8 white pixel
        uint32_t white_faces[6] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
                                    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
        m_default_irradiance = bgfx::createTextureCube(1, false, 1, bgfx::TextureFormat::RGBA8,
            BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT, bgfx::copy(white_faces, sizeof(white_faces)));

        m_default_prefilter = bgfx::createTextureCube(1, false, 1, bgfx::TextureFormat::RGBA8,
            BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT, bgfx::copy(white_faces, sizeof(white_faces)));

        // BRDF LUT: 1x1 with R=1.0, G=0.0 — scale=1, bias=0
        uint32_t brdf_pixel = 0x000000FF; // R=255 (1.0), G=0 (0.0) in RGBA8
        m_default_brdf_lut = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8,
            BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT, bgfx::copy(&brdf_pixel, sizeof(brdf_pixel)));

        m_initialized = true;
        return true;
    }

    void shutdown() override {
        if (!m_initialized) {
            return;
        }

        // Destroy shader programs
        // Check if PBR program is different BEFORE destroying anything
        bool pbr_is_separate = bgfx::isValid(m_pbr_program) &&
                               bgfx::isValid(m_default_program) &&
                               m_pbr_program.idx != m_default_program.idx;

        if (bgfx::isValid(m_default_program)) {
            bgfx::destroy(m_default_program);
            m_default_program = BGFX_INVALID_HANDLE;
        }
        if (pbr_is_separate) {
            bgfx::destroy(m_pbr_program);
            m_pbr_program = BGFX_INVALID_HANDLE;
        } else {
            m_pbr_program = BGFX_INVALID_HANDLE;  // Was alias to default, already destroyed
        }
        if (bgfx::isValid(m_shadow_program)) {
            bgfx::destroy(m_shadow_program);
            m_shadow_program = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(m_debug_program)) {
            bgfx::destroy(m_debug_program);
            m_debug_program = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(m_skinned_pbr_program)) {
            bgfx::destroy(m_skinned_pbr_program);
            m_skinned_pbr_program = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(m_skybox_program)) {
            bgfx::destroy(m_skybox_program);
            m_skybox_program = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(m_billboard_program)) {
            bgfx::destroy(m_billboard_program);
            m_billboard_program = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(m_blit_program)) {
            bgfx::destroy(m_blit_program);
            m_blit_program = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(m_u_boneMatrices)) {
            bgfx::destroy(m_u_boneMatrices);
            m_u_boneMatrices = BGFX_INVALID_HANDLE;
        }

        // Destroy skybox resources
        if (bgfx::isValid(m_fullscreen_triangle_vb)) {
            bgfx::destroy(m_fullscreen_triangle_vb);
            m_fullscreen_triangle_vb = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(m_u_skyboxParams)) {
            bgfx::destroy(m_u_skyboxParams);
            m_u_skyboxParams = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(m_u_customInvViewProj)) {
            bgfx::destroy(m_u_customInvViewProj);
            m_u_customInvViewProj = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(m_s_skybox)) {
            bgfx::destroy(m_s_skybox);
            m_s_skybox = BGFX_INVALID_HANDLE;
        }

        // Destroy billboard resources
        if (bgfx::isValid(m_u_billboardColor)) {
            bgfx::destroy(m_u_billboardColor);
            m_u_billboardColor = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(m_u_billboardUV)) {
            bgfx::destroy(m_u_billboardUV);
            m_u_billboardUV = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(m_u_billboardParams)) {
            bgfx::destroy(m_u_billboardParams);
            m_u_billboardParams = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(m_s_billboard)) {
            bgfx::destroy(m_s_billboard);
            m_s_billboard = BGFX_INVALID_HANDLE;
        }

        // Destroy PBR uniforms
        m_pbr_uniforms.destroy();

        // Destroy default textures
        if (bgfx::isValid(m_white_texture)) {
            bgfx::destroy(m_white_texture);
        }
        if (bgfx::isValid(m_default_normal)) {
            bgfx::destroy(m_default_normal);
        }
        if (bgfx::isValid(m_dummy_shadow_texture)) {
            bgfx::destroy(m_dummy_shadow_texture);
        }
        if (bgfx::isValid(m_default_irradiance)) {
            bgfx::destroy(m_default_irradiance);
        }
        if (bgfx::isValid(m_default_prefilter)) {
            bgfx::destroy(m_default_prefilter);
        }
        if (bgfx::isValid(m_default_brdf_lut)) {
            bgfx::destroy(m_default_brdf_lut);
        }

        // Destroy all resources
        for (auto& [id, handle] : m_meshes) {
            bgfx::destroy(handle.vbh);
            if (bgfx::isValid(handle.ibh)) {
                bgfx::destroy(handle.ibh);
            }
        }
        m_meshes.clear();

        for (auto& [id, handle] : m_textures) {
            bgfx::destroy(handle);
        }
        m_textures.clear();

        for (auto& [id, handle] : m_shaders) {
            bgfx::destroy(handle);
        }
        m_shaders.clear();

        // Destroy render targets
        for (auto& [id, rt] : m_render_targets) {
            if (bgfx::isValid(rt.fbh)) {
                bgfx::destroy(rt.fbh);
            }
            for (auto th : rt.color_attachments) {
                bgfx::destroy(th);
            }
            if (bgfx::isValid(rt.depth_attachment)) {
                bgfx::destroy(rt.depth_attachment);
            }
        }
        m_render_targets.clear();

        bgfx::shutdown();
        m_initialized = false;
    }

    void begin_frame() override {
        bgfx::touch(0);

        auto now = std::chrono::steady_clock::now();
        if (m_last_frame_time.time_since_epoch().count() != 0) {
            auto elapsed = std::chrono::duration<float>(now - m_last_frame_time).count();
            // Clamp to prevent huge jumps (e.g. after breakpoint or sleep)
            m_delta_time = std::min(elapsed, 0.25f);
        }
        m_last_frame_time = now;
        m_total_time += m_delta_time;
    }

    void end_frame() override {
        bgfx::frame();
    }

    void resize(uint32_t width, uint32_t height) override {
        m_width = width;
        m_height = height;
        uint32_t flags = m_vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;
        bgfx::reset(width, height, flags);
        bgfx::setViewRect(0, 0, 0, uint16_t(width), uint16_t(height));
    }

    MeshHandle create_mesh(const MeshData& data) override {
        if (data.vertices.empty()) {
            return MeshHandle{};
        }

        MeshHandle handle{m_next_mesh_id++};

        BGFXMesh mesh;
        mesh.vertex_count = static_cast<uint32_t>(data.vertices.size());
        mesh.index_count = static_cast<uint32_t>(data.indices.size());
        mesh.bounds = data.bounds;

        // Create vertex buffer
        const bgfx::Memory* vb_mem = bgfx::copy(
            data.vertices.data(),
            static_cast<uint32_t>(data.vertices.size() * sizeof(Vertex))
        );
        mesh.vbh = bgfx::createVertexBuffer(vb_mem, s_vertex_layout);

        // Create index buffer if we have indices
        if (!data.indices.empty()) {
            const bgfx::Memory* ib_mem = bgfx::copy(
                data.indices.data(),
                static_cast<uint32_t>(data.indices.size() * sizeof(uint32_t))
            );
            mesh.ibh = bgfx::createIndexBuffer(ib_mem, BGFX_BUFFER_INDEX32);
        }

        m_meshes[handle.id] = mesh;
        return handle;
    }

    TextureHandle create_texture(const TextureData& data) override {
        if (data.pixels.empty()) {
            return TextureHandle{};
        }

        TextureHandle handle{m_next_texture_id++};

        bgfx::TextureFormat::Enum format = bgfx::TextureFormat::RGBA8;
        switch (data.format) {
            case TextureFormat::RGBA8: format = bgfx::TextureFormat::RGBA8; break;
            case TextureFormat::RGBA16F: format = bgfx::TextureFormat::RGBA16F; break;
            case TextureFormat::RGBA32F: format = bgfx::TextureFormat::RGBA32F; break;
            case TextureFormat::R8: format = bgfx::TextureFormat::R8; break;
            case TextureFormat::RG8: format = bgfx::TextureFormat::RG8; break;
            case TextureFormat::Depth24: format = bgfx::TextureFormat::D24; break;
            case TextureFormat::Depth32F: format = bgfx::TextureFormat::D32F; break;
            case TextureFormat::BC1: format = bgfx::TextureFormat::BC1; break;
            case TextureFormat::BC3: format = bgfx::TextureFormat::BC3; break;
            case TextureFormat::BC7: format = bgfx::TextureFormat::BC7; break;
        }

        const bgfx::Memory* mem = bgfx::copy(data.pixels.data(), static_cast<uint32_t>(data.pixels.size()));

        bgfx::TextureHandle th;
        if (data.is_cubemap) {
            th = bgfx::createTextureCube(
                uint16_t(data.width),
                data.mip_levels > 1,
                1, format,
                BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE,
                mem
            );
        } else if (data.depth > 1) {
            th = bgfx::createTexture3D(
                uint16_t(data.width),
                uint16_t(data.height),
                uint16_t(data.depth),
                data.mip_levels > 1,
                format,
                BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE,
                mem
            );
        } else {
            th = bgfx::createTexture2D(
                uint16_t(data.width),
                uint16_t(data.height),
                data.mip_levels > 1,
                1, format,
                BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE,
                mem
            );
        }

        m_textures[handle.id] = th;
        return handle;
    }

    ShaderHandle create_shader(const ShaderData& data) override {
        if (data.vertex_binary.empty() || data.fragment_binary.empty()) {
            return ShaderHandle{};
        }

        ShaderHandle handle{m_next_shader_id++};

        const bgfx::Memory* vs_mem = bgfx::copy(data.vertex_binary.data(), static_cast<uint32_t>(data.vertex_binary.size()));
        const bgfx::Memory* fs_mem = bgfx::copy(data.fragment_binary.data(), static_cast<uint32_t>(data.fragment_binary.size()));

        bgfx::ShaderHandle vsh = bgfx::createShader(vs_mem);
        bgfx::ShaderHandle fsh = bgfx::createShader(fs_mem);

        bgfx::ProgramHandle program = bgfx::createProgram(vsh, fsh, true);
        m_shaders[handle.id] = program;

        return handle;
    }

    MaterialHandle create_material(const MaterialData& data) override {
        MaterialHandle handle{m_next_material_id++};
        m_materials[handle.id] = data;
        return handle;
    }

    MeshHandle create_primitive(PrimitiveMesh type, float size) override {
        MeshData data;

        switch (type) {
            case PrimitiveMesh::Cube:
                data = create_cube_mesh(size);
                break;
            case PrimitiveMesh::Sphere:
                data = create_sphere_mesh(size, 32, 16);
                break;
            case PrimitiveMesh::Plane:
                data = create_plane_mesh(size);
                break;
            case PrimitiveMesh::Quad:
                data = create_quad_mesh(size);
                break;
            default:
                data = create_cube_mesh(size);
                break;
        }

        return create_mesh(data);
    }

    void destroy_mesh(MeshHandle h) override {
        auto it = m_meshes.find(h.id);
        if (it != m_meshes.end()) {
            bgfx::destroy(it->second.vbh);
            if (bgfx::isValid(it->second.ibh)) {
                bgfx::destroy(it->second.ibh);
            }
            m_meshes.erase(it);
        }
    }

    void destroy_texture(TextureHandle h) override {
        auto it = m_textures.find(h.id);
        if (it != m_textures.end()) {
            bgfx::destroy(it->second);
            m_textures.erase(it);
        }
    }

    void destroy_shader(ShaderHandle h) override {
        auto it = m_shaders.find(h.id);
        if (it != m_shaders.end()) {
            bgfx::destroy(it->second);
            m_shaders.erase(it);
        }
    }

    void destroy_material(MaterialHandle h) override {
        m_materials.erase(h.id);
    }

    RenderTargetHandle create_render_target(const RenderTargetDesc& desc) override {
        RenderTargetHandle handle{m_next_render_target_id++};
        BGFXRenderTarget rt;
        rt.desc = desc;

        // Calculate texture flags
        uint64_t flags = BGFX_TEXTURE_RT;
        if (desc.samplable) {
            flags |= BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
        }

        // Create color attachments
        std::vector<bgfx::Attachment> attachments;
        for (uint32_t i = 0; i < desc.color_attachment_count; ++i) {
            bgfx::TextureHandle th = bgfx::createTexture2D(
                uint16_t(desc.width),
                uint16_t(desc.height),
                desc.generate_mipmaps,
                1,
                to_bgfx_format(desc.color_format),
                flags
            );
            rt.color_attachments.push_back(th);

            // Create external texture handle
            TextureHandle ext_handle{m_next_texture_id++};
            m_textures[ext_handle.id] = th;
            rt.color_texture_handles.push_back(ext_handle);

            bgfx::Attachment att;
            att.init(th);
            attachments.push_back(att);
        }

        // Create depth attachment if requested
        if (desc.has_depth) {
            // Use RT_WRITE_ONLY only if not samplable (write-only is faster but can't be read in shaders)
            uint64_t depth_flags = desc.samplable
                ? (BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP)
                : (BGFX_TEXTURE_RT | BGFX_TEXTURE_RT_WRITE_ONLY);
            bgfx::TextureHandle th = bgfx::createTexture2D(
                uint16_t(desc.width),
                uint16_t(desc.height),
                false,
                1,
                to_bgfx_format(desc.depth_format),
                depth_flags
            );
            rt.depth_attachment = th;

            // Create external texture handle for depth
            TextureHandle ext_handle{m_next_texture_id++};
            m_textures[ext_handle.id] = th;
            rt.depth_texture_handle = ext_handle;

            bgfx::Attachment att;
            att.init(th);
            attachments.push_back(att);
        }

        // Create framebuffer
        rt.fbh = bgfx::createFrameBuffer(
            static_cast<uint8_t>(attachments.size()),
            attachments.data(),
            false  // Don't destroy textures with framebuffer
        );

        if (!bgfx::isValid(rt.fbh)) {
            log(LogLevel::Error, "Failed to create render target");
            // Cleanup bgfx textures
            for (auto th : rt.color_attachments) {
                bgfx::destroy(th);
            }
            if (bgfx::isValid(rt.depth_attachment)) {
                bgfx::destroy(rt.depth_attachment);
            }
            // Cleanup external texture handle mappings
            for (auto& ext : rt.color_texture_handles) {
                m_textures.erase(ext.id);
            }
            if (rt.depth_texture_handle.valid()) {
                m_textures.erase(rt.depth_texture_handle.id);
            }
            return RenderTargetHandle{};
        }

        m_render_targets[handle.id] = std::move(rt);

        if (desc.debug_name) {
            bgfx::setName(m_render_targets[handle.id].fbh, desc.debug_name);
        }

        log(LogLevel::Debug, ("Created render target " + std::to_string(handle.id) +
            " (" + std::to_string(desc.width) + "x" + std::to_string(desc.height) + ")").c_str());

        return handle;
    }

    void destroy_render_target(RenderTargetHandle h) override {
        auto it = m_render_targets.find(h.id);
        if (it == m_render_targets.end()) return;

        auto& rt = it->second;

        // Destroy framebuffer
        if (bgfx::isValid(rt.fbh)) {
            bgfx::destroy(rt.fbh);
        }

        // Destroy color textures
        for (auto th : rt.color_attachments) {
            bgfx::destroy(th);
        }
        for (auto& ext : rt.color_texture_handles) {
            m_textures.erase(ext.id);
        }

        // Destroy depth texture
        if (bgfx::isValid(rt.depth_attachment)) {
            bgfx::destroy(rt.depth_attachment);
            m_textures.erase(rt.depth_texture_handle.id);
        }

        m_render_targets.erase(it);
    }

    TextureHandle get_render_target_texture(RenderTargetHandle h, uint32_t attachment) override {
        auto it = m_render_targets.find(h.id);
        if (it == m_render_targets.end()) return TextureHandle{};

        const auto& rt = it->second;

        if (attachment == UINT32_MAX) {
            // Return depth attachment
            return rt.depth_texture_handle;
        }

        if (attachment < rt.color_texture_handles.size()) {
            return rt.color_texture_handles[attachment];
        }

        return TextureHandle{};
    }

    void resize_render_target(RenderTargetHandle h, uint32_t width, uint32_t height) override {
        auto it = m_render_targets.find(h.id);
        if (it == m_render_targets.end()) return;

        auto& rt = it->second;
        rt.desc.width = width;
        rt.desc.height = height;

        // Destroy only the bgfx GPU objects (framebuffer)
        if (bgfx::isValid(rt.fbh)) {
            bgfx::destroy(rt.fbh);
            rt.fbh = BGFX_INVALID_HANDLE;
        }

        // Calculate texture flags
        uint64_t flags = BGFX_TEXTURE_RT;
        if (rt.desc.samplable) {
            flags |= BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
        }

        // Recreate color attachment GPU textures in-place
        std::vector<bgfx::Attachment> attachments;
        for (size_t i = 0; i < rt.color_attachments.size(); ++i) {
            // Destroy old bgfx texture
            bgfx::destroy(rt.color_attachments[i]);

            // Create new bgfx texture at new size
            bgfx::TextureHandle th = bgfx::createTexture2D(
                uint16_t(width), uint16_t(height),
                rt.desc.generate_mipmaps, 1,
                to_bgfx_format(rt.desc.color_format), flags);
            rt.color_attachments[i] = th;

            // Update the existing external handle mapping (preserves TextureHandle for callers)
            m_textures[rt.color_texture_handles[i].id] = th;

            bgfx::Attachment att;
            att.init(th);
            attachments.push_back(att);
        }

        // Recreate depth attachment GPU texture in-place
        if (rt.desc.has_depth && bgfx::isValid(rt.depth_attachment)) {
            bgfx::destroy(rt.depth_attachment);

            uint64_t depth_flags = rt.desc.samplable
                ? (BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP)
                : (BGFX_TEXTURE_RT | BGFX_TEXTURE_RT_WRITE_ONLY);
            bgfx::TextureHandle th = bgfx::createTexture2D(
                uint16_t(width), uint16_t(height),
                false, 1,
                to_bgfx_format(rt.desc.depth_format), depth_flags);
            rt.depth_attachment = th;

            // Update external handle mapping
            m_textures[rt.depth_texture_handle.id] = th;

            bgfx::Attachment att;
            att.init(th);
            attachments.push_back(att);
        }

        // Recreate framebuffer
        rt.fbh = bgfx::createFrameBuffer(
            static_cast<uint8_t>(attachments.size()),
            attachments.data(),
            false);

        if (rt.desc.debug_name) {
            bgfx::setName(rt.fbh, rt.desc.debug_name);
        }

        log(LogLevel::Debug, ("Resized render target " + std::to_string(h.id) +
            " to " + std::to_string(width) + "x" + std::to_string(height)).c_str());
    }

    void configure_view(RenderView view, const ViewConfig& config) override {
        uint16_t view_id = static_cast<uint16_t>(view);
        m_view_configs[view_id] = config;

        // Set up the view
        if (config.render_target.valid()) {
            auto it = m_render_targets.find(config.render_target.id);
            if (it != m_render_targets.end()) {
                bgfx::setViewFrameBuffer(view_id, it->second.fbh);

                uint16_t w = config.viewport_width ? config.viewport_width : uint16_t(it->second.desc.width);
                uint16_t h = config.viewport_height ? config.viewport_height : uint16_t(it->second.desc.height);
                bgfx::setViewRect(view_id, config.viewport_x, config.viewport_y, w, h);
            }
        } else {
            // Use backbuffer
            bgfx::setViewFrameBuffer(view_id, BGFX_INVALID_HANDLE);

            uint16_t w = config.viewport_width ? config.viewport_width : uint16_t(m_width);
            uint16_t h = config.viewport_height ? config.viewport_height : uint16_t(m_height);
            bgfx::setViewRect(view_id, config.viewport_x, config.viewport_y, w, h);
        }

        // Set clear flags
        uint16_t clear_flags = 0;
        if (config.clear_color_enabled) clear_flags |= BGFX_CLEAR_COLOR;
        if (config.clear_depth_enabled) clear_flags |= BGFX_CLEAR_DEPTH;
        if (config.clear_stencil_enabled) clear_flags |= BGFX_CLEAR_STENCIL;

        bgfx::setViewClear(view_id, clear_flags, config.clear_color, config.clear_depth, config.clear_stencil);
    }

    void set_view_transform(RenderView view, const Mat4& view_matrix, const Mat4& proj_matrix) override {
        uint16_t view_id = static_cast<uint16_t>(view);
        bgfx::setViewTransform(view_id, glm::value_ptr(view_matrix), glm::value_ptr(proj_matrix));
    }

    void queue_draw(const DrawCall& call) override {
        m_draw_queue.push_back(call);
    }

    void queue_draw(const DrawCall& call, RenderView view) override {
        m_view_draw_queue.push_back({call, view});
    }

    void set_camera(const Mat4& view, const Mat4& proj) override {
        m_view_matrix = view;
        m_proj_matrix = proj;
        bgfx::setViewTransform(0, glm::value_ptr(view), glm::value_ptr(proj));

        // Extract camera position from inverse view matrix
        Mat4 inv_view = glm::inverse(view);
        m_camera_position = Vec3(inv_view[3]);
    }

    void set_light(uint32_t index, const LightData& light) override {
        if (index < 8) {
            m_lights[index] = light;
        }
    }

    void clear_lights() override {
        for (auto& light : m_lights) {
            light = LightData{};
        }
    }

    void set_shadow_data(const std::array<Mat4, 4>& cascade_matrices,
                          const Vec4& cascade_splits,
                          const Vec4& shadow_params) override {
        m_shadow_matrices = cascade_matrices;
        m_cascade_splits = cascade_splits;
        m_shadow_params = shadow_params;
    }

    void set_shadow_texture(uint32_t cascade, TextureHandle texture) override {
        if (cascade < 4) {
            auto it = m_textures.find(texture.id);
            if (it != m_textures.end()) {
                m_shadow_textures[cascade] = it->second;
            }
        }
    }

    void enable_shadows(bool enabled) override {
        m_shadows_enabled = enabled;
    }

    void submit_mesh(RenderView view, MeshHandle mesh, MaterialHandle material, const Mat4& transform) override {
        DrawCall call;
        call.mesh = mesh;
        call.material = material;
        call.transform = transform;
        queue_draw(call, view);
    }

    void submit_skinned_mesh(RenderView view, MeshHandle mesh, MaterialHandle material,
                              const Mat4& transform, const Mat4* bone_matrices, uint32_t bone_count) override {
        if (!bgfx::isValid(m_skinned_pbr_program) || !bone_matrices || bone_count == 0) {
            // Fall back to regular mesh rendering
            submit_mesh(view, mesh, material, transform);
            return;
        }

        // Get mesh data
        auto mesh_it = m_meshes.find(mesh.id);
        if (mesh_it == m_meshes.end()) return;

        const BGFXMesh& bgfx_mesh = mesh_it->second;
        if (!bgfx::isValid(bgfx_mesh.vbh)) return;

        // Get material data
        MaterialData* mat_data = nullptr;
        auto mat_it = m_materials.find(material.id);
        if (mat_it != m_materials.end()) {
            mat_data = &mat_it->second;
        }

        uint16_t view_id = static_cast<uint16_t>(view);

        // Set transform
        bgfx::setTransform(&transform);

        // Upload bone matrices (clamped to 128 bones max)
        uint32_t actual_bones = std::min(bone_count, 128u);
        bgfx::setUniform(m_u_boneMatrices, bone_matrices, actual_bones * 4);  // 4 vec4s per matrix

        // Set vertex/index buffers
        bgfx::setVertexBuffer(0, bgfx_mesh.vbh);
        if (bgfx::isValid(bgfx_mesh.ibh)) {
            bgfx::setIndexBuffer(bgfx_mesh.ibh);
        }

        // Set PBR material uniforms (same as regular PBR)
        if (mat_data) {
            Vec4 albedo_color(mat_data->albedo.x, mat_data->albedo.y, mat_data->albedo.z, mat_data->albedo.w);
            Vec4 pbr_params(mat_data->metallic, mat_data->roughness, mat_data->ao, mat_data->alpha_cutoff);
            Vec4 emissive_color(mat_data->emissive.x, mat_data->emissive.y, mat_data->emissive.z, 0.0f);

            bgfx::setUniform(m_pbr_uniforms.u_albedoColor, &albedo_color);
            bgfx::setUniform(m_pbr_uniforms.u_pbrParams, &pbr_params);
            bgfx::setUniform(m_pbr_uniforms.u_emissiveColor, &emissive_color);

            // Set textures
            auto bind_texture = [this](bgfx::UniformHandle uniform, TextureHandle tex, uint8_t slot, bgfx::TextureHandle fallback) {
                auto it = m_textures.find(tex.id);
                if (it != m_textures.end() && bgfx::isValid(it->second)) {
                    bgfx::setTexture(slot, uniform, it->second);
                } else {
                    bgfx::setTexture(slot, uniform, fallback);
                }
            };

            bind_texture(m_pbr_uniforms.s_albedo, mat_data->albedo_map, 0, m_white_texture);
            bind_texture(m_pbr_uniforms.s_normal, mat_data->normal_map, 1, m_default_normal);
            bind_texture(m_pbr_uniforms.s_metallicRoughness, mat_data->metallic_roughness_map, 2, m_white_texture);
            bind_texture(m_pbr_uniforms.s_ao, mat_data->ao_map, 3, m_white_texture);
            bind_texture(m_pbr_uniforms.s_emissive, mat_data->emissive_map, 4, m_white_texture);
        }

        // Set camera position for PBR specular
        Vec4 cam_pos(m_camera_position.x, m_camera_position.y, m_camera_position.z, 1.0f);
        bgfx::setUniform(m_pbr_uniforms.u_cameraPos, &cam_pos);

        // Set render state
        uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                         BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS |
                         BGFX_STATE_CULL_CW | BGFX_STATE_MSAA;

        bgfx::setState(state);
        bgfx::submit(view_id, m_skinned_pbr_program);
    }

    void flush_debug_draw(RenderView view) override {
        if (!bgfx::isValid(m_debug_program)) return;

        const auto& lines = DebugDraw::get_lines();
        if (lines.empty()) return;

        // Filter lines based on view (depth tested vs overlay)
        bool depth_test = (view == RenderView::Debug);
        std::vector<const DebugDraw::DebugLine*> filtered_lines;
        for (const auto& line : lines) {
            if (line.depth_test == depth_test) {
                filtered_lines.push_back(&line);
            }
        }

        if (filtered_lines.empty()) return;

        uint16_t view_id = static_cast<uint16_t>(view);

        // Each line has 2 vertices, need position (3 floats) + color (4 bytes packed)
        uint32_t num_lines = static_cast<uint32_t>(filtered_lines.size());
        uint32_t num_vertices = num_lines * 2;

        // Check if we can allocate transient buffer
        if (!bgfx::getAvailTransientVertexBuffer(num_vertices, m_debug_vertex_layout)) {
            return;  // Not enough space
        }

        bgfx::TransientVertexBuffer tvb;
        bgfx::allocTransientVertexBuffer(&tvb, num_vertices, m_debug_vertex_layout);

        // Debug vertex: 3 floats for position + 1 uint32 for packed RGBA color
        struct DebugVertex {
            float x, y, z;
            uint32_t abgr;
        };

        auto* vertex = reinterpret_cast<DebugVertex*>(tvb.data);
        for (const auto* line : filtered_lines) {
            // Convert RGBA to ABGR for bgfx
            auto rgba_to_abgr = [](uint32_t rgba) -> uint32_t {
                uint8_t r = (rgba >> 24) & 0xFF;
                uint8_t g = (rgba >> 16) & 0xFF;
                uint8_t b = (rgba >> 8) & 0xFF;
                uint8_t a = rgba & 0xFF;
                return (a << 24) | (b << 16) | (g << 8) | r;
            };

            // First vertex
            vertex->x = line->a.x;
            vertex->y = line->a.y;
            vertex->z = line->a.z;
            vertex->abgr = rgba_to_abgr(line->color_a);
            vertex++;

            // Second vertex
            vertex->x = line->b.x;
            vertex->y = line->b.y;
            vertex->z = line->b.z;
            vertex->abgr = rgba_to_abgr(line->color_b);
            vertex++;
        }

        // Set render state
        uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                         BGFX_STATE_PT_LINES | BGFX_STATE_LINEAA;

        if (depth_test) {
            state |= BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_WRITE_Z;
        }

        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setState(state);
        bgfx::submit(view_id, m_debug_program);
    }

    void blit_to_screen(RenderView view, TextureHandle source) override {
        // Blit a texture to the final backbuffer
        auto it = m_textures.find(source.id);
        if (it == m_textures.end()) return;

        // Blit program required — skybox shader can't be used as fallback because
        // it expects a cubemap sampler (s_skybox), not the 2D sampler (s_texture)
        // used by blit, producing a black screen or garbage output.
        if (!bgfx::isValid(m_blit_program)) {
            log(LogLevel::Error, "Blit shader program unavailable for blit_to_screen");
            return;
        }
        bgfx::ProgramHandle program = m_blit_program;

        bgfx::setTexture(0, m_pbr_uniforms.s_blit_texture, it->second);
        bgfx::setVertexBuffer(0, m_fullscreen_triangle_vb);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::submit(static_cast<uint16_t>(view), program);
    }

    void submit_skybox(RenderView view, TextureHandle cubemap,
                                const Mat4& inverse_view_proj,
                                float intensity, float rotation) override {
        // Validate we have the required resources
        if (!bgfx::isValid(m_skybox_program) || !bgfx::isValid(m_fullscreen_triangle_vb)) {
            return;
        }

        // Find the cubemap texture
        auto it = m_textures.find(cubemap.id);
        if (it == m_textures.end() || !bgfx::isValid(it->second)) {
            return;
        }

        uint16_t view_id = static_cast<uint16_t>(view);

        // Set inverse view-projection matrix uniform
        bgfx::setUniform(m_u_customInvViewProj, glm::value_ptr(inverse_view_proj));

        // Set skybox parameters: x=intensity, y=rotation (in radians)
        Vec4 skybox_params(intensity, rotation, 0.0f, 0.0f);
        bgfx::setUniform(m_u_skyboxParams, glm::value_ptr(skybox_params));

        // Bind cubemap texture
        bgfx::setTexture(0, m_s_skybox, it->second);

        // Set vertex buffer (fullscreen triangle)
        bgfx::setVertexBuffer(0, m_fullscreen_triangle_vb);

        // Set render state: write RGB, no depth write, depth test at far plane
        uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_LEQUAL;

        bgfx::setState(state);
        bgfx::submit(view_id, m_skybox_program);
    }

    void submit_billboard(RenderView view, MeshHandle quad_mesh, TextureHandle texture,
                                   const Mat4& transform, const Vec4& color,
                                   const Vec2& uv_offset, const Vec2& uv_scale,
                                   bool depth_test, bool depth_write) override {
        // Validate we have the required resources
        if (!bgfx::isValid(m_billboard_program)) {
            return;
        }

        // Find the mesh
        auto mesh_it = m_meshes.find(quad_mesh.id);
        if (mesh_it == m_meshes.end()) {
            return;
        }
        const BGFXMesh& mesh = mesh_it->second;

        // Find the texture
        auto tex_it = m_textures.find(texture.id);
        bgfx::TextureHandle tex_handle = m_white_texture;
        if (tex_it != m_textures.end() && bgfx::isValid(tex_it->second)) {
            tex_handle = tex_it->second;
        }

        uint16_t view_id = static_cast<uint16_t>(view);

        // Set transform
        bgfx::setTransform(glm::value_ptr(transform));

        // Set billboard color uniform
        bgfx::setUniform(m_u_billboardColor, glm::value_ptr(color));

        // Set UV offset and scale: xy=offset, zw=scale
        Vec4 uv_params(uv_offset.x, uv_offset.y, uv_scale.x, uv_scale.y);
        bgfx::setUniform(m_u_billboardUV, glm::value_ptr(uv_params));

        // Set billboard params (depth fade distance, unused)
        Vec4 billboard_params(0.5f, 0.0f, 0.0f, 0.0f);
        bgfx::setUniform(m_u_billboardParams, glm::value_ptr(billboard_params));

        // Bind texture
        bgfx::setTexture(0, m_s_billboard, tex_handle);

        // Set vertex and index buffers
        bgfx::setVertexBuffer(0, mesh.vbh);
        if (bgfx::isValid(mesh.ibh)) {
            bgfx::setIndexBuffer(mesh.ibh);
        }

        // Configure render state
        uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA;

        if (depth_test) {
            state |= BGFX_STATE_DEPTH_TEST_LESS;
        }
        if (depth_write) {
            state |= BGFX_STATE_WRITE_Z;
        }

        bgfx::setState(state);
        bgfx::submit(view_id, m_billboard_program);
    }

    void set_ao_texture(TextureHandle texture) override {
        auto it = m_textures.find(texture.id);
        if (it != m_textures.end()) {
            m_ao_texture = it->second;
        }
    }

    void flush() override {
        // Process legacy draw queue (view 0)
        if (!m_draw_queue.empty()) {
            // Sort draw calls by material then mesh for batching
            std::sort(m_draw_queue.begin(), m_draw_queue.end(),
                [](const DrawCall& a, const DrawCall& b) {
                    if (a.material.id != b.material.id) return a.material.id < b.material.id;
                    return a.mesh.id < b.mesh.id;
                });

            // Submit draw calls to view 0
            submit_draw_calls(m_draw_queue, 0);
            m_draw_queue.clear();
        }

        // Process view-specific draw queue
        if (!m_view_draw_queue.empty()) {
            // Sort by view, then by material, then by mesh
            std::sort(m_view_draw_queue.begin(), m_view_draw_queue.end(),
                [](const ViewDrawCall& a, const ViewDrawCall& b) {
                    uint16_t va = static_cast<uint16_t>(a.view);
                    uint16_t vb = static_cast<uint16_t>(b.view);
                    if (va != vb) return va < vb;
                    if (a.call.material.id != b.call.material.id)
                        return a.call.material.id < b.call.material.id;
                    return a.call.mesh.id < b.call.mesh.id;
                });

            // Submit per-view
            for (const auto& vdc : m_view_draw_queue) {
                submit_single_draw(vdc.call, static_cast<uint16_t>(vdc.view));
            }
            m_view_draw_queue.clear();
        }
    }

    // Helper to submit draw calls to a specific view
    void submit_draw_calls(const std::vector<DrawCall>& calls, uint16_t view_id) {
        for (const auto& call : calls) {
            submit_single_draw(call, view_id);
        }
    }

    void submit_single_draw(const DrawCall& call, uint16_t view_id) {
        auto mesh_it = m_meshes.find(call.mesh.id);
        if (mesh_it == m_meshes.end()) return;

        const auto& mesh = mesh_it->second;

        // Set transform
        bgfx::setTransform(glm::value_ptr(call.transform));

        // Set vertex buffer
        bgfx::setVertexBuffer(0, mesh.vbh);

        // Set index buffer if available
        if (bgfx::isValid(mesh.ibh)) {
            bgfx::setIndexBuffer(mesh.ibh);
        }

        // Set state
        uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                         BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS |
                         BGFX_STATE_CULL_CW | BGFX_STATE_MSAA;

        bgfx::setState(state);

        // Determine which shader program to use
        bgfx::ProgramHandle program = m_default_program;
        bool use_pbr = false;

        auto mat_it = m_materials.find(call.material.id);
        if (mat_it != m_materials.end()) {
            if (mat_it->second.shader.valid()) {
                auto shader_it = m_shaders.find(mat_it->second.shader.id);
                if (shader_it != m_shaders.end()) {
                    program = shader_it->second;
                }
            } else {
                // No custom shader - use PBR if available
                use_pbr = bgfx::isValid(m_pbr_program);
                if (use_pbr) {
                    program = m_pbr_program;
                }
            }
        }

        // Upload PBR uniforms if using PBR shader
        if (use_pbr) {
            const MaterialData* mat_ptr = (mat_it != m_materials.end()) ? &mat_it->second : nullptr;
            upload_pbr_uniforms(mat_ptr);
        }

        // Submit draw call
        bgfx::submit(view_id, program);
    }

    // Upload PBR uniforms (lights, camera, material, etc.)
    void upload_pbr_uniforms(const MaterialData* mat_data = nullptr) {
        // Camera position
        Vec4 cam_pos(m_camera_position, 1.0f);
        bgfx::setUniform(m_pbr_uniforms.u_cameraPos, glm::value_ptr(cam_pos));

        // Material values - use provided material data or fall back to defaults
        Vec4 albedo = mat_data
            ? mat_data->albedo
            : Vec4(1.0f, 1.0f, 1.0f, 1.0f);
        Vec4 pbr_params = mat_data
            ? Vec4(mat_data->metallic, mat_data->roughness, mat_data->ao, mat_data->alpha_cutoff)
            : Vec4(0.0f, 0.5f, 1.0f, 0.5f);  // metallic, roughness, ao, alpha_cutoff
        Vec4 emissive = mat_data
            ? Vec4(mat_data->emissive.x, mat_data->emissive.y, mat_data->emissive.z, 0.0f)
            : Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        bgfx::setUniform(m_pbr_uniforms.u_albedoColor, glm::value_ptr(albedo));
        bgfx::setUniform(m_pbr_uniforms.u_pbrParams, glm::value_ptr(pbr_params));
        bgfx::setUniform(m_pbr_uniforms.u_emissiveColor, glm::value_ptr(emissive));

        // Pack and upload light data
        std::array<Vec4, 32> light_data{};
        int active_light_count = 0;

        for (int i = 0; i < 8; ++i) {
            if (m_lights[i].intensity > 0.0f) {
                GPULightData gpu_light = packLightForGPU(m_lights[i]);
                int base = i * 4;
                light_data[base + 0] = gpu_light.position_type;
                light_data[base + 1] = gpu_light.direction_range;
                light_data[base + 2] = gpu_light.color_intensity;
                light_data[base + 3] = gpu_light.spot_params;
                active_light_count = i + 1;
            }
        }

        bgfx::setUniform(m_pbr_uniforms.u_lights, light_data.data(), 32);
        Vec4 light_count(static_cast<float>(active_light_count), 0.0f, 0.0f, 0.0f);
        bgfx::setUniform(m_pbr_uniforms.u_lightCount, glm::value_ptr(light_count));

        // IBL params (disabled by default)
        Vec4 ibl_params(m_ibl_intensity, 0.0f, 5.0f, 0.0f);  // intensity, rotation, max_mip, unused
        bgfx::setUniform(m_pbr_uniforms.u_iblParams, glm::value_ptr(ibl_params));

        // Time uniform
        Vec4 time_data(m_total_time, m_delta_time, std::sin(m_total_time), std::cos(m_total_time));
        bgfx::setUniform(m_pbr_uniforms.u_time, glm::value_ptr(time_data));

        // Shadow uniforms
        if (m_shadows_enabled) {
            bgfx::setUniform(m_pbr_uniforms.u_shadowParams, glm::value_ptr(m_shadow_params));
            bgfx::setUniform(m_pbr_uniforms.u_cascadeSplits, glm::value_ptr(m_cascade_splits));
            bgfx::setUniform(m_pbr_uniforms.u_shadowMatrix0, glm::value_ptr(m_shadow_matrices[0]));
            bgfx::setUniform(m_pbr_uniforms.u_shadowMatrix1, glm::value_ptr(m_shadow_matrices[1]));
            bgfx::setUniform(m_pbr_uniforms.u_shadowMatrix2, glm::value_ptr(m_shadow_matrices[2]));
            bgfx::setUniform(m_pbr_uniforms.u_shadowMatrix3, glm::value_ptr(m_shadow_matrices[3]));
        } else {
            // Set zero bias to disable shadows in shader
            Vec4 disabled_params(0.0f, 0.0f, 0.0f, 0.0f);
            bgfx::setUniform(m_pbr_uniforms.u_shadowParams, glm::value_ptr(disabled_params));
        }

        // Bind material textures (fall back to defaults when absent)
        auto bind_texture = [this](bgfx::UniformHandle uniform, TextureHandle tex, uint8_t slot, bgfx::TextureHandle fallback) {
            auto it = m_textures.find(tex.id);
            if (it != m_textures.end() && bgfx::isValid(it->second)) {
                bgfx::setTexture(slot, uniform, it->second);
            } else {
                bgfx::setTexture(slot, uniform, fallback);
            }
        };

        if (mat_data) {
            bind_texture(m_pbr_uniforms.s_albedo, mat_data->albedo_map, 0, m_white_texture);
            bind_texture(m_pbr_uniforms.s_normal, mat_data->normal_map, 1, m_default_normal);
            bind_texture(m_pbr_uniforms.s_metallicRoughness, mat_data->metallic_roughness_map, 2, m_white_texture);
            bind_texture(m_pbr_uniforms.s_ao, mat_data->ao_map, 3, m_white_texture);
            bind_texture(m_pbr_uniforms.s_emissive, mat_data->emissive_map, 4, m_white_texture);
        } else {
            bgfx::setTexture(0, m_pbr_uniforms.s_albedo, m_white_texture);
            bgfx::setTexture(1, m_pbr_uniforms.s_normal, m_default_normal);
            bgfx::setTexture(2, m_pbr_uniforms.s_metallicRoughness, m_white_texture);
            bgfx::setTexture(3, m_pbr_uniforms.s_ao, m_white_texture);
            bgfx::setTexture(4, m_pbr_uniforms.s_emissive, m_white_texture);
        }

        // IBL textures (slots 5-7) — use fallback white cubemaps when no real IBL is loaded
        bgfx::setTexture(5, m_pbr_uniforms.s_irradiance, m_default_irradiance);
        bgfx::setTexture(6, m_pbr_uniforms.s_prefilter, m_default_prefilter);
        bgfx::setTexture(7, m_pbr_uniforms.s_brdfLUT, m_default_brdf_lut);

        // Bind shadow map textures (slots 8-11) - always bind with comparison filtering
        // Must use D32F format textures for comparison sampling (SampleCmp); use dummy shadow texture as fallback
        for (int i = 0; i < 4; ++i) {
            bgfx::UniformHandle sampler;
            switch (i) {
                case 0: sampler = m_pbr_uniforms.s_shadowMap0; break;
                case 1: sampler = m_pbr_uniforms.s_shadowMap1; break;
                case 2: sampler = m_pbr_uniforms.s_shadowMap2; break;
                default: sampler = m_pbr_uniforms.s_shadowMap3; break;
            }
            bool is_valid = bgfx::isValid(m_shadow_textures[i]);
            bgfx::TextureHandle shadow_tex = is_valid
                ? m_shadow_textures[i]
                : m_dummy_shadow_texture;
            bgfx::setTexture(8 + i, sampler, shadow_tex,
                BGFX_SAMPLER_COMPARE_LEQUAL | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        }
    }

    void clear(uint32_t color, float depth) override {
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, color, depth, 0);
    }

    uint32_t get_width() const override { return m_width; }
    uint32_t get_height() const override { return m_height; }

    void set_vsync(bool enabled) override {
        m_vsync = enabled;
        uint32_t flags = enabled ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;
        bgfx::reset(m_width, m_height, flags);
    }

    bool get_vsync() const override { return m_vsync; }

    // Quality settings
    void set_render_scale(float scale) override {
        m_render_scale = std::clamp(scale, 0.5f, 2.0f);
        // Note: Actual resolution changes would be applied on next resize or frame
    }

    float get_render_scale() const override { return m_render_scale; }

    void set_shadow_quality(int quality) override {
        m_shadow_quality = std::clamp(quality, 0, 4);
        // 0=off, 1=low (512), 2=medium (1024), 3=high (2048), 4=ultra (4096)
    }

    int get_shadow_quality() const override { return m_shadow_quality; }

    void set_lod_bias(float bias) override {
        m_lod_bias = std::clamp(bias, -2.0f, 2.0f);
    }

    float get_lod_bias() const override { return m_lod_bias; }

    // Post-processing toggles
    void set_bloom_enabled(bool enabled) override { m_bloom_enabled = enabled; }
    void set_bloom_intensity(float intensity) override { m_bloom_intensity = std::max(0.0f, intensity); }
    bool get_bloom_enabled() const override { return m_bloom_enabled; }
    float get_bloom_intensity() const override { return m_bloom_intensity; }

    void set_ao_enabled(bool enabled) override { m_ao_enabled = enabled; }
    bool get_ao_enabled() const override { return m_ao_enabled; }

    void set_ibl_intensity(float intensity) override { m_ibl_intensity = std::max(0.0f, intensity); }
    float get_ibl_intensity() const override { return m_ibl_intensity; }

    void set_motion_blur_enabled(bool enabled) override { m_motion_blur_enabled = enabled; }
    bool get_motion_blur_enabled() const override { return m_motion_blur_enabled; }

    uint16_t get_native_texture_handle(TextureHandle h) const override {
        auto it = m_textures.find(h.id);
        if (it != m_textures.end()) {
            return it->second.idx;
        }
        return bgfx::kInvalidHandle;
    }

    MeshBufferInfo get_mesh_buffer_info(MeshHandle mesh) const override {
        MeshBufferInfo info{0, 0, 0, false};
        auto it = m_meshes.find(mesh.id);
        if (it != m_meshes.end()) {
            info.vertex_buffer = it->second.vbh.idx;
            info.index_buffer = it->second.ibh.idx;
            info.index_count = it->second.index_count;
            info.valid = bgfx::isValid(it->second.vbh);
        }
        return info;
    }

private:
    // Internal mesh structure
    struct BGFXMesh {
        bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
        bgfx::IndexBufferHandle ibh = BGFX_INVALID_HANDLE;
        uint32_t vertex_count = 0;
        uint32_t index_count = 0;
        AABB bounds;
    };

    // Internal render target structure
    struct BGFXRenderTarget {
        bgfx::FrameBufferHandle fbh = BGFX_INVALID_HANDLE;
        std::vector<bgfx::TextureHandle> color_attachments;
        bgfx::TextureHandle depth_attachment = BGFX_INVALID_HANDLE;
        RenderTargetDesc desc;
        std::vector<TextureHandle> color_texture_handles;  // External handles for color
        TextureHandle depth_texture_handle;                // External handle for depth
    };

    // View-specific draw queue entry
    struct ViewDrawCall {
        DrawCall call;
        RenderView view;
    };

    // Primitive mesh generators
    MeshData create_cube_mesh(float size) {
        MeshData data;
        float h = size * 0.5f;

        // 24 vertices (4 per face for proper normals)
        data.vertices = {
            // Front face
            {{-h, -h,  h}, {0, 0, 1}, {0, 0}, {1, 1, 1, 1}, {1, 0, 0}},
            {{ h, -h,  h}, {0, 0, 1}, {1, 0}, {1, 1, 1, 1}, {1, 0, 0}},
            {{ h,  h,  h}, {0, 0, 1}, {1, 1}, {1, 1, 1, 1}, {1, 0, 0}},
            {{-h,  h,  h}, {0, 0, 1}, {0, 1}, {1, 1, 1, 1}, {1, 0, 0}},
            // Back face
            {{ h, -h, -h}, {0, 0, -1}, {0, 0}, {1, 1, 1, 1}, {-1, 0, 0}},
            {{-h, -h, -h}, {0, 0, -1}, {1, 0}, {1, 1, 1, 1}, {-1, 0, 0}},
            {{-h,  h, -h}, {0, 0, -1}, {1, 1}, {1, 1, 1, 1}, {-1, 0, 0}},
            {{ h,  h, -h}, {0, 0, -1}, {0, 1}, {1, 1, 1, 1}, {-1, 0, 0}},
            // Top face
            {{-h,  h,  h}, {0, 1, 0}, {0, 0}, {1, 1, 1, 1}, {1, 0, 0}},
            {{ h,  h,  h}, {0, 1, 0}, {1, 0}, {1, 1, 1, 1}, {1, 0, 0}},
            {{ h,  h, -h}, {0, 1, 0}, {1, 1}, {1, 1, 1, 1}, {1, 0, 0}},
            {{-h,  h, -h}, {0, 1, 0}, {0, 1}, {1, 1, 1, 1}, {1, 0, 0}},
            // Bottom face
            {{-h, -h, -h}, {0, -1, 0}, {0, 0}, {1, 1, 1, 1}, {1, 0, 0}},
            {{ h, -h, -h}, {0, -1, 0}, {1, 0}, {1, 1, 1, 1}, {1, 0, 0}},
            {{ h, -h,  h}, {0, -1, 0}, {1, 1}, {1, 1, 1, 1}, {1, 0, 0}},
            {{-h, -h,  h}, {0, -1, 0}, {0, 1}, {1, 1, 1, 1}, {1, 0, 0}},
            // Right face
            {{ h, -h,  h}, {1, 0, 0}, {0, 0}, {1, 1, 1, 1}, {0, 0, -1}},
            {{ h, -h, -h}, {1, 0, 0}, {1, 0}, {1, 1, 1, 1}, {0, 0, -1}},
            {{ h,  h, -h}, {1, 0, 0}, {1, 1}, {1, 1, 1, 1}, {0, 0, -1}},
            {{ h,  h,  h}, {1, 0, 0}, {0, 1}, {1, 1, 1, 1}, {0, 0, -1}},
            // Left face
            {{-h, -h, -h}, {-1, 0, 0}, {0, 0}, {1, 1, 1, 1}, {0, 0, 1}},
            {{-h, -h,  h}, {-1, 0, 0}, {1, 0}, {1, 1, 1, 1}, {0, 0, 1}},
            {{-h,  h,  h}, {-1, 0, 0}, {1, 1}, {1, 1, 1, 1}, {0, 0, 1}},
            {{-h,  h, -h}, {-1, 0, 0}, {0, 1}, {1, 1, 1, 1}, {0, 0, 1}},
        };

        // 36 indices (6 per face)
        data.indices = {
            0, 1, 2, 0, 2, 3,       // Front
            4, 5, 6, 4, 6, 7,       // Back
            8, 9, 10, 8, 10, 11,    // Top
            12, 13, 14, 12, 14, 15, // Bottom
            16, 17, 18, 16, 18, 19, // Right
            20, 21, 22, 20, 22, 23  // Left
        };

        data.bounds = AABB{{-h, -h, -h}, {h, h, h}};
        return data;
    }

    MeshData create_sphere_mesh(float radius, int segments, int rings) {
        MeshData data;

        for (int ring = 0; ring <= rings; ++ring) {
            float theta = static_cast<float>(ring) * glm::pi<float>() / static_cast<float>(rings);
            float sin_theta = std::sin(theta);
            float cos_theta = std::cos(theta);

            for (int seg = 0; seg <= segments; ++seg) {
                float phi = static_cast<float>(seg) * 2.0f * glm::pi<float>() / static_cast<float>(segments);
                float sin_phi = std::sin(phi);
                float cos_phi = std::cos(phi);

                Vec3 normal{sin_theta * cos_phi, cos_theta, sin_theta * sin_phi};
                Vec3 pos = normal * radius;
                Vec2 uv{static_cast<float>(seg) / segments, static_cast<float>(ring) / rings};

                data.vertices.push_back({pos, normal, uv, {1, 1, 1, 1}, {-sin_phi, 0, cos_phi}});
            }
        }

        for (int ring = 0; ring < rings; ++ring) {
            for (int seg = 0; seg < segments; ++seg) {
                int a = ring * (segments + 1) + seg;
                int b = a + segments + 1;

                data.indices.push_back(a);
                data.indices.push_back(b);
                data.indices.push_back(a + 1);

                data.indices.push_back(b);
                data.indices.push_back(b + 1);
                data.indices.push_back(a + 1);
            }
        }

        data.bounds = AABB{{-radius, -radius, -radius}, {radius, radius, radius}};
        return data;
    }

    MeshData create_plane_mesh(float size) {
        MeshData data;
        float h = size * 0.5f;

        data.vertices = {
            {{-h, 0, -h}, {0, 1, 0}, {0, 0}, {1, 1, 1, 1}, {1, 0, 0}},
            {{ h, 0, -h}, {0, 1, 0}, {1, 0}, {1, 1, 1, 1}, {1, 0, 0}},
            {{ h, 0,  h}, {0, 1, 0}, {1, 1}, {1, 1, 1, 1}, {1, 0, 0}},
            {{-h, 0,  h}, {0, 1, 0}, {0, 1}, {1, 1, 1, 1}, {1, 0, 0}},
        };

        data.indices = {0, 1, 2, 0, 2, 3};
        data.bounds = AABB{{-h, 0, -h}, {h, 0, h}};
        return data;
    }

    MeshData create_quad_mesh(float size) {
        MeshData data;
        float h = size * 0.5f;

        data.vertices = {
            {{-h, -h, 0}, {0, 0, 1}, {0, 0}, {1, 1, 1, 1}, {1, 0, 0}},
            {{ h, -h, 0}, {0, 0, 1}, {1, 0}, {1, 1, 1, 1}, {1, 0, 0}},
            {{ h,  h, 0}, {0, 0, 1}, {1, 1}, {1, 1, 1, 1}, {1, 0, 0}},
            {{-h,  h, 0}, {0, 0, 1}, {0, 1}, {1, 1, 1, 1}, {1, 0, 0}},
        };

        data.indices = {0, 1, 2, 0, 2, 3};
        data.bounds = AABB{{-h, -h, 0}, {h, h, 0}};
        return data;
    }

    // State
    bool m_initialized = false;
    bool m_vsync = true;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    // Quality settings
    float m_render_scale = 1.0f;
    int m_shadow_quality = 3;  // Default: high
    float m_lod_bias = 0.0f;

    // Post-processing settings
    bool m_bloom_enabled = true;
    float m_bloom_intensity = 1.0f;
    bool m_ao_enabled = true;
    float m_ibl_intensity = 0.0f;
    bool m_motion_blur_enabled = false;

    // Shader programs
    bgfx::ProgramHandle m_default_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_pbr_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_shadow_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_debug_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_skinned_pbr_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_skybox_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_billboard_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_blit_program = BGFX_INVALID_HANDLE;

    // Skybox resources
    bgfx::VertexBufferHandle m_fullscreen_triangle_vb = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout m_skybox_vertex_layout;
    bgfx::UniformHandle m_u_skyboxParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_customInvViewProj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_s_skybox = BGFX_INVALID_HANDLE;

    // Billboard resources
    bgfx::UniformHandle m_u_billboardColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_billboardUV = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_billboardParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_s_billboard = BGFX_INVALID_HANDLE;

    // Skinned mesh uniform (128 bones * 4 vec4s per matrix = 512 vec4s)
    bgfx::UniformHandle m_u_boneMatrices = BGFX_INVALID_HANDLE;

    // Debug vertex layout (position + color)
    bgfx::VertexLayout m_debug_vertex_layout;

    // PBR uniforms (per-instance, not shared across renderers)
    PBRUniforms m_pbr_uniforms;

    // Default textures for PBR
    bgfx::TextureHandle m_white_texture = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_default_normal = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_dummy_shadow_texture = BGFX_INVALID_HANDLE;  // D32F format for shadow sampler fallback

    // Default IBL textures (fallback when no environment map is loaded)
    bgfx::TextureHandle m_default_irradiance = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_default_prefilter = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_default_brdf_lut = BGFX_INVALID_HANDLE;

    // Camera position (for PBR specular)
    Vec3 m_camera_position{0.0f};

    // Shadow system data
    bool m_shadows_enabled = false;

    // SSAO texture
    bgfx::TextureHandle m_ao_texture = BGFX_INVALID_HANDLE;
    std::array<Mat4, 4> m_shadow_matrices{Mat4(1.0f), Mat4(1.0f), Mat4(1.0f), Mat4(1.0f)};
    Vec4 m_cascade_splits{10.0f, 30.0f, 100.0f, 500.0f};
    Vec4 m_shadow_params{0.001f, 0.01f, 0.1f, 1.0f};  // bias, normalBias, cascadeBlend, pcfRadius
    std::array<bgfx::TextureHandle, 4> m_shadow_textures{{
        BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE
    }};

    // Time tracking for shader animations
    float m_total_time = 0.0f;
    float m_delta_time = 0.016f;
    std::chrono::steady_clock::time_point m_last_frame_time{};

    // Resources
    uint32_t m_next_mesh_id = 1;
    uint32_t m_next_texture_id = 1;
    uint32_t m_next_shader_id = 1;
    uint32_t m_next_material_id = 1;
    uint32_t m_next_render_target_id = 1;

    std::unordered_map<uint32_t, BGFXMesh> m_meshes;
    std::unordered_map<uint32_t, bgfx::TextureHandle> m_textures;
    std::unordered_map<uint32_t, bgfx::ProgramHandle> m_shaders;
    std::unordered_map<uint32_t, MaterialData> m_materials;
    std::unordered_map<uint32_t, BGFXRenderTarget> m_render_targets;

    // View configurations
    std::unordered_map<uint16_t, ViewConfig> m_view_configs;
    std::unordered_map<uint16_t, RenderTargetHandle> m_view_render_targets;

    // Camera
    Mat4 m_view_matrix{1.0f};
    Mat4 m_proj_matrix{1.0f};

    // Lights
    LightData m_lights[8]{};

    // Draw queues
    std::vector<DrawCall> m_draw_queue;  // Legacy queue for view 0
    std::vector<ViewDrawCall> m_view_draw_queue;  // Queue with view specification

    // Helper to convert TextureFormat to bgfx format
    static bgfx::TextureFormat::Enum to_bgfx_format(TextureFormat format) {
        switch (format) {
            case TextureFormat::RGBA8:    return bgfx::TextureFormat::RGBA8;
            case TextureFormat::RGBA16F:  return bgfx::TextureFormat::RGBA16F;
            case TextureFormat::RGBA32F:  return bgfx::TextureFormat::RGBA32F;
            case TextureFormat::R8:       return bgfx::TextureFormat::R8;
            case TextureFormat::RG8:      return bgfx::TextureFormat::RG8;
            case TextureFormat::Depth24:  return bgfx::TextureFormat::D24;
            case TextureFormat::Depth32F: return bgfx::TextureFormat::D32F;
            case TextureFormat::BC1:      return bgfx::TextureFormat::BC1;
            case TextureFormat::BC3:      return bgfx::TextureFormat::BC3;
            case TextureFormat::BC7:      return bgfx::TextureFormat::BC7;
            default:                      return bgfx::TextureFormat::RGBA8;
        }
    }
};

std::unique_ptr<IRenderer> create_bgfx_renderer() {
    return std::make_unique<BGFXRenderer>();
}

} // namespace engine::render
