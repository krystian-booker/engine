#pragma once

#include <engine/render/types.hpp>
#include <engine/render/render_target.hpp>
#include <engine/render/shadow_system.hpp>
#include <engine/render/ssao.hpp>
#include <engine/render/post_process.hpp>
#include <engine/render/volumetric.hpp>
#include <engine/core/math.hpp>
#include <vector>
#include <functional>

namespace engine::render {

using namespace engine::core;

class IRenderer;

// Render pass flags for selective rendering
enum class RenderPassFlags : uint32_t {
    None          = 0,
    Shadows       = 1 << 0,
    DepthPrepass  = 1 << 1,
    GBuffer       = 1 << 2,
    SSAO          = 1 << 3,
    MainOpaque    = 1 << 4,
    Volumetric    = 1 << 5,
    Transparent   = 1 << 6,
    Particles     = 1 << 7,   // Particle systems
    SSR           = 1 << 8,
    PostProcess   = 1 << 9,
    TAA           = 1 << 10,
    Debug         = 1 << 11,
    UI            = 1 << 12,
    Final         = 1 << 13,

    // Common combinations
    AllOpaque     = Shadows | DepthPrepass | MainOpaque,
    AllEffects    = SSAO | Volumetric | Particles | SSR | PostProcess | TAA,
    All           = 0xFFFFFFFF
};

inline RenderPassFlags operator|(RenderPassFlags a, RenderPassFlags b) {
    return static_cast<RenderPassFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline RenderPassFlags operator&(RenderPassFlags a, RenderPassFlags b) {
    return static_cast<RenderPassFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool has_flag(RenderPassFlags flags, RenderPassFlags flag) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

// Quality preset for render pipeline
enum class RenderQuality {
    Low,        // Mobile/minimum spec
    Medium,     // Mid-range
    High,       // High-end
    Ultra,      // Enthusiast
    Custom      // User-defined settings
};

// Render pipeline configuration
struct RenderPipelineConfig {
    RenderQuality quality = RenderQuality::High;
    RenderPassFlags enabled_passes = RenderPassFlags::All;

    // Resolution settings
    float render_scale = 1.0f;          // Internal resolution multiplier
    bool dynamic_resolution = false;     // Adjust resolution based on performance
    float target_frametime_ms = 16.67f;  // Target frame time for dynamic resolution

    // Shadow settings
    ShadowConfig shadow_config;

    // SSAO settings
    SSAOConfig ssao_config;

    // Post-processing settings
    BloomConfig bloom_config;
    ToneMappingConfig tonemap_config;
    TAAConfig taa_config;

    // Volumetric settings
    VolumetricConfig volumetric_config;

    // Transparency settings
    bool order_independent_transparency = false;
    int max_oit_layers = 4;

    // Debug settings
    bool show_debug_overlay = false;
    bool wireframe_mode = false;
};

// Camera data for rendering
struct CameraData {
    Mat4 view_matrix{1.0f};
    Mat4 projection_matrix{1.0f};
    Mat4 view_projection{1.0f};
    Mat4 inverse_view{1.0f};
    Mat4 inverse_projection{1.0f};
    Mat4 inverse_view_projection{1.0f};
    Mat4 prev_view_projection{1.0f};  // For TAA/motion vectors

    Vec3 position{0.0f};
    Vec3 forward{0.0f, 0.0f, -1.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    Vec3 right{1.0f, 0.0f, 0.0f};

    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    float fov_y = 60.0f;  // Degrees
    float aspect_ratio = 16.0f / 9.0f;

    // Jitter for TAA
    Vec2 jitter{0.0f};
    Vec2 prev_jitter{0.0f};
};

// Renderable object
struct RenderObject {
    MeshHandle mesh;
    MaterialHandle material;
    Mat4 transform{1.0f};
    Mat4 prev_transform{1.0f};  // For motion vectors
    AABB bounds;
    uint32_t layer_mask = 0xFFFFFFFF;
    uint8_t blend_mode = 0;  // 0=Opaque, 1=AlphaTest, 2=AlphaBlend, 3=Additive, 4=Multiply
    bool visible = true;
    bool casts_shadows = true;
    bool receives_shadows = true;

    // Skinning data (optional)
    bool skinned = false;
    const Mat4* bone_matrices = nullptr;
    uint32_t bone_count = 0;
};

// Statistics from render pipeline
struct RenderStats {
    uint32_t draw_calls = 0;
    uint32_t triangles = 0;
    uint32_t vertices = 0;
    uint32_t objects_rendered = 0;
    uint32_t objects_culled = 0;
    uint32_t shadow_casters = 0;
    uint32_t lights = 0;

    float shadow_pass_ms = 0.0f;
    float depth_pass_ms = 0.0f;
    float ssao_pass_ms = 0.0f;
    float main_pass_ms = 0.0f;
    float volumetric_pass_ms = 0.0f;
    float transparent_pass_ms = 0.0f;
    float post_process_ms = 0.0f;
    float total_frame_ms = 0.0f;

    uint64_t gpu_memory_used = 0;
    uint64_t gpu_memory_total = 0;
};

// Callback for custom render passes
using CustomRenderCallback = std::function<void(IRenderer*, RenderView)>;

// Unified render pipeline that orchestrates all rendering passes
class RenderPipeline {
public:
    RenderPipeline() = default;
    ~RenderPipeline();

    // Initialize/shutdown
    void init(IRenderer* renderer, const RenderPipelineConfig& config);
    void shutdown();

    // Configuration
    void set_config(const RenderPipelineConfig& config);
    const RenderPipelineConfig& get_config() const { return m_config; }

    // Apply quality preset
    void apply_quality_preset(RenderQuality quality);

    // Frame rendering
    void begin_frame();
    void render(const CameraData& camera,
                const std::vector<RenderObject>& objects,
                const std::vector<LightData>& lights);
    void end_frame();

    // Submit individual render objects
    void submit_object(const RenderObject& object);
    void submit_light(const LightData& light);

    // Resize handling
    void resize(uint32_t width, uint32_t height);

    // Get final rendered texture (for editor integration)
    TextureHandle get_final_texture() const;
    TextureHandle get_depth_texture() const;

    // Debug visualization
    TextureHandle get_shadow_debug_texture() const;
    TextureHandle get_ssao_debug_texture() const;
    TextureHandle get_volumetric_debug_texture() const;

    // Custom render pass injection
    void add_custom_pass(RenderView after_view, CustomRenderCallback callback);

    // Statistics
    const RenderStats& get_stats() const { return m_stats; }

    // Subsystem access (for advanced usage)
    ShadowSystem* get_shadow_system() { return &m_shadow_system; }
    SSAOSystem* get_ssao_system() { return &m_ssao_system; }
    PostProcessSystem* get_post_process_system() { return &m_post_process_system; }
    TAASystem* get_taa_system() { return &m_taa_system; }
    VolumetricSystem* get_volumetric_system() { return &m_volumetric_system; }

private:
    // Render passes
    void shadow_pass(const CameraData& camera,
                     const std::vector<RenderObject>& objects,
                     const std::vector<LightData>& lights);
    void depth_prepass(const CameraData& camera,
                       const std::vector<RenderObject>& objects);
    void ssao_pass(const CameraData& camera);
    void main_pass(const CameraData& camera,
                   const std::vector<RenderObject>& objects,
                   const std::vector<LightData>& lights);
    void volumetric_pass(const CameraData& camera,
                         const std::vector<LightData>& lights);
    void transparent_pass(const CameraData& camera,
                          const std::vector<RenderObject>& objects,
                          const std::vector<LightData>& lights);
    void post_process_pass(const CameraData& camera);
    void debug_pass(const CameraData& camera);
    void final_pass();

    // Helper methods
    void create_render_targets();
    void destroy_render_targets();
    void update_camera_uniforms(const CameraData& camera);
    void update_light_uniforms(const std::vector<LightData>& lights);
    void cull_objects(const CameraData& camera,
                      const std::vector<RenderObject>& objects,
                      std::vector<const RenderObject*>& visible_objects);
    void sort_objects_front_to_back(const CameraData& camera,
                                    std::vector<const RenderObject*>& objects);
    void sort_objects_back_to_front(const CameraData& camera,
                                    std::vector<const RenderObject*>& objects);

    IRenderer* m_renderer = nullptr;
    RenderPipelineConfig m_config;
    bool m_initialized = false;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_internal_width = 0;   // After render scale
    uint32_t m_internal_height = 0;

    // Render targets
    RenderTargetHandle m_depth_target;
    RenderTargetHandle m_gbuffer;  // For future deferred path
    RenderTargetHandle m_hdr_target;
    RenderTargetHandle m_ldr_target;

    // Subsystems
    ShadowSystem m_shadow_system;
    SSAOSystem m_ssao_system;
    PostProcessSystem m_post_process_system;
    TAASystem m_taa_system;
    VolumetricSystem m_volumetric_system;

    // Frame data
    std::vector<const RenderObject*> m_visible_opaque;
    std::vector<const RenderObject*> m_visible_transparent;
    std::vector<const RenderObject*> m_shadow_casters;

    // Custom passes
    std::vector<std::pair<RenderView, CustomRenderCallback>> m_custom_passes;

    // Statistics
    RenderStats m_stats;

    // Frame counter
    uint32_t m_frame_count = 0;
};

// Helper to build camera data from common parameters
CameraData make_camera_data(
    const Vec3& position,
    const Vec3& target,
    const Vec3& up,
    float fov_y,
    float aspect_ratio,
    float near_plane,
    float far_plane
);

// Helper to build light data
LightData make_directional_light(
    const Vec3& direction,
    const Vec3& color,
    float intensity,
    bool casts_shadows = true
);

LightData make_point_light(
    const Vec3& position,
    const Vec3& color,
    float intensity,
    float range,
    bool casts_shadows = false
);

LightData make_spot_light(
    const Vec3& position,
    const Vec3& direction,
    const Vec3& color,
    float intensity,
    float range,
    float inner_angle,
    float outer_angle,
    bool casts_shadows = false
);

} // namespace engine::render
