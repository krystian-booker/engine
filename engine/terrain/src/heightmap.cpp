#include <engine/terrain/heightmap.hpp>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <random>

namespace engine::terrain {

bool Heightmap::load_from_file(const std::string& path, HeightmapFormat format) {
    // For now, use load_raw which expects a raw binary file
    // A full implementation would use image loading library (stb_image, etc.)
    return false;
}

bool Heightmap::load_from_memory(const void* data, uint32_t width, uint32_t height, HeightmapFormat format) {
    if (!data || width == 0 || height == 0) return false;

    m_width = width;
    m_height = height;
    m_data.resize(width * height);

    switch (format) {
        case HeightmapFormat::R8: {
            const uint8_t* src = static_cast<const uint8_t*>(data);
            for (size_t i = 0; i < m_data.size(); ++i) {
                m_data[i] = src[i] / 255.0f;
            }
            break;
        }
        case HeightmapFormat::R16: {
            const uint16_t* src = static_cast<const uint16_t*>(data);
            for (size_t i = 0; i < m_data.size(); ++i) {
                m_data[i] = src[i] / 65535.0f;
            }
            break;
        }
        case HeightmapFormat::R32F: {
            const float* src = static_cast<const float*>(data);
            std::copy(src, src + m_data.size(), m_data.begin());
            break;
        }
        case HeightmapFormat::Raw16: {
            const uint16_t* src = static_cast<const uint16_t*>(data);
            for (size_t i = 0; i < m_data.size(); ++i) {
                m_data[i] = src[i] / 65535.0f;
            }
            break;
        }
    }

    recalculate_bounds();
    return true;
}

bool Heightmap::load_raw(const std::string& path, uint32_t width, uint32_t height, HeightmapFormat format) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    size_t expected_size = 0;
    switch (format) {
        case HeightmapFormat::R8: expected_size = width * height; break;
        case HeightmapFormat::R16:
        case HeightmapFormat::Raw16: expected_size = width * height * 2; break;
        case HeightmapFormat::R32F: expected_size = width * height * 4; break;
    }

    if (file_size < expected_size) return false;

    std::vector<uint8_t> buffer(expected_size);
    file.read(reinterpret_cast<char*>(buffer.data()), expected_size);

    return load_from_memory(buffer.data(), width, height, format);
}

void Heightmap::generate_flat(uint32_t width, uint32_t height, float height_value) {
    m_width = width;
    m_height = height;
    m_data.resize(width * height, height_value);
    m_min_height = height_value;
    m_max_height = height_value;
}

void Heightmap::generate_noise(uint32_t width, uint32_t height, float frequency,
                                uint32_t octaves, float persistence) {
    m_width = width;
    m_height = height;
    m_data.resize(width * height);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> offset_dist(0.0f, 1000.0f);

    float offset_x = offset_dist(gen);
    float offset_z = offset_dist(gen);

    for (uint32_t z = 0; z < height; ++z) {
        for (uint32_t x = 0; x < width; ++x) {
            float h = 0.0f;
            float amp = 1.0f;
            float freq = frequency;
            float max_value = 0.0f;

            for (uint32_t o = 0; o < octaves; ++o) {
                float nx = (x + offset_x) * freq;
                float nz = (z + offset_z) * freq;

                // Simple noise approximation using sin
                float noise = std::sin(nx * 1.0f) * std::sin(nz * 1.0f) +
                              std::sin(nx * 2.3f + 1.7f) * std::sin(nz * 2.1f + 0.9f) * 0.5f +
                              std::sin(nx * 4.1f + 2.3f) * std::sin(nz * 3.7f + 1.1f) * 0.25f;
                noise = (noise + 1.0f) * 0.5f;  // Normalize to 0-1

                h += noise * amp;
                max_value += amp;

                amp *= persistence;
                freq *= 2.0f;
            }

            m_data[z * width + x] = h / max_value;
        }
    }

    recalculate_bounds();
}

void Heightmap::generate_from_function(uint32_t width, uint32_t height,
                                        std::function<float(float x, float z)> height_func) {
    m_width = width;
    m_height = height;
    m_data.resize(width * height);

    for (uint32_t z = 0; z < height; ++z) {
        for (uint32_t x = 0; x < width; ++x) {
            float u = static_cast<float>(x) / (width - 1);
            float v = static_cast<float>(z) / (height - 1);
            m_data[z * width + x] = height_func(u, v);
        }
    }

    recalculate_bounds();
}

