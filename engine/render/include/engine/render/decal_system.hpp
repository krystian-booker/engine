#pragma once

#include <engine/core/math.hpp>
#include <engine/render/material.hpp>
#include <bgfx/bgfx.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <functional>

namespace engine::render {

using namespace engine::core;

// Decal projection modes
enum class DecalProjection : uint8_t {
    Box,        // Standard box projection
    Sphere,     // Spherical projection (for corners)
    Cylinder    // Cylindrical projection (for elongated surfaces)
};

// Decal blend modes
enum class DecalBlendMode : uint8_t {
    Normal,     // Standard alpha blending
    Additive,   // Additive blending (for glow effects)
    Multiply,   // Multiply blending (for stains)
    Overlay     // Overlay blending
};

// Decal channel flags
enum class DecalChannel : uint8_t {
    None      = 0,
    Albedo    = 1 << 0,
    Normal    = 1 << 1,
    Roughness = 1 << 2,
    Metallic  = 1 << 3,
    Emissive  = 1 << 4,
    All       = Albedo | Normal | Roughness | Metallic | Emissive
};

inline DecalChannel operator|(DecalChannel a, DecalChannel b) {
    return static_cast<DecalChannel>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline DecalChannel operator&(DecalChannel a, DecalChannel b) {
    return static_cast<DecalChannel>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline bool has_channel(DecalChannel mask, DecalChannel channel) {
    return (static_cast<uint8_t>(mask) & static_cast<uint8_t>(channel)) != 0;
}

// Decal definition - shared template for decals
struct DecalDefinition {
    std::string name;

    // Textures
    bgfx::TextureHandle albedo_texture = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle normal_texture = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle arm_texture = BGFX_INVALID_HANDLE;  // AO, Roughness, Metallic
    bgfx::TextureHandle emissive_texture = BGFX_INVALID_HANDLE;

    // Base color (multiplied with albedo texture)
    Vec4 base_color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);

    // Emissive color and intensity
    Vec3 emissive_color = Vec3(0.0f);
    float emissive_intensity = 1.0f;

    // PBR values (used when no ARM texture)
    float roughness = 0.5f;
    float metallic = 0.0f;

    // Size in world units
    Vec3 size = Vec3(1.0f, 1.0f, 1.0f);

    // Projection settings
    DecalProjection projection = DecalProjection::Box;
    DecalBlendMode blend_mode = DecalBlendMode::Normal;
    DecalChannel channels = DecalChannel::Albedo | DecalChannel::Normal;

    // Fade settings
    float angle_fade_start = 0.7f;  // Start fading at this dot product with surface normal
    float angle_fade_end = 0.3f;    // Fully faded at this dot product
    float distance_fade_start = 50.0f;  // Start distance fade
    float distance_fade_end = 100.0f;   // End distance fade

    // Sorting priority (higher = rendered on top)
    int32_t sort_priority = 0;

    // UV animation
    bool animate_uv = false;
    Vec2 uv_scroll_speed = Vec2(0.0f);
    Vec2 uv_tile = Vec2(1.0f);

    bool is_valid() const {
        return bgfx::isValid(albedo_texture) || bgfx::isValid(normal_texture);
    }
};

// Handle type for decal definitions
using DecalDefHandle = uint32_t;
constexpr DecalDefHandle INVALID_DECAL_DEF = UINT32_MAX;

// Individual decal instance
struct DecalInstance {
    // Transform
    Vec3 position = Vec3(0.0f);
    Quat rotation = Quat::identity();
    Vec3 scale = Vec3(1.0f);

    // Definition reference
    DecalDefHandle definition = INVALID_DECAL_DEF;

    // Instance overrides
    Vec4 color_tint = Vec4(1.0f);  // Multiplied with definition color
    float opacity = 1.0f;

    // Lifetime (-1 = infinite)
    float lifetime = -1.0f;
    float age = 0.0f;

    // Fade in/out
    float fade_in_time = 0.0f;
    float fade_out_time = 0.5f;

    // Instance ID for tracking
    uint32_t instance_id = 0;

    // Is this decal active?
    bool active = true;

    // Calculate current opacity based on lifetime
    float get_current_opacity() const {
        if (!active) return 0.0f;

        float fade_opacity = opacity;

        // Fade in
        if (fade_in_time > 0.0f && age < fade_in_time) {
            fade_opacity *= age / fade_in_time;
        }

        // Fade out (if has lifetime)
        if (lifetime > 0.0f && fade_out_time > 0.0f) {
            float time_remaining = lifetime - age;
            if (time_remaining < fade_out_time) {
                fade_opacity *= time_remaining / fade_out_time;
            }
        }

        return fade_opacity;
    }

    // Get the world transform matrix
    Mat4 get_transform() const {
        Mat4 result = Mat4::identity();
        result = Mat4::from_translation(position) *
                 Mat4::from_rotation(rotation) *
                 Mat4::from_scale(scale);
        return result;
    }

    // Check if expired
    bool is_expired() const {
        return lifetime > 0.0f && age >= lifetime;
    }
};

// Handle type for decal instances
using DecalHandle = uint32_t;
constexpr DecalHandle INVALID_DECAL = UINT32_MAX;

// Decal spawn parameters
struct DecalSpawnParams {
    Vec3 position = Vec3(0.0f);
    Vec3 direction = Vec3(0.0f, -1.0f, 0.0f);  // Forward direction of decal
    Vec3 up = Vec3(0.0f, 0.0f, 1.0f);          // Up direction
    Vec3 scale = Vec3(1.0f);

    DecalDefHandle definition = INVALID_DECAL_DEF;

    Vec4 color_tint = Vec4(1.0f);
    float opacity = 1.0f;
    float lifetime = -1.0f;
    float fade_in_time = 0.0f;
    float fade_out_time = 0.5f;

    bool random_rotation = false;  // Random rotation around direction axis
};

// Decal system configuration
struct DecalSystemConfig {
    uint32_t max_decals = 4096;
    uint32_t max_definitions = 256;
    float update_frequency = 30.0f;  // Updates per second for lifetime checks
    bool enable_distance_culling = true;
    float cull_distance = 150.0f;
};

// Decal system - manages decals in the scene
class DecalSystem {
public:
    DecalSystem() = default;
    ~DecalSystem();

    // Non-copyable
    DecalSystem(const DecalSystem&) = delete;
    DecalSystem& operator=(const DecalSystem&) = delete;

    // Initialize/shutdown
    void init(const DecalSystemConfig& config = {});
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Definition management
    DecalDefHandle create_definition(const DecalDefinition& def);
    void destroy_definition(DecalDefHandle handle);
    DecalDefinition* get_definition(DecalDefHandle handle);
    const DecalDefinition* get_definition(DecalDefHandle handle) const;

    // Instance management
    DecalHandle spawn(const DecalSpawnParams& params);
    DecalHandle spawn(DecalDefHandle def, const Vec3& position, const Vec3& direction);
    void destroy(DecalHandle handle);
    void destroy_all();
    DecalInstance* get_instance(DecalHandle handle);
    const DecalInstance* get_instance(DecalHandle handle) const;

    // Batch operations
    void spawn_batch(const std::vector<DecalSpawnParams>& params, std::vector<DecalHandle>& out_handles);
    void destroy_expired();

    // Update (call each frame)
    void update(float dt);

    // Render decals
    void render(bgfx::ViewId view_id,
                bgfx::TextureHandle depth_texture,
                bgfx::TextureHandle normal_texture,
                const Mat4& view_matrix,
                const Mat4& proj_matrix,
                const Mat4& inv_view_proj);

    // Query
    uint32_t get_active_count() const { return m_active_count; }
    uint32_t get_max_count() const { return m_config.max_decals; }

    // Visibility culling
    void set_camera_position(const Vec3& pos) { m_camera_position = pos; }
    std::vector<DecalHandle> get_visible_decals(const Vec3& camera_pos, float max_distance) const;

    // Statistics
    struct Stats {
        uint32_t active_decals = 0;
        uint32_t definitions = 0;
        uint32_t draws_this_frame = 0;
        uint32_t culled_this_frame = 0;
    };
    Stats get_stats() const { return m_stats; }

private:
    void create_unit_cube();
    void destroy_gpu_resources();
    Quat calculate_rotation(const Vec3& direction, const Vec3& up, bool random_rotation);

    DecalSystemConfig m_config;
    bool m_initialized = false;

    // Definitions
    std::vector<DecalDefinition> m_definitions;
    std::vector<bool> m_definition_used;
    uint32_t m_definition_count = 0;

    // Instances
    std::vector<DecalInstance> m_instances;
    std::vector<bool> m_instance_used;
    uint32_t m_active_count = 0;
    uint32_t m_next_instance_id = 1;

    // GPU resources
    bgfx::VertexBufferHandle m_cube_vb = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle m_cube_ib = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_decal_program = BGFX_INVALID_HANDLE;

    // Uniforms
    bgfx::UniformHandle u_decal_params = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_decal_color = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_decal_size = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_inv_view_proj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_depth = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_gbuffer_normal = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_decal_albedo = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_decal_normal = BGFX_INVALID_HANDLE;

    // Camera for culling
    Vec3 m_camera_position = Vec3(0.0f);

    // Timing
    float m_update_accumulator = 0.0f;

    // Stats
    Stats m_stats;
};

// ECS Component for decals attached to entities
struct DecalComponent {
    DecalHandle decal_handle = INVALID_DECAL;

    // Local offset from entity transform
    Vec3 local_offset = Vec3(0.0f);
    Quat local_rotation = Quat::identity();

    // Auto-update transform from entity
    bool follow_entity = true;
};

// Global decal system instance
DecalSystem& get_decal_system();

// Helper functions
namespace DecalHelpers {

// Create a blood splatter decal definition
inline DecalDefinition create_blood_splatter() {
    DecalDefinition def;
    def.name = "blood_splatter";
    def.base_color = Vec4(0.5f, 0.0f, 0.0f, 1.0f);
    def.size = Vec3(0.5f, 0.5f, 0.1f);
    def.channels = DecalChannel::Albedo;
    def.blend_mode = DecalBlendMode::Multiply;
    def.roughness = 0.8f;
    return def;
}

// Create a bullet hole decal definition
inline DecalDefinition create_bullet_hole() {
    DecalDefinition def;
    def.name = "bullet_hole";
    def.base_color = Vec4(0.1f, 0.1f, 0.1f, 1.0f);
    def.size = Vec3(0.05f, 0.05f, 0.02f);
    def.channels = DecalChannel::Albedo | DecalChannel::Normal;
    def.roughness = 0.9f;
    return def;
}

// Create a scorch mark decal definition
inline DecalDefinition create_scorch_mark() {
    DecalDefinition def;
    def.name = "scorch_mark";
    def.base_color = Vec4(0.05f, 0.05f, 0.05f, 1.0f);
    def.size = Vec3(1.0f, 1.0f, 0.1f);
    def.channels = DecalChannel::Albedo | DecalChannel::Roughness;
    def.roughness = 0.95f;
    def.blend_mode = DecalBlendMode::Multiply;
    return def;
}

// Create a footprint decal definition
inline DecalDefinition create_footprint() {
    DecalDefinition def;
    def.name = "footprint";
    def.base_color = Vec4(0.3f, 0.2f, 0.15f, 0.5f);
    def.size = Vec3(0.15f, 0.35f, 0.01f);
    def.channels = DecalChannel::Albedo | DecalChannel::Normal;
    def.roughness = 0.7f;
    def.lifetime = 30.0f;
    def.fade_out_time = 5.0f;
    return def;
}

// Create an emissive (glowing) decal definition
inline DecalDefinition create_glowing_rune() {
    DecalDefinition def;
    def.name = "glowing_rune";
    def.base_color = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    def.emissive_color = Vec3(0.2f, 0.5f, 1.0f);
    def.emissive_intensity = 5.0f;
    def.size = Vec3(0.5f, 0.5f, 0.1f);
    def.channels = DecalChannel::Emissive;
    def.animate_uv = true;
    def.uv_scroll_speed = Vec2(0.1f, 0.0f);
    return def;
}

} // namespace DecalHelpers

} // namespace engine::render
