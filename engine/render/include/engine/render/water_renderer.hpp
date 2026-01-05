#pragma once

#include <engine/render/types.hpp>
#include <engine/core/math.hpp>
#include <functional>
#include <memory>

namespace engine::render {

// Water quality presets
enum class WaterQuality : uint8_t {
    Low,        // Basic reflection, no refraction
    Medium,     // Planar reflection, simple refraction
    High,       // Full features, medium resolution
    Ultra       // Maximum quality, full resolution
};

// Water surface render settings
struct WaterRenderSettings {
    // Surface colors
    Vec3 shallow_color{0.1f, 0.4f, 0.5f};   // Color in shallow areas
    Vec3 deep_color{0.02f, 0.08f, 0.15f};    // Color in deep areas
    float depth_fade_distance = 8.0f;        // Distance for shallow-to-deep transition
    float opacity = 0.85f;                   // Base surface opacity

    // Reflections
    bool enable_reflection = true;
    float reflection_strength = 0.6f;
    int reflection_resolution = 512;         // Planar reflection texture size
    float reflection_clip_offset = 0.1f;     // Clipping plane offset

    // Refraction
    bool enable_refraction = true;
    float refraction_strength = 0.2f;
    float refraction_distortion = 0.03f;     // UV distortion amount

    // Normal mapping (dual normal maps for detail)
    bool enable_normal_maps = true;
    TextureHandle normal_map_1;
    TextureHandle normal_map_2;
    Vec2 normal_scale_1{8.0f, 8.0f};         // Tiling scale
    Vec2 normal_scale_2{4.0f, 4.0f};
    Vec2 normal_scroll_1{0.02f, 0.01f};      // Scroll speed
    Vec2 normal_scroll_2{-0.01f, 0.015f};

    // Waves (syncs with physics WaveSettings)
    bool enable_vertex_waves = true;
    float wave_amplitude = 0.3f;
    float wave_frequency = 1.0f;
    Vec2 wave_direction{1.0f, 0.3f};
    float wave_speed = 1.5f;
    bool use_gerstner = true;                // Realistic wave shape
    float gerstner_steepness = 0.4f;

    // Foam
    bool enable_foam = true;
    TextureHandle foam_texture;
    float foam_threshold = 0.6f;             // Wave height for foam
    float shore_foam_width = 1.5f;           // Foam at depth intersection
    float foam_intensity = 0.8f;
    Vec2 foam_scroll{0.03f, 0.02f};

    // Caustics (underwater light patterns)
    bool enable_caustics = true;
    TextureHandle caustics_texture;
    float caustics_scale = 2.0f;
    float caustics_speed = 0.5f;
    float caustics_intensity = 0.3f;

    // Fresnel effect
    float fresnel_power = 4.0f;              // Edge reflection strength
    float fresnel_bias = 0.02f;

    // Specular highlights
    float specular_power = 256.0f;
    float specular_intensity = 1.0f;

    // Quality
    WaterQuality quality = WaterQuality::High;
};

// Water surface component for entities
struct WaterSurfaceComponent {
    WaterRenderSettings settings;

    // Grid mesh parameters
    int grid_resolution = 64;                // Vertices per edge
    float grid_size = 100.0f;                // World size of water plane

    // Runtime state
    MeshHandle water_mesh;
    TextureHandle reflection_texture;
    TextureHandle refraction_texture;
    TextureHandle depth_texture;

    bool needs_rebuild = true;
    float time_offset = 0.0f;                // For wave animation
};

// Underwater effect settings
struct UnderwaterSettings {
    bool enabled = true;

    // Fog
    Vec3 fog_color{0.05f, 0.15f, 0.25f};
    float fog_density = 0.1f;
    float fog_start = 0.0f;
    float fog_end = 50.0f;

    // Color grading
    Vec3 tint_color{0.7f, 0.9f, 1.0f};
    float tint_strength = 0.3f;
    float saturation = 0.8f;

    // Distortion
    bool enable_distortion = true;
    float distortion_strength = 0.01f;
    float distortion_speed = 1.0f;

    // Caustics on surfaces
    bool enable_caustics = true;
    float caustics_intensity = 0.4f;

    // Audio hint (for audio system integration)
    bool trigger_underwater_audio = true;
};

// Water volume for underwater detection
struct WaterVolumeRenderComponent {
    float surface_height = 0.0f;             // Y position of water surface
    UnderwaterSettings underwater_settings;
    bool camera_underwater = false;          // Set by system each frame
};

// Water rendering system
class WaterRenderer {
public:
    static WaterRenderer& instance();

    WaterRenderer(const WaterRenderer&) = delete;
    WaterRenderer& operator=(const WaterRenderer&) = delete;

    // Initialization
    bool init();
    void shutdown();

    // Frame lifecycle
    void begin_frame(float dt);
    void end_frame();

    // Water surface rendering
    void render_water_surfaces(const RenderView& view);

    // Reflection pass (call before main scene render)
    void begin_reflection_pass(const WaterSurfaceComponent& water, const RenderView& view);
    void end_reflection_pass();
    bool is_rendering_reflection() const { return m_rendering_reflection; }
    Mat4 get_reflection_view_matrix() const { return m_reflection_view; }
    Vec4 get_clip_plane() const { return m_clip_plane; }

    // Underwater effects
    void update_underwater_state(const Vec3& camera_pos);
    bool is_camera_underwater() const { return m_camera_underwater; }
    const UnderwaterSettings& get_underwater_settings() const { return m_underwater_settings; }

    // Mesh generation
    MeshHandle create_water_grid_mesh(int resolution, float size);

    // Configuration
    void set_global_quality(WaterQuality quality);
    WaterQuality get_global_quality() const { return m_global_quality; }

    // Time for shader animation
    float get_water_time() const { return m_water_time; }

private:
    WaterRenderer();
    ~WaterRenderer();

    void load_default_textures();
    void create_shaders();
    void update_reflection_texture(WaterSurfaceComponent& water);

    bool m_initialized = false;
    float m_water_time = 0.0f;
    WaterQuality m_global_quality = WaterQuality::High;

    // Reflection rendering state
    bool m_rendering_reflection = false;
    Mat4 m_reflection_view;
    Vec4 m_clip_plane;

    // Underwater state
    bool m_camera_underwater = false;
    UnderwaterSettings m_underwater_settings;
    float m_current_surface_height = 0.0f;

    // Default resources
    TextureHandle m_default_normal_map;
    TextureHandle m_default_foam_texture;
    TextureHandle m_default_caustics_texture;
    ShaderHandle m_water_shader;
    ShaderHandle m_underwater_shader;
};

// Convenience function
inline WaterRenderer& water_renderer() {
    return WaterRenderer::instance();
}

// Shader uniform data structure (for GPU upload)
struct WaterShaderData {
    Vec4 shallow_color;          // xyz = color, w = opacity
    Vec4 deep_color;             // xyz = color, w = depth_fade_distance
    Vec4 wave_params;            // xy = direction, z = amplitude, w = frequency
    Vec4 wave_params2;           // x = speed, y = steepness, z = time, w = unused
    Vec4 normal_scroll;          // xy = scroll1, zw = scroll2
    Vec4 normal_scale;           // xy = scale1, zw = scale2
    Vec4 foam_params;            // x = threshold, y = shore_width, z = intensity, w = unused
    Vec4 fresnel_params;         // x = power, y = bias, z = reflection_strength, w = refraction_strength
    Vec4 specular_params;        // x = power, y = intensity, z = caustics_scale, w = caustics_speed
};

} // namespace engine::render
