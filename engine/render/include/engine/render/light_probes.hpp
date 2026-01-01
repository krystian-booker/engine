#pragma once

#include <engine/core/math.hpp>
#include <bgfx/bgfx.h>
#include <vector>
#include <array>
#include <memory>
#include <functional>

namespace engine::render {

using namespace engine::core;

// Spherical Harmonics order for light probes
// L2 = 9 coefficients per channel (RGB = 27 total)
constexpr uint32_t SH_COEFFICIENT_COUNT = 9;

// SH coefficients for a single color channel
using SHCoefficients = std::array<float, SH_COEFFICIENT_COUNT>;

// SH coefficients for RGB
struct SHCoefficientsRGB {
    SHCoefficients r;
    SHCoefficients g;
    SHCoefficients b;

    SHCoefficientsRGB() {
        r.fill(0.0f);
        g.fill(0.0f);
        b.fill(0.0f);
    }

    // Get dominant direction
    Vec3 get_dominant_direction() const {
        // L1 coefficients encode direction
        return normalize(Vec3(r[3], r[1], r[2]));
    }

    // Get average color (L0)
    Vec3 get_average_color() const {
        return Vec3(r[0], g[0], b[0]) * 0.282095f;  // 1/(2*sqrt(pi))
    }
};

// Individual light probe
struct LightProbe {
    Vec3 position = Vec3(0.0f);
    float radius = 10.0f;  // Influence radius

    SHCoefficientsRGB sh_coefficients;

    // Validity flag
    bool valid = false;
    bool needs_update = true;

    // Probe index in volume
    uint32_t index = 0;

    // Sample irradiance at a given normal direction
    Vec3 sample_irradiance(const Vec3& normal) const;

    // Clear coefficients
    void clear() {
        sh_coefficients = SHCoefficientsRGB();
        valid = false;
    }
};

// Light probe volume - grid-based collection of probes
struct LightProbeVolume {
    // Volume bounds
    Vec3 min_bounds = Vec3(-10.0f);
    Vec3 max_bounds = Vec3(10.0f);

    // Grid resolution
    uint32_t resolution_x = 4;
    uint32_t resolution_y = 2;
    uint32_t resolution_z = 4;

    // Probes
    std::vector<LightProbe> probes;

    // Volume properties
    bool enabled = true;
    int priority = 0;  // Higher priority volumes override lower ones
    float blend_distance = 1.0f;

    // Get probe at grid position
    LightProbe* get_probe(uint32_t x, uint32_t y, uint32_t z);
    const LightProbe* get_probe(uint32_t x, uint32_t y, uint32_t z) const;

    // Get probe index from grid position
    uint32_t get_probe_index(uint32_t x, uint32_t y, uint32_t z) const;

    // Get grid position from world position
    bool get_grid_position(const Vec3& world_pos, uint32_t& x, uint32_t& y, uint32_t& z) const;

    // Sample irradiance at world position
    Vec3 sample_irradiance(const Vec3& world_pos, const Vec3& normal) const;

    // Initialize probes
    void initialize();

    // Mark all probes for update
    void invalidate();

    // Get total probe count
    uint32_t get_probe_count() const {
        return resolution_x * resolution_y * resolution_z;
    }

    // Check if position is inside volume
    bool contains(const Vec3& pos) const {
        return pos.x >= min_bounds.x && pos.x <= max_bounds.x &&
               pos.y >= min_bounds.y && pos.y <= max_bounds.y &&
               pos.z >= min_bounds.z && pos.z <= max_bounds.z;
    }

    // Get cell size
    Vec3 get_cell_size() const {
        return Vec3(
            (max_bounds.x - min_bounds.x) / resolution_x,
            (max_bounds.y - min_bounds.y) / resolution_y,
            (max_bounds.z - min_bounds.z) / resolution_z
        );
    }
};

// Handle types
using LightProbeVolumeHandle = uint32_t;
constexpr LightProbeVolumeHandle INVALID_PROBE_VOLUME = UINT32_MAX;

// Light probe baking settings
struct LightProbeBakeSettings {
    uint32_t samples_per_probe = 256;    // Ray samples per probe
    uint32_t bounces = 2;                 // Indirect bounces
    float ray_max_distance = 1000.0f;     // Maximum ray distance
    bool include_sky = true;              // Include sky contribution
    bool include_emissives = true;        // Include emissive surfaces
    float intensity_multiplier = 1.0f;
};

// Light probe system configuration
struct LightProbeSystemConfig {
    uint32_t max_volumes = 64;
    uint32_t max_probes_per_volume = 512;
    bool use_gpu_baking = true;
    uint32_t probes_per_frame = 4;       // Incremental baking
};

// Ray hit callback for probe baking
struct ProbeRayHit {
    Vec3 position;
    Vec3 normal;
    Vec3 color;
    float distance;
    bool hit;
};
using ProbeRayCallback = std::function<ProbeRayHit(const Vec3& origin, const Vec3& direction)>;

// Light probe system
class LightProbeSystem {
public:
    LightProbeSystem() = default;
    ~LightProbeSystem();