float Heightmap::sample(float u, float v, HeightmapFilter filter) const {
    if (!is_valid()) return 0.0f;

    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    switch (filter) {
        case HeightmapFilter::Nearest: return sample_nearest(u, v);
        case HeightmapFilter::Bilinear: return sample_bilinear(u, v);
        case HeightmapFilter::Bicubic: return sample_bicubic(u, v);
    }

    return sample_bilinear(u, v);
}

float Heightmap::sample_world(float x, float z, const Vec3& terrain_scale) const {
    if (terrain_scale.x <= 0.0f || terrain_scale.z <= 0.0f) return 0.0f;

    float u = x / terrain_scale.x;
    float v = z / terrain_scale.z;

    return sample(u, v) * terrain_scale.y;
}

float Heightmap::get_height(uint32_t x, uint32_t y) const {
    if (x >= m_width || y >= m_height) return 0.0f;
    return m_data[y * m_width + x];
}

void Heightmap::set_height(uint32_t x, uint32_t y, float height) {
    if (x >= m_width || y >= m_height) return;
    m_data[y * m_width + x] = height;
}

Vec3 Heightmap::calculate_normal(float u, float v, float terrain_scale_xz, float terrain_scale_y) const {
    if (!is_valid()) return Vec3(0.0f, 1.0f, 0.0f);

    float step = 1.0f / std::max(m_width, m_height);

    float h_left = sample(u - step, v);
    float h_right = sample(u + step, v);
    float h_down = sample(u, v - step);
    float h_up = sample(u, v + step);

    float dx = (h_right - h_left) * terrain_scale_y / (2.0f * step * terrain_scale_xz);
    float dz = (h_up - h_down) * terrain_scale_y / (2.0f * step * terrain_scale_xz);

    return normalize(Vec3(-dx, 1.0f, -dz));
}

Vec3 Heightmap::calculate_normal_world(float x, float z, const Vec3& terrain_scale) const {
    float u = x / terrain_scale.x;
    float v = z / terrain_scale.z;
    return calculate_normal(u, v, terrain_scale.x, terrain_scale.y);
}

void Heightmap::smooth(uint32_t iterations) {
    if (!is_valid()) return;

    std::vector<float> temp(m_data.size());

    for (uint32_t iter = 0; iter < iterations; ++iter) {
        for (uint32_t z = 0; z < m_height; ++z) {
            for (uint32_t x = 0; x < m_width; ++x) {
                float sum = 0.0f;
                int count = 0;

                for (int dz = -1; dz <= 1; ++dz) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int nx = static_cast<int>(x) + dx;
                        int nz = static_cast<int>(z) + dz;

                        if (nx >= 0 && nx < static_cast<int>(m_width) &&
                            nz >= 0 && nz < static_cast<int>(m_height)) {
                            sum += m_data[nz * m_width + nx];
                            count++;
                        }
                    }
                }

                temp[z * m_width + x] = sum / count;
            }
        }

        std::swap(m_data, temp);
    }

    recalculate_bounds();
}

void Heightmap::normalize(float min_height, float max_height) {
    if (!is_valid() || m_max_height <= m_min_height) return;

    float range = m_max_height - m_min_height;
    float target_range = max_height - min_height;

    for (float& h : m_data) {
        h = ((h - m_min_height) / range) * target_range + min_height;
    }

    m_min_height = min_height;
    m_max_height = max_height;
}

void Heightmap::apply_curve(std::function<float(float)> curve) {
    for (float& h : m_data) {
        h = curve(h);
    }
    recalculate_bounds();
}

void Heightmap::blend(const Heightmap& other, float weight) {
    if (!is_valid() || !other.is_valid()) return;
    if (m_width != other.m_width || m_height != other.m_height) return;

    for (size_t i = 0; i < m_data.size(); ++i) {
        m_data[i] = m_data[i] * (1.0f - weight) + other.m_data[i] * weight;
    }

    recalculate_bounds();
}

void Heightmap::add_noise(float frequency, float amplitude) {
    if (!is_valid()) return;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> offset_dist(0.0f, 1000.0f);

    float offset_x = offset_dist(gen);
    float offset_z = offset_dist(gen);

    for (uint32_t z = 0; z < m_height; ++z) {
        for (uint32_t x = 0; x < m_width; ++x) {
            float nx = (x + offset_x) * frequency;
            float nz = (z + offset_z) * frequency;

            float noise = std::sin(nx) * std::sin(nz);
            m_data[z * m_width + x] += noise * amplitude;
        }
    }

    recalculate_bounds();
}

