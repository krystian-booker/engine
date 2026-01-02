#include <engine/render/reflection_probes.hpp>
#include <algorithm>
#include <cmath>

namespace engine::render {

// Global instance
static ReflectionProbeSystem* s_reflection_probe_system = nullptr;

ReflectionProbeSystem& get_reflection_probe_system() {
    if (!s_reflection_probe_system) {
        static ReflectionProbeSystem instance;
        s_reflection_probe_system = &instance;
    }
    return *s_reflection_probe_system;
}

ReflectionProbeSystem::~ReflectionProbeSystem() {
    if (m_initialized) {
        shutdown();
    }
}

void ReflectionProbeSystem::init(const ReflectionProbeConfig& config) {
    if (m_initialized) return;

    m_config = config;

    m_probes.resize(64);  // Default max probes
    m_probe_used.resize(64, false);

    // Create capture framebuffer
    m_capture_color = bgfx::createTexture2D(
        config.resolution, config.resolution, false, 1,
        config.hdr ? bgfx::TextureFormat::RGBA16F : bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_RT
    );

    m_capture_depth = bgfx::createTexture2D(
        config.resolution, config.resolution, false, 1,
        bgfx::TextureFormat::D24S8,
        BGFX_TEXTURE_RT
    );

    bgfx::TextureHandle fb_textures[] = { m_capture_color, m_capture_depth };
    m_capture_fb = bgfx::createFrameBuffer(2, fb_textures, true);

    // Create uniforms
    u_probe_data = bgfx::createUniform("u_probeData", bgfx::UniformType::Vec4, 4);
    s_environment = bgfx::createUniform("s_environment", bgfx::UniformType::Sampler);
    s_probe_array = bgfx::createUniform("s_probeArray", bgfx::UniformType::Sampler);

    m_initialized = true;
}

void ReflectionProbeSystem::shutdown() {
    if (!m_initialized) return;

    // Destroy probe cubemaps
    for (uint32_t i = 0; i < m_probes.size(); ++i) {
        if (m_probe_used[i] && bgfx::isValid(m_probes[i].cubemap)) {
            bgfx::destroy(m_probes[i].cubemap);
        }
    }

    if (bgfx::isValid(m_capture_fb)) bgfx::destroy(m_capture_fb);
    if (bgfx::isValid(m_capture_color)) bgfx::destroy(m_capture_color);
    if (bgfx::isValid(m_capture_depth)) bgfx::destroy(m_capture_depth);
    if (bgfx::isValid(m_prefilter_program)) bgfx::destroy(m_prefilter_program);

    if (bgfx::isValid(u_probe_data)) bgfx::destroy(u_probe_data);
    if (bgfx::isValid(s_environment)) bgfx::destroy(s_environment);
    if (bgfx::isValid(s_probe_array)) bgfx::destroy(s_probe_array);

    m_probes.clear();
    m_probe_used.clear();

    m_initialized = false;
}

ReflectionProbeHandle ReflectionProbeSystem::create_probe(const Vec3& position,
                                                           ReflectionProbeType type) {
    // Find free slot
    for (uint32_t i = 0; i < m_probes.size(); ++i) {
        if (!m_probe_used[i]) {
            ReflectionProbe& probe = m_probes[i];
            probe = ReflectionProbe{};
            probe.position = position;
            probe.type = type;
            probe.probe_id = m_next_probe_id++;
            probe.resolution = m_config.resolution;
            probe.mip_levels = m_config.mip_levels;

            // Create cubemap
            probe.cubemap = bgfx::createTextureCube(
                m_config.resolution, true, 1,
                m_config.hdr ? bgfx::TextureFormat::RGBA16F : bgfx::TextureFormat::RGBA8,
                BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_W_CLAMP
            );

            m_probe_used[i] = true;
            m_stats.total_probes++;

            if (type == ReflectionProbeType::Realtime) {
                m_stats.realtime_probes++;
            }

            return i;
        }
    }

    return INVALID_REFLECTION_PROBE;
}

void ReflectionProbeSystem::destroy_probe(ReflectionProbeHandle handle) {
    if (handle >= m_probes.size() || !m_probe_used[handle]) {
        return;
    }

    ReflectionProbe& probe = m_probes[handle];

    if (bgfx::isValid(probe.cubemap)) {
        bgfx::destroy(probe.cubemap);
    }

    if (probe.type == ReflectionProbeType::Realtime) {
        m_stats.realtime_probes--;
    }
    m_stats.total_probes--;

    m_probes[handle] = ReflectionProbe{};
    m_probe_used[handle] = false;
}

ReflectionProbe* ReflectionProbeSystem::get_probe(ReflectionProbeHandle handle) {
    if (handle >= m_probes.size() || !m_probe_used[handle]) {
        return nullptr;
    }
    return &m_probes[handle];
}

const ReflectionProbe* ReflectionProbeSystem::get_probe(ReflectionProbeHandle handle) const {
    if (handle >= m_probes.size() || !m_probe_used[handle]) {
        return nullptr;
    }
    return &m_probes[handle];
}

void ReflectionProbeSystem::set_probe_cubemap(ReflectionProbeHandle handle,
                                                bgfx::TextureHandle cubemap) {
    ReflectionProbe* probe = get_probe(handle);
    if (!probe) return;

    if (probe->type != ReflectionProbeType::Custom) {
        // Destroy existing cubemap if we own it
        if (bgfx::isValid(probe->cubemap)) {
            bgfx::destroy(probe->cubemap);
        }
    }

    probe->cubemap = cubemap;
    probe->type = ReflectionProbeType::Custom;
    probe->needs_update = false;
}

void ReflectionProbeSystem::bake_probe(ReflectionProbeHandle handle,
                                         ProbeRenderCallback render_callback) {
    ReflectionProbe* probe = get_probe(handle);
    if (!probe) return;

    // Capture all 6 faces
    for (uint32_t face = 0; face < 6; ++face) {
        capture_face(*probe, face, render_callback);
    }

    // Pre-filter for roughness levels
    if (m_config.prefilter_environment) {
        prefilter_cubemap(*probe);
    }

    probe->needs_update = false;
    probe->last_update_frame = m_frame_number;
}

void ReflectionProbeSystem::bake_all(ProbeRenderCallback render_callback) {
    for (uint32_t i = 0; i < m_probes.size(); ++i) {
        if (m_probe_used[i] && m_probes[i].type == ReflectionProbeType::Baked) {
            bake_probe(i, render_callback);
        }
    }
}

void ReflectionProbeSystem::update(uint32_t frame_number, ProbeRenderCallback render_callback) {
    m_frame_number = frame_number;
    m_stats.probes_updated_this_frame = 0;

    uint32_t realtime_updated = 0;

    for (uint32_t i = 0; i < m_probes.size() && realtime_updated < m_config.max_realtime_probes; ++i) {
        if (!m_probe_used[i]) continue;

        ReflectionProbe& probe = m_probes[i];

        if (probe.type != ReflectionProbeType::Realtime) continue;
        if (!probe.enabled) continue;

        // Check update interval
        if (m_config.update_interval > 0.0f) {
            // Would check time elapsed
        }

        if (m_config.update_one_face_per_frame) {
            // Update one face
            capture_face(probe, probe.next_face_to_update, render_callback);
            probe.next_face_to_update = (probe.next_face_to_update + 1) % 6;

            // Pre-filter after all faces updated
            if (probe.next_face_to_update == 0 && m_config.prefilter_environment) {
                prefilter_cubemap(probe);
            }
        } else {
            // Update all faces
            for (uint32_t face = 0; face < 6; ++face) {
                capture_face(probe, face, render_callback);
            }
            if (m_config.prefilter_environment) {
                prefilter_cubemap(probe);
            }
        }

        probe.last_update_frame = frame_number;
        realtime_updated++;
        m_stats.probes_updated_this_frame++;
    }
}

void ReflectionProbeSystem::capture_face(ReflectionProbe& probe, uint32_t face,
                                           ProbeRenderCallback render_callback) {
    Mat4 view = get_face_view_matrix(probe.position, face);
    Mat4 proj = get_face_projection_matrix();

    render_callback(view, proj, face);

    // Copy capture buffer to cubemap face
    // In practice, this would use bgfx::blit or render directly to cubemap face
}

void ReflectionProbeSystem::prefilter_cubemap(ReflectionProbe& /*probe*/) {
    // Would run compute shader or render passes to generate mip levels
    // with GGX importance sampling for roughness levels
}

Mat4 ReflectionProbeSystem::get_face_view_matrix(const Vec3& position, uint32_t face) {
    Vec3 forward, up;
    ReflectionProbeUtils::get_face_vectors(face, forward, up);

    Vec3 right = cross(up, forward);

    Mat4 view(1.0f);
    view[0][0] = right.x;
    view[0][1] = right.y;
    view[0][2] = right.z;
    view[1][0] = up.x;
    view[1][1] = up.y;
    view[1][2] = up.z;
    view[2][0] = forward.x;
    view[2][1] = forward.y;
    view[2][2] = forward.z;
    view[3][0] = -dot(right, position);
    view[3][1] = -dot(up, position);
    view[3][2] = -dot(forward, position);

    return view;
}

Mat4 ReflectionProbeSystem::get_face_projection_matrix() {
    // 90 degree FOV for cubemap face
    float fov = 3.14159f * 0.5f;
    float aspect = 1.0f;
    float near_plane = m_config.near_plane;
    float far_plane = m_config.far_plane;

    float tan_half_fov = std::tan(fov * 0.5f);

    Mat4 proj(1.0f);
    proj[0][0] = 1.0f / (aspect * tan_half_fov);
    proj[1][1] = 1.0f / tan_half_fov;
    proj[2][2] = far_plane / (far_plane - near_plane);
    proj[2][3] = 1.0f;
    proj[3][2] = -(far_plane * near_plane) / (far_plane - near_plane);
    proj[3][3] = 0.0f;

    return proj;
}

std::vector<ReflectionProbeHandle> ReflectionProbeSystem::get_probes_at(const Vec3& position) const {
    std::vector<ReflectionProbeHandle> result;

    for (uint32_t i = 0; i < m_probes.size(); ++i) {
        if (m_probe_used[i] && m_probes[i].is_in_range(position)) {
            result.push_back(i);
        }
    }

    // Sort by importance
    std::sort(result.begin(), result.end(),
        [this](ReflectionProbeHandle a, ReflectionProbeHandle b) {
            return m_probes[a].importance > m_probes[b].importance;
        }
    );

    return result;
}

ReflectionProbeHandle ReflectionProbeSystem::get_dominant_probe(const Vec3& position) const {
    ReflectionProbeHandle best = INVALID_REFLECTION_PROBE;
    float best_weight = 0.0f;

    for (uint32_t i = 0; i < m_probes.size(); ++i) {
        if (!m_probe_used[i]) continue;

        const ReflectionProbe& probe = m_probes[i];
        float weight = probe.get_weight_at(position) * probe.importance;

        if (weight > best_weight) {
            best_weight = weight;
            best = i;
        }
    }

    return best;
}

Vec3 ReflectionProbeSystem::sample_environment(const Vec3& position,
                                                 const Vec3& direction,
                                                 float /*roughness*/) const {
    auto probes = get_probes_at(position);

    if (probes.empty()) {
        // Fallback to skybox
        // Would sample m_skybox here
        return Vec3(0.2f, 0.3f, 0.5f);
    }

    // For now, just use dominant probe
    const ReflectionProbe& probe = m_probes[probes[0]];

    // Apply box projection
    Vec3 lookup_dir = probe.box_project(position, direction);

    // Would sample probe.cubemap here with lookup_dir and roughness-based mip
    return lookup_dir * probe.intensity;
}

std::vector<ReflectionProbeSystem::ProbeShaderData> ReflectionProbeSystem::get_shader_data(
    const Vec3& camera_pos, uint32_t max_probes) const {

    std::vector<ProbeShaderData> result;
    result.reserve(max_probes);

    // Get probes sorted by distance to camera
    std::vector<std::pair<float, uint32_t>> sorted_probes;

    for (uint32_t i = 0; i < m_probes.size(); ++i) {
        if (!m_probe_used[i] || !m_probes[i].enabled) continue;

        float dist = length(m_probes[i].position - camera_pos);
        sorted_probes.push_back({dist, i});
    }

    std::sort(sorted_probes.begin(), sorted_probes.end());

    for (uint32_t i = 0; i < std::min(static_cast<uint32_t>(sorted_probes.size()), max_probes); ++i) {
        const ReflectionProbe& probe = m_probes[sorted_probes[i].second];

        ProbeShaderData data;
        data.position_radius = Vec4(probe.position, probe.influence_radius);
        data.box_min = Vec4(probe.box_min, probe.importance);
        data.box_max = Vec4(probe.box_max, probe.blend_distance);
        data.intensity_projection = Vec4(
            probe.intensity,
            static_cast<float>(probe.projection),
            0.0f, 0.0f
        );

        result.push_back(data);
    }

    return result;
}

void ReflectionProbeSystem::bind_probes(bgfx::ViewId view_id, const Vec3& camera_pos) {
    auto shader_data = get_shader_data(camera_pos, 4);

    m_stats.visible_probes = static_cast<uint32_t>(shader_data.size());

    if (shader_data.empty()) {
        // Bind skybox only
        if (bgfx::isValid(m_skybox)) {
            bgfx::setTexture(4, s_environment, m_skybox);
        }
        return;
    }

    // Set probe data uniforms
    bgfx::setUniform(u_probe_data, shader_data.data(),
                     static_cast<uint16_t>(shader_data.size()));

    // Bind first probe cubemap (simplified - would use texture array in production)
    const ReflectionProbe& probe = m_probes[get_dominant_probe(camera_pos)];
    if (bgfx::isValid(probe.cubemap)) {
        bgfx::setTexture(4, s_environment, probe.cubemap);
    } else if (bgfx::isValid(m_skybox)) {
        bgfx::setTexture(4, s_environment, m_skybox);
    }
}

} // namespace engine::render