    // Non-copyable
    LightProbeSystem(const LightProbeSystem&) = delete;
    LightProbeSystem& operator=(const LightProbeSystem&) = delete;

    // Initialize/shutdown
    void init(const LightProbeSystemConfig& config = {});
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Volume management
    LightProbeVolumeHandle create_volume(const Vec3& min_bounds,
                                          const Vec3& max_bounds,
                                          uint32_t res_x, uint32_t res_y, uint32_t res_z);
    void destroy_volume(LightProbeVolumeHandle handle);
    LightProbeVolume* get_volume(LightProbeVolumeHandle handle);
    const LightProbeVolume* get_volume(LightProbeVolumeHandle handle) const;

    // Baking
    void bake_all(const LightProbeBakeSettings& settings,
                  ProbeRayCallback ray_callback);
    void bake_volume(LightProbeVolumeHandle handle,
                     const LightProbeBakeSettings& settings,
                     ProbeRayCallback ray_callback);
    void bake_incremental(const LightProbeBakeSettings& settings,
                           ProbeRayCallback ray_callback);

    // Query probes at position
    Vec3 sample_irradiance(const Vec3& world_pos, const Vec3& normal) const;

    // Get volumes affecting a position
    std::vector<LightProbeVolumeHandle> get_volumes_at(const Vec3& pos) const;

    // Upload probe data to GPU
    void upload_to_gpu();

    // Get GPU texture for shader sampling
    bgfx::TextureHandle get_probe_texture() const { return m_probe_texture; }

    // Set sky color for fallback
    void set_sky_color(const Vec3& color) { m_sky_color = color; }
    void set_sky_sh(const SHCoefficientsRGB& sh) { m_sky_sh = sh; }

    // Debug visualization
    void debug_draw_probes(const Mat4& view_proj);
    void debug_draw_volume(LightProbeVolumeHandle handle, const Mat4& view_proj);

    // Statistics
    struct Stats {
        uint32_t total_volumes = 0;
        uint32_t total_probes = 0;
        uint32_t probes_baked = 0;
        uint32_t probes_pending = 0;
    };
    Stats get_stats() const { return m_stats; }

private:
    void bake_probe(LightProbe& probe,
                    const LightProbeBakeSettings& settings,
                    ProbeRayCallback ray_callback);
    void project_to_sh(const std::vector<std::pair<Vec3, Vec3>>& samples,
                        SHCoefficientsRGB& out_sh);

    LightProbeSystemConfig m_config;
    bool m_initialized = false;

    // Volumes
    std::vector<LightProbeVolume> m_volumes;
    std::vector<bool> m_volume_used;

    // GPU resources
    bgfx::TextureHandle m_probe_texture = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_probe_params = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_probes = BGFX_INVALID_HANDLE;

    // Fallback sky
    Vec3 m_sky_color = Vec3(0.2f, 0.3f, 0.5f);
    SHCoefficientsRGB m_sky_sh;

    // Incremental baking state
    uint32_t m_bake_volume_index = 0;
    uint32_t m_bake_probe_index = 0;

