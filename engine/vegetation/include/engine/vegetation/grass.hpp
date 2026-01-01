#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <engine/core/math.hpp>

namespace engine::vegetation {

using namespace engine::core;

// Grass blade instance data (GPU)
struct GrassInstance {
    Vec3 position;
    float rotation;         // Y-axis rotation (radians)
    float scale;            // Uniform scale
    float bend;             // Wind bend amount
    uint32_t color_packed;  // RGBA8
    float random;           // Per-instance random value
};

// Grass wind settings
struct GrassWindSettings {
    Vec2 direction = Vec2(1.0f, 0.0f);
    float speed = 1.0f;
    float strength = 0.3f;
    float frequency = 2.0f;
    float turbulence = 0.5f;

    // Gust settings
    bool enable_gusts = true;
    float gust_strength = 0.5f;
    float gust_frequency = 0.1f;
    float gust_speed = 3.0f;
};

// Grass rendering settings
struct GrassSettings {
    // Density
    float density = 50.0f;                  // Blades per square unit
    float density_variance = 0.3f;          // Random density variation

    // Blade shape
    float blade_width = 0.03f;
    float blade_width_variance = 0.3f;
    float blade_height = 0.5f;
    float blade_height_variance = 0.4f;
    uint32_t blade_segments = 3;            // Tessellation segments

    // Color
    Vec3 base_color = Vec3(0.1f, 0.4f, 0.1f);
    Vec3 tip_color = Vec3(0.2f, 0.6f, 0.15f);
    float color_variance = 0.2f;
    Vec3 dry_color = Vec3(0.4f, 0.35f, 0.1f);  // For variation
    float dry_amount = 0.1f;

    // Wind
    GrassWindSettings wind;

    // LOD
    float lod_start_distance = 20.0f;
    float lod_end_distance = 60.0f;
    float cull_distance = 80.0f;
    bool use_distance_fade = true;
    float fade_start_distance = 50.0f;

    // Interaction
    bool enable_interaction = true;
    float interaction_radius = 1.0f;
    float interaction_strength = 1.0f;
    float interaction_recovery = 2.0f;      // Speed to recover from bend

    // Rendering
    bool cast_shadows = false;
    bool receive_shadows = true;
    bool use_alpha_cutoff = true;
    float alpha_cutoff = 0.5f;

    // Performance
    uint32_t max_instances = 100000;
    uint32_t chunk_size = 16;               // World units per chunk
};

// Grass chunk - a tile of grass instances
struct GrassChunk {
    Vec2 position;                          // World position (corner)
    float size;                             // Chunk size in world units
    AABB bounds;

    std::vector<GrassInstance> instances;
    uint32_t instance_buffer = UINT32_MAX;  // GPU buffer handle

    bool visible = false;
    bool dirty = false;
    float distance_to_camera = 0.0f;
    uint32_t lod = 0;
};

// Interaction source (player, objects that bend grass)
struct GrassInteractor {
    Vec3 position;
    Vec3 velocity;
    float radius = 1.0f;
    float strength = 1.0f;
};

// Grass system - manages grass rendering
class GrassSystem {
public:
    GrassSystem() = default;
    ~GrassSystem();

    // Initialize with terrain bounds
    void init(const AABB& terrain_bounds, const GrassSettings& settings = {});
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Settings
    void set_settings(const GrassSettings& settings);
    const GrassSettings& get_settings() const { return m_settings; }

    // Generation
    void generate_grass(std::function<float(float x, float z)> height_func,
                        std::function<float(float x, float z)> density_func = nullptr,
                        std::function<Vec3(float x, float z)> normal_func = nullptr);

    void generate_from_density_map(const void* density_data, uint32_t width, uint32_t height,
                                    std::function<float(float x, float z)> height_func);

    // Clear and regenerate specific region
    void regenerate_region(const AABB& region);
    void clear();

    // Update
    void update(float dt, const Vec3& camera_position, const Frustum& frustum);

    // Interaction
    void add_interactor(const GrassInteractor& interactor);
    void remove_interactor(uint32_t index);
    void clear_interactors();
    void set_player_position(const Vec3& position, const Vec3& velocity);

    // Rendering
    void render(uint16_t view_id);
    void render_shadow(uint16_t view_id);

    // Statistics
    struct Stats {
        uint32_t total_instances = 0;
        uint32_t visible_instances = 0;
        uint32_t visible_chunks = 0;
        uint32_t total_chunks = 0;
    };
    Stats get_stats() const { return m_stats; }

    // Textures
    void set_blade_texture(uint32_t texture) { m_blade_texture = texture; }
    void set_noise_texture(uint32_t texture) { m_noise_texture = texture; }

private:
    void generate_chunk(GrassChunk& chunk,
                        std::function<float(float x, float z)> height_func,
                        std::function<float(float x, float z)> density_func,
                        std::function<Vec3(float x, float z)> normal_func);

    void update_chunk_visibility(const Vec3& camera_pos, const Frustum& frustum);
    void update_wind(float dt);
    void update_interactions(float dt);
    void upload_chunk(GrassChunk& chunk);

    void create_gpu_resources();
    void destroy_gpu_resources();

    GrassSettings m_settings;
    AABB m_terrain_bounds;
    bool m_initialized = false;

    std::vector<GrassChunk> m_chunks;
    uint32_t m_chunks_x = 0;
    uint32_t m_chunks_z = 0;

    // Interactors
    std::vector<GrassInteractor> m_interactors;
    Vec3 m_player_position;
    Vec3 m_player_velocity;

    // Wind state
    float m_wind_time = 0.0f;
    Vec4 m_wind_params;  // direction.xy, time, strength

    // GPU resources
    uint32_t m_shader_program = UINT32_MAX;
    uint32_t m_shadow_program = UINT32_MAX;
    uint32_t m_blade_texture = UINT32_MAX;
    uint32_t m_noise_texture = UINT32_MAX;

    // Uniforms
    uint32_t m_u_wind_params = UINT32_MAX;
    uint32_t m_u_grass_params = UINT32_MAX;
    uint32_t m_u_interaction_data = UINT32_MAX;

    Stats m_stats;
};

// Global grass system
GrassSystem& get_grass_system();

// ECS component
struct GrassComponent {
    GrassSettings settings;
    bool auto_generate = true;
    std::string density_map_path;
};

} // namespace engine::vegetation
