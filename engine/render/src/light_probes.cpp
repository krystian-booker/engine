#include <engine/render/light_probes.hpp>
#include <algorithm>
#include <cmath>

namespace engine::render {

// Global instance
static LightProbeSystem* s_light_probe_system = nullptr;

LightProbeSystem& get_light_probe_system() {
    if (!s_light_probe_system) {
        static LightProbeSystem instance;
        s_light_probe_system = &instance;
    }
    return *s_light_probe_system;
}

// LightProbe implementation
Vec3 LightProbe::sample_irradiance(const Vec3& normal) const {
    if (!valid) return Vec3(0.0f);
    return LightProbeUtils::evaluate_sh(sh_coefficients, normal);
}

// LightProbeVolume implementation
LightProbe* LightProbeVolume::get_probe(uint32_t x, uint32_t y, uint32_t z) {
    uint32_t index = get_probe_index(x, y, z);
    if (index >= probes.size()) return nullptr;
    return &probes[index];
}

const LightProbe* LightProbeVolume::get_probe(uint32_t x, uint32_t y, uint32_t z) const {
    uint32_t index = get_probe_index(x, y, z);
    if (index >= probes.size()) return nullptr;
    return &probes[index];
}

uint32_t LightProbeVolume::get_probe_index(uint32_t x, uint32_t y, uint32_t z) const {
    return z * (resolution_x * resolution_y) + y * resolution_x + x;
}

bool LightProbeVolume::get_grid_position(const Vec3& world_pos,
                                          uint32_t& x, uint32_t& y, uint32_t& z) const {
    if (!contains(world_pos)) return false;

    Vec3 cell_size = get_cell_size();
    Vec3 local_pos = world_pos - min_bounds;

    x = std::min(static_cast<uint32_t>(local_pos.x / cell_size.x), resolution_x - 1);
    y = std::min(static_cast<uint32_t>(local_pos.y / cell_size.y), resolution_y - 1);
    z = std::min(static_cast<uint32_t>(local_pos.z / cell_size.z), resolution_z - 1);

    return true;
}

Vec3 LightProbeVolume::sample_irradiance(const Vec3& world_pos, const Vec3& normal) const {
    if (!enabled || probes.empty()) return Vec3(0.0f);
    if (!contains(world_pos)) return Vec3(0.0f);

    Vec3 cell_size = get_cell_size();
    Vec3 local_pos = world_pos - min_bounds;

    // Get cell position
    float fx = local_pos.x / cell_size.x;
    float fy = local_pos.y / cell_size.y;
    float fz = local_pos.z / cell_size.z;

    uint32_t x0 = std::min(static_cast<uint32_t>(fx), resolution_x - 1);
    uint32_t y0 = std::min(static_cast<uint32_t>(fy), resolution_y - 1);
    uint32_t z0 = std::min(static_cast<uint32_t>(fz), resolution_z - 1);
    uint32_t x1 = std::min(x0 + 1, resolution_x - 1);
    uint32_t y1 = std::min(y0 + 1, resolution_y - 1);
    uint32_t z1 = std::min(z0 + 1, resolution_z - 1);

    // Interpolation weights
    float wx = fx - x0;
    float wy = fy - y0;
    float wz = fz - z0;

    // Sample 8 surrounding probes
    Vec3 samples[8];
    samples[0] = get_probe(x0, y0, z0) ? get_probe(x0, y0, z0)->sample_irradiance(normal) : Vec3(0.0f);
    samples[1] = get_probe(x1, y0, z0) ? get_probe(x1, y0, z0)->sample_irradiance(normal) : Vec3(0.0f);
    samples[2] = get_probe(x0, y1, z0) ? get_probe(x0, y1, z0)->sample_irradiance(normal) : Vec3(0.0f);
    samples[3] = get_probe(x1, y1, z0) ? get_probe(x1, y1, z0)->sample_irradiance(normal) : Vec3(0.0f);
    samples[4] = get_probe(x0, y0, z1) ? get_probe(x0, y0, z1)->sample_irradiance(normal) : Vec3(0.0f);
    samples[5] = get_probe(x1, y0, z1) ? get_probe(x1, y0, z1)->sample_irradiance(normal) : Vec3(0.0f);
    samples[6] = get_probe(x0, y1, z1) ? get_probe(x0, y1, z1)->sample_irradiance(normal) : Vec3(0.0f);
    samples[7] = get_probe(x1, y1, z1) ? get_probe(x1, y1, z1)->sample_irradiance(normal) : Vec3(0.0f);

    // Trilinear interpolation
    Vec3 s00 = samples[0] * (1.0f - wx) + samples[1] * wx;
    Vec3 s01 = samples[2] * (1.0f - wx) + samples[3] * wx;
    Vec3 s10 = samples[4] * (1.0f - wx) + samples[5] * wx;
    Vec3 s11 = samples[6] * (1.0f - wx) + samples[7] * wx;

    Vec3 s0 = s00 * (1.0f - wy) + s01 * wy;
    Vec3 s1 = s10 * (1.0f - wy) + s11 * wy;

    return s0 * (1.0f - wz) + s1 * wz;
}

void LightProbeVolume::initialize() {
    uint32_t count = get_probe_count();
    probes.resize(count);

    Vec3 cell_size = get_cell_size();

    for (uint32_t z = 0; z < resolution_z; ++z) {
        for (uint32_t y = 0; y < resolution_y; ++y) {
            for (uint32_t x = 0; x < resolution_x; ++x) {
                uint32_t index = get_probe_index(x, y, z);
                LightProbe& probe = probes[index];

                probe.position = min_bounds + Vec3(
                    (x + 0.5f) * cell_size.x,
                    (y + 0.5f) * cell_size.y,
                    (z + 0.5f) * cell_size.z
                );
                probe.radius = length(cell_size) * 0.5f;
                probe.index = index;
                probe.valid = false;
                probe.needs_update = true;
            }
        }
    }
}

void LightProbeVolume::invalidate() {
    for (auto& probe : probes) {
        probe.needs_update = true;
    }
}

// LightProbeSystem implementation
LightProbeSystem::~LightProbeSystem() {
    if (m_initialized) {
        shutdown();
    }
}

void LightProbeSystem::init(const LightProbeSystemConfig& config) {
    if (m_initialized) return;

    m_config = config;

    m_volumes.resize(m_config.max_volumes);
    m_volume_used.resize(m_config.max_volumes, false);

    // Create uniforms
    u_probe_params = bgfx::createUniform("u_probeParams", bgfx::UniformType::Vec4);
    s_probes = bgfx::createUniform("s_probes", bgfx::UniformType::Sampler);

    // Initialize sky SH with default color
    m_sky_sh = LightProbeUtils::create_ambient_sh(m_sky_color);

    m_initialized = true;
}

void LightProbeSystem::shutdown() {
    if (!m_initialized) return;

    if (bgfx::isValid(m_probe_texture)) {
        bgfx::destroy(m_probe_texture);
        m_probe_texture = BGFX_INVALID_HANDLE;
    }
    m_probe_texture_width = 0;
    m_probe_texture_height = 0;

    if (bgfx::isValid(u_probe_params)) bgfx::destroy(u_probe_params);
    if (bgfx::isValid(s_probes)) bgfx::destroy(s_probes);

    m_volumes.clear();
    m_volume_used.clear();

    m_initialized = false;
}

LightProbeVolumeHandle LightProbeSystem::create_volume(const Vec3& min_bounds,
                                                        const Vec3& max_bounds,
                                                        uint32_t res_x, uint32_t res_y, uint32_t res_z) {
    // Find free slot
    for (uint32_t i = 0; i < m_config.max_volumes; ++i) {
        if (!m_volume_used[i]) {
            LightProbeVolume& volume = m_volumes[i];
            volume.min_bounds = min_bounds;
            volume.max_bounds = max_bounds;
            volume.resolution_x = res_x;
            volume.resolution_y = res_y;
            volume.resolution_z = res_z;
            volume.initialize();

            m_volume_used[i] = true;
            m_stats.total_volumes++;
            m_stats.total_probes += volume.get_probe_count();
            m_stats.probes_pending += volume.get_probe_count();

            return i;
        }
    }

    return INVALID_PROBE_VOLUME;
}

void LightProbeSystem::destroy_volume(LightProbeVolumeHandle handle) {
    if (handle >= m_config.max_volumes || !m_volume_used[handle]) {
        return;
    }

    m_stats.total_probes -= m_volumes[handle].get_probe_count();
    m_stats.total_volumes--;

    m_volumes[handle] = LightProbeVolume{};
    m_volume_used[handle] = false;
}

LightProbeVolume* LightProbeSystem::get_volume(LightProbeVolumeHandle handle) {
    if (handle >= m_config.max_volumes || !m_volume_used[handle]) {
        return nullptr;
    }
    return &m_volumes[handle];
}

const LightProbeVolume* LightProbeSystem::get_volume(LightProbeVolumeHandle handle) const {
    if (handle >= m_config.max_volumes || !m_volume_used[handle]) {
        return nullptr;
    }
    return &m_volumes[handle];
}

void LightProbeSystem::bake_all(const LightProbeBakeSettings& settings,
                                  ProbeRayCallback ray_callback) {
    for (uint32_t i = 0; i < m_config.max_volumes; ++i) {
        if (m_volume_used[i]) {
            bake_volume(i, settings, ray_callback);
        }
    }
}

void LightProbeSystem::bake_volume(LightProbeVolumeHandle handle,
                                    const LightProbeBakeSettings& settings,
                                    ProbeRayCallback ray_callback) {
    LightProbeVolume* volume = get_volume(handle);
    if (!volume) return;

    for (auto& probe : volume->probes) {
        bake_probe(probe, settings, ray_callback);
    }
}

void LightProbeSystem::bake_incremental(const LightProbeBakeSettings& settings,
                                          ProbeRayCallback ray_callback) {
    uint32_t probes_baked = 0;

    while (probes_baked < m_config.probes_per_frame) {
        // Find next volume with pending probes
        bool found = false;
        for (uint32_t i = 0; i < m_config.max_volumes; ++i) {
            uint32_t vol_idx = (m_bake_volume_index + i) % m_config.max_volumes;

            if (!m_volume_used[vol_idx]) continue;

            LightProbeVolume& volume = m_volumes[vol_idx];

            // Find next probe needing update
            for (uint32_t j = 0; j < volume.probes.size(); ++j) {
                uint32_t probe_idx = (m_bake_probe_index + j) % volume.probes.size();
                LightProbe& probe = volume.probes[probe_idx];

                if (probe.needs_update) {
                    bake_probe(probe, settings, ray_callback);
                    probes_baked++;
                    m_bake_probe_index = (probe_idx + 1) % volume.probes.size();
                    found = true;
                    break;
                }
            }

            if (found) break;

            // Move to next volume
            m_bake_volume_index = (m_bake_volume_index + 1) % m_config.max_volumes;
            m_bake_probe_index = 0;
        }

        if (!found) break;  // No more probes to bake
    }
}

void LightProbeSystem::bake_probe(LightProbe& probe,
                                   const LightProbeBakeSettings& settings,
                                   ProbeRayCallback ray_callback) {
    probe.clear();

    // Generate sphere samples
    std::vector<Vec3> directions;
    LightProbeUtils::generate_sphere_samples(directions, settings.samples_per_probe);

    // Collect radiance samples
    std::vector<std::pair<Vec3, Vec3>> samples;
    samples.reserve(settings.samples_per_probe);

    for (const Vec3& dir : directions) {
        ProbeRayHit hit = ray_callback(probe.position, dir);

        Vec3 radiance;
        if (hit.hit) {
            radiance = hit.color;
        } else if (settings.include_sky) {
            // Sample sky
            radiance = LightProbeUtils::evaluate_sh(m_sky_sh, dir);
        } else {
            radiance = Vec3(0.0f);
        }

        radiance = radiance * settings.intensity_multiplier;
        samples.push_back({dir, radiance});
    }

    // Project to SH
    project_to_sh(samples, probe.sh_coefficients);

    // Convolve with cosine lobe for irradiance
    LightProbeUtils::convolve_cosine(probe.sh_coefficients);

    probe.valid = true;
    probe.needs_update = false;

    m_stats.probes_baked++;
    if (m_stats.probes_pending > 0) {
        m_stats.probes_pending--;
    }
}

void LightProbeSystem::project_to_sh(const std::vector<std::pair<Vec3, Vec3>>& samples,
                                       SHCoefficientsRGB& out_sh) {
    out_sh = SHCoefficientsRGB();

    float weight = 4.0f * 3.14159f / samples.size();  // Solid angle per sample

    for (const auto& [dir, color] : samples) {
        LightProbeUtils::add_sample_to_sh(dir, color, weight, out_sh);
    }
}

Vec3 LightProbeSystem::sample_irradiance(const Vec3& world_pos, const Vec3& normal) const {
    // Find all affecting volumes
    std::vector<std::pair<float, const LightProbeVolume*>> affecting;

    for (uint32_t i = 0; i < m_config.max_volumes; ++i) {
        if (!m_volume_used[i]) continue;

        const LightProbeVolume& volume = m_volumes[i];
        if (!volume.enabled) continue;

        if (volume.contains(world_pos)) {
            affecting.push_back({static_cast<float>(volume.priority), &volume});
        }
    }

    if (affecting.empty()) {
        // Fallback to sky
        return LightProbeUtils::evaluate_sh(m_sky_sh, normal);
    }

    // Sort by priority
    std::sort(affecting.begin(), affecting.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    // Use highest priority volume
    return affecting[0].second->sample_irradiance(world_pos, normal);
}

std::vector<LightProbeVolumeHandle> LightProbeSystem::get_volumes_at(const Vec3& pos) const {
    std::vector<LightProbeVolumeHandle> result;

    for (uint32_t i = 0; i < m_config.max_volumes; ++i) {
        if (m_volume_used[i] && m_volumes[i].contains(pos)) {
            result.push_back(i);
        }
    }

    return result;
}

void LightProbeSystem::upload_to_gpu() {
    if (!m_initialized) {
        return;
    }

    // Calculate total probe count
    uint32_t total_probes = 0;
    for (uint32_t i = 0; i < m_config.max_volumes; ++i) {
        if (m_volume_used[i]) {
            total_probes += m_volumes[i].get_probe_count();
        }
    }

    if (total_probes == 0) {
        if (bgfx::isValid(m_probe_texture)) {
            bgfx::destroy(m_probe_texture);
            m_probe_texture = BGFX_INVALID_HANDLE;
        }
        m_probe_texture_width = 0;
        m_probe_texture_height = 0;
        return;
    }

    const uint32_t pixels_per_probe = LIGHT_PROBE_TEXTURE_PIXELS_PER_PROBE;
    uint32_t texture_width = 128;
    uint32_t texture_height = (total_probes * pixels_per_probe + texture_width - 1) / texture_width;

    // Destroy old texture if exists
    if (bgfx::isValid(m_probe_texture)) {
        bgfx::destroy(m_probe_texture);
    }

    m_probe_texture_width = texture_width;
    m_probe_texture_height = texture_height;

    // Pack probe data
    std::vector<float> data(texture_width * texture_height * 4, 0.0f);

    uint32_t probe_offset = 0;
    for (uint32_t v = 0; v < m_config.max_volumes; ++v) {
        if (!m_volume_used[v]) continue;

        LightProbeVolume& volume = m_volumes[v];
        volume.gpu_base_probe_index = probe_offset;

        for (const auto& probe : volume.probes) {
            const uint32_t pixel_offset = probe_offset * pixels_per_probe;
            for (uint32_t coeff_idx = 0; coeff_idx < SH_COEFFICIENT_COUNT; ++coeff_idx) {
                const uint32_t tex_idx = (pixel_offset + coeff_idx) * 4;
                if (tex_idx + 3 >= data.size()) {
                    break;
                }

                if (probe.valid) {
                    data[tex_idx + 0] = probe.sh_coefficients.r[coeff_idx];
                    data[tex_idx + 1] = probe.sh_coefficients.g[coeff_idx];
                    data[tex_idx + 2] = probe.sh_coefficients.b[coeff_idx];
                    data[tex_idx + 3] = 1.0f;
                }
            }

            probe_offset++;
        }
    }

    const bgfx::Memory* mem = bgfx::copy(data.data(), static_cast<uint32_t>(data.size() * sizeof(float)));
    m_probe_texture = bgfx::createTexture2D(
        texture_width,
        texture_height,
        false,
        1,
        bgfx::TextureFormat::RGBA32F,
        BGFX_TEXTURE_NONE | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        mem
    );
}

bool LightProbeSystem::volume_has_gpu_data(const LightProbeVolume& volume) const {
    if (!volume.enabled || volume.probes.empty()) {
        return false;
    }

    return std::any_of(volume.probes.begin(), volume.probes.end(),
        [](const LightProbe& probe) {
            return probe.valid;
        });
}

bool LightProbeSystem::get_primary_volume_shader_data(LightProbeVolumeShaderData& out) const {
    out = LightProbeVolumeShaderData{};

    if (!m_initialized || !bgfx::isValid(m_probe_texture)) {
        return false;
    }

    const LightProbeVolume* best_volume = nullptr;
    for (uint32_t i = 0; i < m_config.max_volumes; ++i) {
        if (!m_volume_used[i]) {
            continue;
        }

        const LightProbeVolume& volume = m_volumes[i];
        if (!volume_has_gpu_data(volume)) {
            continue;
        }

        if (!best_volume || volume.priority > best_volume->priority) {
            best_volume = &volume;
        }
    }

    if (!best_volume) {
        return false;
    }

    out.min_bounds = best_volume->min_bounds;
    out.max_bounds = best_volume->max_bounds;
    out.resolution = UVec3(best_volume->resolution_x, best_volume->resolution_y, best_volume->resolution_z);
    out.base_probe_index = best_volume->gpu_base_probe_index;
    out.valid = true;
    return true;
}

void LightProbeSystem::debug_draw_probes(const Mat4& /*view_proj*/) {
    // Would use debug draw system to visualize probes
}

void LightProbeSystem::debug_draw_volume(LightProbeVolumeHandle /*handle*/, const Mat4& /*view_proj*/) {
    // Would use debug draw system to visualize volume bounds
}

} // namespace engine::render