void Heightmap::erode_thermal(uint32_t iterations, float talus_angle) {
    if (!is_valid()) return;

    for (uint32_t iter = 0; iter < iterations; ++iter) {
        for (uint32_t z = 1; z < m_height - 1; ++z) {
            for (uint32_t x = 1; x < m_width - 1; ++x) {
                float h = m_data[z * m_width + x];
                float max_diff = 0.0f;
                int max_dx = 0, max_dz = 0;

                // Find steepest neighbor
                for (int dz = -1; dz <= 1; ++dz) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dz == 0) continue;

                        float nh = m_data[(z + dz) * m_width + (x + dx)];
                        float diff = h - nh;

                        if (diff > max_diff) {
                            max_diff = diff;
                            max_dx = dx;
                            max_dz = dz;
                        }
                    }
                }

                // Move material if slope exceeds talus angle
                if (max_diff > talus_angle) {
                    float transfer = (max_diff - talus_angle) * 0.5f;
                    m_data[z * m_width + x] -= transfer;
                    m_data[(z + max_dz) * m_width + (x + max_dx)] += transfer;
                }
            }
        }
    }

    recalculate_bounds();
}

void Heightmap::erode_hydraulic(uint32_t iterations, float rain_amount,
                                 float evaporation, float sediment_capacity) {
    if (!is_valid()) return;

    std::vector<float> water(m_data.size(), 0.0f);
    std::vector<float> sediment(m_data.size(), 0.0f);

    for (uint32_t iter = 0; iter < iterations; ++iter) {
        // Add rain
        for (float& w : water) {
            w += rain_amount;
        }

        // Flow water downhill
        for (uint32_t z = 1; z < m_height - 1; ++z) {
            for (uint32_t x = 1; x < m_width - 1; ++x) {
                size_t idx = z * m_width + x;
                float h = m_data[idx] + water[idx];

                // Find lowest neighbor
                float min_h = h;
                int min_dx = 0, min_dz = 0;

                for (int dz = -1; dz <= 1; ++dz) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dz == 0) continue;

                        size_t nidx = (z + dz) * m_width + (x + dx);
                        float nh = m_data[nidx] + water[nidx];

                        if (nh < min_h) {
                            min_h = nh;
                            min_dx = dx;
                            min_dz = dz;
                        }
                    }
                }

                // Transfer water and sediment
                if (min_h < h) {
                    float flow = std::min(water[idx], (h - min_h) * 0.5f);
                    size_t nidx = (z + min_dz) * m_width + (x + min_dx);

                    water[idx] -= flow;
                    water[nidx] += flow;

                    // Erode based on water velocity
                    float erosion = flow * sediment_capacity;
                    if (sediment[idx] < erosion) {
                        float diff = erosion - sediment[idx];
                        m_data[idx] -= diff;
                        sediment[idx] += diff;
                    }

                    // Transfer sediment with water
                    float sed_transfer = sediment[idx] * (flow / (water[idx] + flow + 0.001f));
                    sediment[idx] -= sed_transfer;
                    sediment[nidx] += sed_transfer;
                }
            }
        }

        // Evaporate water and deposit sediment
        for (size_t i = 0; i < m_data.size(); ++i) {
            water[i] *= (1.0f - evaporation);

            // Deposit excess sediment
            float max_sed = water[i] * sediment_capacity;
            if (sediment[i] > max_sed) {
                float deposit = sediment[i] - max_sed;
                m_data[i] += deposit;
                sediment[i] = max_sed;
            }
        }
    }

    recalculate_bounds();
}

bool Heightmap::save_to_file(const std::string& path, HeightmapFormat format) const {
    return save_raw(path);
}

bool Heightmap::save_raw(const std::string& path) const {
    if (!is_valid()) return false;

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    // Save as 16-bit raw
    std::vector<uint16_t> data16(m_data.size());
    for (size_t i = 0; i < m_data.size(); ++i) {
        data16[i] = static_cast<uint16_t>(std::clamp(m_data[i], 0.0f, 1.0f) * 65535.0f);
    }

    file.write(reinterpret_cast<const char*>(data16.data()), data16.size() * sizeof(uint16_t));
    return true;
}

void Heightmap::recalculate_bounds() {
    if (m_data.empty()) {
        m_min_height = 0.0f;
        m_max_height = 0.0f;
        return;
    }

    m_min_height = m_data[0];
    m_max_height = m_data[0];

    for (float h : m_data) {
        m_min_height = std::min(m_min_height, h);
        m_max_height = std::max(m_max_height, h);
    }
}

float Heightmap::sample_nearest(float u, float v) const {
    uint32_t x = static_cast<uint32_t>(u * (m_width - 1) + 0.5f);
    uint32_t y = static_cast<uint32_t>(v * (m_height - 1) + 0.5f);
    x = std::min(x, m_width - 1);
    y = std::min(y, m_height - 1);
    return m_data[y * m_width + x];
}

