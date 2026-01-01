#pragma once

#include <string>
#include <vector>
#include <memory>
#include <engine/core/math.hpp>

namespace engine::terrain {

using namespace engine::core;

// Heightmap data format
enum class HeightmapFormat : uint8_t {
    R8,         // 8-bit grayscale (0-255 -> 0-1)
    R16,        // 16-bit grayscale (0-65535 -> 0-1)
    R32F,       // 32-bit float (direct height values)
    Raw16       // Raw 16-bit data file
};

// Heightmap filtering mode
enum class HeightmapFilter : uint8_t {
    Nearest,    // Nearest neighbor
    Bilinear,   // Bilinear interpolation
    Bicubic     // Bicubic interpolation (smooth)
};

// Heightmap class - stores and samples height data
class Heightmap {
public:
    Heightmap() = default;
    ~Heightmap() = default;

    // Loading
    bool load_from_file(const std::string& path, HeightmapFormat format = HeightmapFormat::R16);
    bool load_from_memory(const void* data, uint32_t width, uint32_t height, HeightmapFormat format);
    bool load_raw(const std::string& path, uint32_t width, uint32_t height, HeightmapFormat format);

    // Generate procedurally
    void generate_flat(uint32_t width, uint32_t height, float height_value = 0.0f);
    void generate_noise(uint32_t width, uint32_t height, float frequency = 0.01f,
                        uint32_t octaves = 4, float persistence = 0.5f);
    void generate_from_function(uint32_t width, uint32_t height,
                                std::function<float(float x, float z)> height_func);

    // Sampling
    float sample(float u, float v, HeightmapFilter filter = HeightmapFilter::Bilinear) const;
    float sample_world(float x, float z, const Vec3& terrain_scale) const;

    // Get height at integer coordinates
    float get_height(uint32_t x, uint32_t y) const;
    void set_height(uint32_t x, uint32_t y, float height);

    // Normal calculation
    Vec3 calculate_normal(float u, float v, float terrain_scale_xz, float terrain_scale_y) const;
    Vec3 calculate_normal_world(float x, float z, const Vec3& terrain_scale) const;

    // Modification
    void smooth(uint32_t iterations = 1);
    void normalize(float min_height = 0.0f, float max_height = 1.0f);
    void apply_curve(std::function<float(float)> curve);
    void blend(const Heightmap& other, float weight);
    void add_noise(float frequency, float amplitude);

    // Erosion simulation (simple thermal/hydraulic)
    void erode_thermal(uint32_t iterations, float talus_angle = 0.5f);
    void erode_hydraulic(uint32_t iterations, float rain_amount = 0.01f,
                         float evaporation = 0.5f, float sediment_capacity = 0.01f);

    // Save
    bool save_to_file(const std::string& path, HeightmapFormat format = HeightmapFormat::R16) const;
    bool save_raw(const std::string& path) const;

    // Properties
    uint32_t get_width() const { return m_width; }
    uint32_t get_height() const { return m_height; }
    bool is_valid() const { return m_width > 0 && m_height > 0 && !m_data.empty(); }

    // Direct data access
    const std::vector<float>& get_data() const { return m_data; }
    std::vector<float>& get_data() { return m_data; }

    // Statistics
    float get_min_height() const { return m_min_height; }
    float get_max_height() const { return m_max_height; }
    void recalculate_bounds();

private:
    float sample_nearest(float u, float v) const;
    float sample_bilinear(float u, float v) const;
    float sample_bicubic(float u, float v) const;
    float cubic_interpolate(float p0, float p1, float p2, float p3, float t) const;

    std::vector<float> m_data;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    float m_min_height = 0.0f;
    float m_max_height = 1.0f;
};

// Splat map - controls texture blending
class SplatMap {
public:
    SplatMap() = default;
    ~SplatMap() = default;

    // Loading
    bool load_from_file(const std::string& path);  // RGBA image
    bool load_from_memory(const void* data, uint32_t width, uint32_t height, uint32_t channels);

    // Generate
    void generate_from_heightmap(const Heightmap& heightmap, uint32_t width, uint32_t height);
    void generate_from_slope(const Heightmap& heightmap, const Vec3& terrain_scale,
                             float grass_max_slope, float rock_min_slope);

    // Sampling
    Vec4 sample(float u, float v) const;  // Returns RGBA weights
    void get_weights_at(float u, float v, float* out_weights, uint32_t count) const;

    // Modification
    void set_weight(uint32_t x, uint32_t y, uint32_t channel, float weight);
    float get_weight(uint32_t x, uint32_t y, uint32_t channel) const;
    void normalize_weights();  // Ensure weights sum to 1

    // Painting (for editor)
    void paint(float u, float v, uint32_t channel, float strength, float radius,
               float falloff = 0.5f);

    // Properties
    uint32_t get_width() const { return m_width; }
    uint32_t get_height() const { return m_height; }
    uint32_t get_channels() const { return m_channels; }
    bool is_valid() const { return m_width > 0 && m_height > 0; }

    // Direct data access
    const std::vector<float>& get_data() const { return m_data; }

    // Save
    bool save_to_file(const std::string& path) const;

private:
    std::vector<float> m_data;  // Interleaved RGBA
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_channels = 4;
};

// Hole map - marks areas with no terrain
class HoleMap {
public:
    HoleMap() = default;

    bool load_from_file(const std::string& path);
    void generate(uint32_t width, uint32_t height, bool fill_value = false);

    bool is_hole(float u, float v) const;
    bool is_hole_at(uint32_t x, uint32_t y) const;
    void set_hole(uint32_t x, uint32_t y, bool is_hole);

    uint32_t get_width() const { return m_width; }
    uint32_t get_height() const { return m_height; }

private:
    std::vector<bool> m_data;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace engine::terrain