    // Stats
    Stats m_stats;
};

// Global light probe system
LightProbeSystem& get_light_probe_system();

// Light probe utilities
namespace LightProbeUtils {

// SH basis functions (L2)
inline float sh_basis(uint32_t index, const Vec3& dir) {
    static const float k0 = 0.282095f;    // 1/(2*sqrt(pi))
    static const float k1 = 0.488603f;    // sqrt(3)/(2*sqrt(pi))
    static const float k2 = 1.092548f;    // sqrt(15)/(2*sqrt(pi))
    static const float k3 = 0.315392f;    // sqrt(5)/(4*sqrt(pi))
    static const float k4 = 0.546274f;    // sqrt(15)/(4*sqrt(pi))

    switch (index) {
        case 0: return k0;                                    // L0
        case 1: return k1 * dir.y;                           // L1-1
        case 2: return k1 * dir.z;                           // L10
        case 3: return k1 * dir.x;                           // L11
        case 4: return k2 * dir.x * dir.y;                   // L2-2
        case 5: return k2 * dir.y * dir.z;                   // L2-1
        case 6: return k3 * (3.0f * dir.z * dir.z - 1.0f);  // L20
        case 7: return k2 * dir.x * dir.z;                   // L21
        case 8: return k4 * (dir.x * dir.x - dir.y * dir.y); // L22
        default: return 0.0f;
    }
}

// Evaluate SH at direction
inline Vec3 evaluate_sh(const SHCoefficientsRGB& sh, const Vec3& dir) {
    Vec3 result(0.0f);

    for (uint32_t i = 0; i < SH_COEFFICIENT_COUNT; ++i) {
        float basis = sh_basis(i, dir);
        result.x += sh.r[i] * basis;
        result.y += sh.g[i] * basis;
        result.z += sh.b[i] * basis;
    }

    return result;
}

// Project sample to SH
inline void add_sample_to_sh(const Vec3& direction, const Vec3& color,
                              float weight, SHCoefficientsRGB& sh) {
    for (uint32_t i = 0; i < SH_COEFFICIENT_COUNT; ++i) {
        float basis = sh_basis(i, direction);
        sh.r[i] += color.x * basis * weight;
        sh.g[i] += color.y * basis * weight;
        sh.b[i] += color.z * basis * weight;
    }
}

// Convolve SH with cosine lobe for irradiance
inline void convolve_cosine(SHCoefficientsRGB& sh) {
    // Zonal harmonics coefficients for cosine lobe
    static const float a0 = 3.141593f;      // pi
    static const float a1 = 2.094395f;      // 2*pi/3
    static const float a2 = 0.785398f;      // pi/4

    // Apply convolution
    sh.r[0] *= a0; sh.g[0] *= a0; sh.b[0] *= a0;  // L0
    sh.r[1] *= a1; sh.g[1] *= a1; sh.b[1] *= a1;  // L1
    sh.r[2] *= a1; sh.g[2] *= a1; sh.b[2] *= a1;
    sh.r[3] *= a1; sh.g[3] *= a1; sh.b[3] *= a1;
    sh.r[4] *= a2; sh.g[4] *= a2; sh.b[4] *= a2;  // L2
    sh.r[5] *= a2; sh.g[5] *= a2; sh.b[5] *= a2;
    sh.r[6] *= a2; sh.g[6] *= a2; sh.b[6] *= a2;
    sh.r[7] *= a2; sh.g[7] *= a2; sh.b[7] *= a2;
    sh.r[8] *= a2; sh.g[8] *= a2; sh.b[8] *= a2;
}

// Generate uniform sphere samples
inline void generate_sphere_samples(std::vector<Vec3>& samples, uint32_t count) {
    samples.resize(count);

    float golden_ratio = (1.0f + std::sqrt(5.0f)) / 2.0f;

    for (uint32_t i = 0; i < count; ++i) {
        float theta = 2.0f * 3.14159f * i / golden_ratio;
        float phi = std::acos(1.0f - 2.0f * (i + 0.5f) / count);

        samples[i] = Vec3(
            std::sin(phi) * std::cos(theta),
            std::sin(phi) * std::sin(theta),
            std::cos(phi)
        );
    }
}

// Trilinear interpolation of probes
inline Vec3 trilinear_sample(const Vec3 weights[2], const Vec3 probes[8]) {
    Vec3 result(0.0f);

    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                float w = weights[x].x * weights[y].y * weights[z].z;
                result = result + probes[z * 4 + y * 2 + x] * w;
            }
        }
    }

    return result;
}

// Create ambient SH from single color
inline SHCoefficientsRGB create_ambient_sh(const Vec3& color) {
    SHCoefficientsRGB sh;
    float l0_scale = 2.0f * std::sqrt(3.14159f);

    sh.r[0] = color.x * l0_scale;
    sh.g[0] = color.y * l0_scale;
    sh.b[0] = color.z * l0_scale;

    return sh;
}

} // namespace LightProbeUtils

// ECS Component for light probe volumes
struct LightProbeVolumeComponent {
    LightProbeVolumeHandle volume_handle = INVALID_PROBE_VOLUME;
    bool auto_update = false;
    float update_interval = 0.0f;
    float time_since_update = 0.0f;
};

} // namespace engine::render