float Heightmap::sample_bilinear(float u, float v) const {
    float fx = u * (m_width - 1);
    float fy = v * (m_height - 1);

    uint32_t x0 = static_cast<uint32_t>(fx);
    uint32_t y0 = static_cast<uint32_t>(fy);
    uint32_t x1 = std::min(x0 + 1, m_width - 1);
    uint32_t y1 = std::min(y0 + 1, m_height - 1);

    float tx = fx - x0;
    float ty = fy - y0;

    float h00 = m_data[y0 * m_width + x0];
    float h10 = m_data[y0 * m_width + x1];
    float h01 = m_data[y1 * m_width + x0];
    float h11 = m_data[y1 * m_width + x1];

    float h0 = h00 + (h10 - h00) * tx;
    float h1 = h01 + (h11 - h01) * tx;

    return h0 + (h1 - h0) * ty;
}

float Heightmap::sample_bicubic(float u, float v) const {
    float fx = u * (m_width - 1);
    float fy = v * (m_height - 1);

    int x1 = static_cast<int>(fx);
    int y1 = static_cast<int>(fy);

    float tx = fx - x1;
    float ty = fy - y1;

    float rows[4];
    for (int j = -1; j <= 2; ++j) {
        int y = std::clamp(y1 + j, 0, static_cast<int>(m_height) - 1);

        float p0 = m_data[y * m_width + std::clamp(x1 - 1, 0, static_cast<int>(m_width) - 1)];
        float p1 = m_data[y * m_width + std::clamp(x1, 0, static_cast<int>(m_width) - 1)];
        float p2 = m_data[y * m_width + std::clamp(x1 + 1, 0, static_cast<int>(m_width) - 1)];
        float p3 = m_data[y * m_width + std::clamp(x1 + 2, 0, static_cast<int>(m_width) - 1)];

        rows[j + 1] = cubic_interpolate(p0, p1, p2, p3, tx);
    }

    return cubic_interpolate(rows[0], rows[1], rows[2], rows[3], ty);
}

float Heightmap::cubic_interpolate(float p0, float p1, float p2, float p3, float t) const {
    float a = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
    float b = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
    float c = -0.5f * p0 + 0.5f * p2;
    float d = p1;

    return a * t * t * t + b * t * t + c * t + d;
}

// SplatMap implementation

bool SplatMap::load_from_file(const std::string& path) {
    // Would use image loading library
    return false;
}

bool SplatMap::load_from_memory(const void* data, uint32_t width, uint32_t height, uint32_t channels) {
    if (!data || width == 0 || height == 0 || channels == 0) return false;

    m_width = width;
    m_height = height;
    m_channels = channels;
    m_data.resize(width * height * channels);

    const uint8_t* src = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < m_data.size(); ++i) {
        m_data[i] = src[i] / 255.0f;
    }

    return true;
}

void SplatMap::generate_from_heightmap(const Heightmap& heightmap, uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
    m_channels = 4;
    m_data.resize(width * height * 4, 0.0f);

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            float u = static_cast<float>(x) / (width - 1);
            float v = static_cast<float>(y) / (height - 1);
            float h = heightmap.sample(u, v);

            size_t idx = (y * width + x) * 4;

            // Simple height-based layering
            if (h < 0.3f) {
                m_data[idx + 0] = 1.0f;  // Layer 0 (e.g., sand/dirt)
            } else if (h < 0.6f) {
                m_data[idx + 1] = 1.0f;  // Layer 1 (e.g., grass)
            } else if (h < 0.8f) {
                m_data[idx + 2] = 1.0f;  // Layer 2 (e.g., rock)
            } else {
                m_data[idx + 3] = 1.0f;  // Layer 3 (e.g., snow)
            }
        }
    }
}

void SplatMap::generate_from_slope(const Heightmap& heightmap, const Vec3& terrain_scale,
                                    float grass_max_slope, float rock_min_slope) {
    if (!heightmap.is_valid()) return;

    uint32_t width = heightmap.get_width();
    uint32_t height = heightmap.get_height();

    m_width = width;
    m_height = height;
    m_channels = 4;
    m_data.resize(width * height * 4, 0.0f);

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            float u = static_cast<float>(x) / (width - 1);
            float v = static_cast<float>(y) / (height - 1);

            Vec3 normal = heightmap.calculate_normal(u, v, terrain_scale.x, terrain_scale.y);
            float slope = 1.0f - normal.y;  // 0 = flat, 1 = vertical

            size_t idx = (y * width + x) * 4;

            if (slope < grass_max_slope) {
                m_data[idx + 1] = 1.0f;  // Grass
            } else if (slope > rock_min_slope) {
                m_data[idx + 2] = 1.0f;  // Rock
            } else {
                // Blend
                float t = (slope - grass_max_slope) / (rock_min_slope - grass_max_slope);
                m_data[idx + 1] = 1.0f - t;
                m_data[idx + 2] = t;
            }
        }
    }
}

