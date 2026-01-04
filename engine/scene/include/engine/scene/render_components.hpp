#pragma once

#include <engine/core/math.hpp>
#include <cstdint>

namespace engine::scene {

using namespace engine::core;

// Forward declarations for render handles (defined in engine::render)
struct MeshHandle { uint32_t id = UINT32_MAX; bool valid() const { return id != UINT32_MAX; } };
struct MaterialHandle { uint32_t id = UINT32_MAX; bool valid() const { return id != UINT32_MAX; } };
struct TextureHandle { uint32_t id = UINT32_MAX; bool valid() const { return id != UINT32_MAX; } };

// Mesh renderer component
struct MeshRenderer {
    MeshHandle mesh;
    MaterialHandle material;
    uint8_t render_layer = 0;  // For sorting/filtering
    bool visible = true;
    bool cast_shadows = true;
    bool receive_shadows = true;
};

// Camera component
struct Camera {
    float fov = 60.0f;           // Field of view in degrees
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    float aspect_ratio = 16.0f / 9.0f;
    uint8_t priority = 0;        // Higher priority = renders later
    bool active = true;
    bool orthographic = false;
    float ortho_size = 10.0f;    // Half-size for orthographic

    // Compute projection matrix
    Mat4 projection() const {
        if (orthographic) {
            float w = ortho_size * aspect_ratio;
            float h = ortho_size;
            return glm::ortho(-w, w, -h, h, near_plane, far_plane);
        } else {
            return glm::perspective(glm::radians(fov), aspect_ratio, near_plane, far_plane);
        }
    }
};

// Light types
enum class LightType : uint8_t {
    Directional = 0,
    Point,
    Spot
};

// Light component
struct Light {
    LightType type = LightType::Point;
    Vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float range = 10.0f;           // Point/Spot only
    float spot_inner_angle = 30.0f; // Spot only (degrees)
    float spot_outer_angle = 45.0f; // Spot only (degrees)
    bool cast_shadows = false;
    bool enabled = true;
};

// Skybox component (attach to camera or scene root)
struct Skybox {
    TextureHandle cubemap;
    float intensity = 1.0f;
    float rotation = 0.0f;  // Y-axis rotation in radians
};

// Billboard orientation mode
enum class BillboardMode : uint8_t {
    ScreenAligned,    // Always face camera
    AxisAligned,      // Rotate around Y axis only
    Fixed             // No automatic rotation
};

// Billboard component for world-space 2D elements
struct Billboard {
    TextureHandle texture;
    Vec2 size{1.0f, 1.0f};
    Vec4 color{1.0f};
    Vec2 uv_offset{0.0f};      // For sprite sheets
    Vec2 uv_scale{1.0f};       // For sprite sheets
    BillboardMode mode = BillboardMode::ScreenAligned;
    float rotation = 0.0f;      // Z-axis rotation in radians
    bool depth_test = true;
    bool visible = true;
};

// Particle emitter settings
struct ParticleEmitter {
    uint32_t max_particles = 1000;
    float emission_rate = 100.0f;  // Particles per second
    float lifetime = 2.0f;
    float initial_speed = 5.0f;
    Vec3 initial_velocity_variance{1.0f};
    Vec4 start_color{1.0f};
    Vec4 end_color{1.0f, 1.0f, 1.0f, 0.0f};
    float start_size = 0.1f;
    float end_size = 0.0f;
    Vec3 gravity{0.0f, -9.81f, 0.0f};
    bool enabled = true;
};

} // namespace engine::scene