Vec4 SplatMap::sample(float u, float v) const {
    if (!is_valid()) return Vec4(1.0f, 0.0f, 0.0f, 0.0f);

    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    float fx = u * (m_width - 1);
    float fy = v * (m_height - 1);

    uint32_t x = static_cast<uint32_t>(fx);
    uint32_t y = static_cast<uint32_t>(fy);

    x = std::min(x, m_width - 1);
    y = std::min(y, m_height - 1);

    size_t idx = (y * m_width + x) * m_channels;

    Vec4 result(0.0f);
    for (uint32_t c = 0; c < std::min(m_channels, 4u); ++c) {
        (&result.x)[c] = m_data[idx + c];
    }

    return result;
}

void SplatMap::get_weights_at(float u, float v, float* out_weights, uint32_t count) const {
    Vec4 sample_result = sample(u, v);
    for (uint32_t i = 0; i < count && i < 4; ++i) {
        out_weights[i] = (&sample_result.x)[i];
    }
    for (uint32_t i = 4; i < count; ++i) {
        out_weights[i] = 0.0f;
    }
}

void SplatMap::set_weight(uint32_t x, uint32_t y, uint32_t channel, float weight) {
    if (x >= m_width || y >= m_height || channel >= m_channels) return;
    m_data[(y * m_width + x) * m_channels + channel] = weight;
}

float SplatMap::get_weight(uint32_t x, uint32_t y, uint32_t channel) const {
    if (x >= m_width || y >= m_height || channel >= m_channels) return 0.0f;
    return m_data[(y * m_width + x) * m_channels + channel];
}

void SplatMap::normalize_weights() {
    for (uint32_t y = 0; y < m_height; ++y) {
        for (uint32_t x = 0; x < m_width; ++x) {
            size_t idx = (y * m_width + x) * m_channels;

            float sum = 0.0f;
            for (uint32_t c = 0; c < m_channels; ++c) {
                sum += m_data[idx + c];
            }

            if (sum > 0.0f) {
                for (uint32_t c = 0; c < m_channels; ++c) {
                    m_data[idx + c] /= sum;
                }
            }
        }
    }
}

void SplatMap::paint(float u, float v, uint32_t channel, float strength, float radius, float falloff) {
    if (channel >= m_channels) return;

    float cx = u * m_width;
    float cy = v * m_height;
    float pixel_radius = radius * std::max(m_width, m_height);

    int min_x = std::max(0, static_cast<int>(cx - pixel_radius));
    int max_x = std::min(static_cast<int>(m_width) - 1, static_cast<int>(cx + pixel_radius));
    int min_y = std::max(0, static_cast<int>(cy - pixel_radius));
    int max_y = std::min(static_cast<int>(m_height) - 1, static_cast<int>(cy + pixel_radius));

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            float dx = x - cx;
            float dy = y - cy;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist <= pixel_radius) {
                float t = dist / pixel_radius;
                float weight = 1.0f - std::pow(t, 1.0f / falloff);
                weight *= strength;

                size_t idx = (y * m_width + x) * m_channels;
                m_data[idx + channel] = std::min(1.0f, m_data[idx + channel] + weight);
            }
        }
    }

    normalize_weights();
}

bool SplatMap::save_to_file(const std::string& path) const {
    // Would use image saving library
    return false;
}

// HoleMap implementation

bool HoleMap::load_from_file(const std::string& path) {
    return false;
}

void HoleMap::generate(uint32_t width, uint32_t height, bool fill_value) {
    m_width = width;
    m_height = height;
    m_data.resize(width * height, fill_value);
}

bool HoleMap::is_hole(float u, float v) const {
    if (m_data.empty()) return false;

    uint32_t x = static_cast<uint32_t>(u * (m_width - 1));
    uint32_t y = static_cast<uint32_t>(v * (m_height - 1));

    return is_hole_at(x, y);
}

bool HoleMap::is_hole_at(uint32_t x, uint32_t y) const {
    if (x >= m_width || y >= m_height) return false;
    return m_data[y * m_width + x];
}

void HoleMap::set_hole(uint32_t x, uint32_t y, bool is_hole) {
    if (x >= m_width || y >= m_height) return;
    m_data[y * m_width + x] = is_hole;
}

} // namespace engine::terrain
