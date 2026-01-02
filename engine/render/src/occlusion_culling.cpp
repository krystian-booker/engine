#include <engine/render/occlusion_culling.hpp>
#include <algorithm>
#include <cmath>

namespace engine::render {

// Global instance
static OcclusionCullingSystem* s_occlusion_system = nullptr;

OcclusionCullingSystem& get_occlusion_system() {
    if (!s_occlusion_system) {
        static OcclusionCullingSystem instance;
        s_occlusion_system = &instance;
    }
    return *s_occlusion_system;
}

OcclusionCullingSystem::~OcclusionCullingSystem() {
    if (m_initialized) {
        shutdown();
    }
}

void OcclusionCullingSystem::init(const OcclusionCullingConfig& config) {
    if (m_initialized) return;

    m_config = config;

    // Allocate occluders and queries
    m_occluders.resize(config.max_software_occluders);
    m_occluder_used.resize(config.max_software_occluders, false);

    m_queries.resize(config.max_queries);
    m_query_used.resize(config.max_queries, false);

    // Create Hi-Z resources
    if (config.method == OcclusionMethod::HiZ || config.method == OcclusionMethod::Hybrid) {
        m_hiz_texture = bgfx::createTexture2D(
            config.hiz_width, config.hiz_height, true, 1,
            bgfx::TextureFormat::R32F,
            BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP |
            BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT
        );

        // Create framebuffers for each mip level
        uint32_t width = config.hiz_width;
        uint32_t height = config.hiz_height;

        for (uint32_t i = 0; i < config.hiz_mip_levels; ++i) {
            bgfx::Attachment att;
            att.init(m_hiz_texture, bgfx::Access::Write, i);
            m_hiz_fbs.push_back(bgfx::createFrameBuffer(1, &att));

            width = std::max(1u, width / 2);
            height = std::max(1u, height / 2);
        }

        // Create uniforms
        u_hiz_params = bgfx::createUniform("u_hizParams", bgfx::UniformType::Vec4);
        s_depth = bgfx::createUniform("s_depth", bgfx::UniformType::Sampler);
        s_hiz = bgfx::createUniform("s_hiz", bgfx::UniformType::Sampler);
    }

    // Allocate software depth buffer
    if (config.method == OcclusionMethod::Software || config.method == OcclusionMethod::Hybrid) {
        m_software_depth.resize(config.software_width * config.software_height, 1.0f);
    }

    m_initialized = true;
}

void OcclusionCullingSystem::shutdown() {
    if (!m_initialized) return;

    // Destroy Hi-Z resources
    if (bgfx::isValid(m_hiz_texture)) {
        bgfx::destroy(m_hiz_texture);
        m_hiz_texture = BGFX_INVALID_HANDLE;
    }

    for (auto& fb : m_hiz_fbs) {
        if (bgfx::isValid(fb)) {
            bgfx::destroy(fb);
        }
    }
    m_hiz_fbs.clear();

    if (bgfx::isValid(m_hiz_program)) {
        bgfx::destroy(m_hiz_program);
        m_hiz_program = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(u_hiz_params)) bgfx::destroy(u_hiz_params);
    if (bgfx::isValid(s_depth)) bgfx::destroy(s_depth);
    if (bgfx::isValid(s_hiz)) bgfx::destroy(s_hiz);

    // Destroy GPU queries
    for (auto& query : m_queries) {
        if (bgfx::isValid(query.gpu_query)) {
            bgfx::destroy(query.gpu_query);
        }
    }

    m_occluders.clear();
    m_occluder_used.clear();
    m_queries.clear();
    m_query_used.clear();
    m_software_depth.clear();

    m_initialized = false;
}

void OcclusionCullingSystem::set_config(const OcclusionCullingConfig& config) {
    // Would need to recreate resources if sizes changed
    m_config = config;
}

OccluderHandle OcclusionCullingSystem::add_occluder(const Occluder& occluder) {
    for (uint32_t i = 0; i < m_occluders.size(); ++i) {
        if (!m_occluder_used[i]) {
            m_occluders[i] = occluder;
            m_occluder_used[i] = true;
            return i;
        }
    }
    return INVALID_OCCLUDER;
}

void OcclusionCullingSystem::remove_occluder(OccluderHandle handle) {
    if (handle < m_occluders.size() && m_occluder_used[handle]) {
        m_occluders[handle] = Occluder{};
        m_occluder_used[handle] = false;
    }
}

void OcclusionCullingSystem::update_occluder(OccluderHandle handle, const Mat4& transform) {
    if (handle < m_occluders.size() && m_occluder_used[handle]) {
        m_occluders[handle].transform = transform;
    }
}

Occluder* OcclusionCullingSystem::get_occluder(OccluderHandle handle) {
    if (handle < m_occluders.size() && m_occluder_used[handle]) {
        return &m_occluders[handle];
    }
    return nullptr;
}

void OcclusionCullingSystem::begin_frame(bgfx::ViewId hiz_view,
                                          bgfx::TextureHandle depth_texture,
                                          const Mat4& view_matrix,
                                          const Mat4& proj_matrix) {
    m_view_matrix = view_matrix;
    m_proj_matrix = proj_matrix;
    m_view_proj_matrix = proj_matrix * view_matrix;
    m_frame_number++;

    m_stats = Stats{};

    // Generate Hi-Z pyramid
    if (m_config.method == OcclusionMethod::HiZ || m_config.method == OcclusionMethod::Hybrid) {
        generate_hiz(hiz_view, depth_texture);
    }

    // Rasterize occluders for software method
    if (m_config.method == OcclusionMethod::Software || m_config.method == OcclusionMethod::Hybrid) {
        rasterize_occluders();
    }
}

void OcclusionCullingSystem::generate_hiz(bgfx::ViewId view_id, bgfx::TextureHandle depth_texture) {
    if (!bgfx::isValid(m_hiz_program)) return;

    uint32_t width = m_config.hiz_width;
    uint32_t height = m_config.hiz_height;

    for (uint32_t mip = 0; mip < m_config.hiz_mip_levels && mip < m_hiz_fbs.size(); ++mip) {
        bgfx::setViewFrameBuffer(view_id + mip, m_hiz_fbs[mip]);
        bgfx::setViewRect(view_id + mip, 0, 0, width, height);

        float params[4] = {
            1.0f / width,
            1.0f / height,
            static_cast<float>(mip),
            0.0f
        };
        bgfx::setUniform(u_hiz_params, params);

        if (mip == 0) {
            bgfx::setTexture(0, s_depth, depth_texture);
        } else {
            bgfx::setTexture(0, s_hiz, m_hiz_texture, mip - 1);
        }

        bgfx::setState(BGFX_STATE_WRITE_R);
        bgfx::submit(view_id + mip, m_hiz_program);

        width = std::max(1u, width / 2);
        height = std::max(1u, height / 2);
    }
}

void OcclusionCullingSystem::rasterize_occluders() {
    // Clear depth buffer
    std::fill(m_software_depth.begin(), m_software_depth.end(), 1.0f);

    // Rasterize each occluder
    for (uint32_t i = 0; i < m_occluders.size(); ++i) {
        if (!m_occluder_used[i] || !m_occluders[i].enabled) continue;

        const Occluder& occluder = m_occluders[i];

        // Transform bounds to view space and rasterize
        // Simplified: just project AABB
        OcclusionBounds world_bounds = occluder.bounds;
        world_bounds.center = Vec3(
            occluder.transform[3][0],
            occluder.transform[3][1],
            occluder.transform[3][2]
        ) + occluder.bounds.center;

        Vec4 screen_rect = OcclusionUtils::calculate_screen_aabb(
            world_bounds, m_view_proj_matrix,
            static_cast<float>(m_config.software_width),
            static_cast<float>(m_config.software_height)
        );

        // Calculate depth
        Vec4 clip = m_view_proj_matrix * Vec4(world_bounds.center, 1.0f);
        float depth = clip.w > 0.0f ? clip.z / clip.w : 1.0f;

        // Rasterize rectangle
        int x0 = std::max(0, static_cast<int>(screen_rect.x));
        int y0 = std::max(0, static_cast<int>(screen_rect.y));
        int x1 = std::min(static_cast<int>(m_config.software_width - 1),
                          static_cast<int>(screen_rect.x + screen_rect.z));
        int y1 = std::min(static_cast<int>(m_config.software_height - 1),
                          static_cast<int>(screen_rect.y + screen_rect.w));

        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                uint32_t idx = y * m_config.software_width + x;
                m_software_depth[idx] = std::min(m_software_depth[idx], depth);
            }
        }
    }
}

OcclusionResult OcclusionCullingSystem::test_bounds(const OcclusionBounds& bounds) {
    m_stats.objects_tested++;

    bool visible = true;

    switch (m_config.method) {
        case OcclusionMethod::HiZ:
            visible = test_hiz(bounds);
            break;
        case OcclusionMethod::Software:
            visible = test_software(bounds);
            break;
        case OcclusionMethod::Hybrid:
            visible = test_hiz(bounds) || test_software(bounds);
            break;
        case OcclusionMethod::None:
        default:
            visible = true;
            break;
    }

    if (visible) {
        m_stats.objects_visible++;
        return OcclusionResult::Visible;
    } else {
        m_stats.objects_occluded++;
        return OcclusionResult::Occluded;
    }
}

OcclusionResult OcclusionCullingSystem::test_sphere(const Vec3& center, float radius) {
    OcclusionBounds bounds;
    bounds.center = center;
    bounds.extents = Vec3(radius);
    bounds.radius = radius;
    return test_bounds(bounds);
}

OcclusionResult OcclusionCullingSystem::test_aabb(const Vec3& min, const Vec3& max) {
    return test_bounds(OcclusionBounds::from_aabb(min, max));
}

bool OcclusionCullingSystem::test_hiz(const OcclusionBounds& bounds) {
    Vec4 screen_rect = OcclusionUtils::calculate_screen_aabb(
        bounds, m_view_proj_matrix,
        static_cast<float>(m_config.hiz_width),
        static_cast<float>(m_config.hiz_height)
    );

    // Check if off-screen
    if (screen_rect.x >= m_config.hiz_width || screen_rect.y >= m_config.hiz_height ||
        screen_rect.x + screen_rect.z < 0 || screen_rect.y + screen_rect.w < 0) {
        return false;
    }

    // Calculate mip level
    uint32_t mip = OcclusionUtils::calculate_hiz_mip(
        screen_rect.z, screen_rect.w,
        m_config.hiz_width, m_config.hiz_mip_levels
    );

    // Calculate object depth
    Vec4 clip = m_view_proj_matrix * Vec4(bounds.center, 1.0f);
    float object_depth = clip.w > 0.0f ? clip.z / clip.w : 1.0f;

    // Sample Hi-Z at screen center
    float u = (screen_rect.x + screen_rect.z * 0.5f) / m_config.hiz_width;
    float v = (screen_rect.y + screen_rect.w * 0.5f) / m_config.hiz_height;
    float hiz_depth = sample_hiz(u, v, mip);

    // Conservative test
    if (m_config.conservative) {
        object_depth -= bounds.radius * 0.01f;  // Bias
    }

    return OcclusionUtils::depth_test_conservative(hiz_depth, object_depth);
}

bool OcclusionCullingSystem::test_software(const OcclusionBounds& bounds) {
    Vec4 screen_rect = OcclusionUtils::calculate_screen_aabb(
        bounds, m_view_proj_matrix,
        static_cast<float>(m_config.software_width),
        static_cast<float>(m_config.software_height)
    );

    // Calculate object depth
    Vec4 clip = m_view_proj_matrix * Vec4(bounds.center, 1.0f);
    float object_depth = clip.w > 0.0f ? clip.z / clip.w : 1.0f;

    // Sample depth buffer at corners and center
    int x0 = std::max(0, static_cast<int>(screen_rect.x));
    int y0 = std::max(0, static_cast<int>(screen_rect.y));
    int x1 = std::min(static_cast<int>(m_config.software_width - 1),
                      static_cast<int>(screen_rect.x + screen_rect.z));
    int y1 = std::min(static_cast<int>(m_config.software_height - 1),
                      static_cast<int>(screen_rect.y + screen_rect.w));

    // Sample corners
    float max_depth = 0.0f;
    max_depth = std::max(max_depth, m_software_depth[y0 * m_config.software_width + x0]);
    max_depth = std::max(max_depth, m_software_depth[y0 * m_config.software_width + x1]);
    max_depth = std::max(max_depth, m_software_depth[y1 * m_config.software_width + x0]);
    max_depth = std::max(max_depth, m_software_depth[y1 * m_config.software_width + x1]);

    return object_depth <= max_depth;
}

float OcclusionCullingSystem::sample_hiz(float /*u*/, float /*v*/, uint32_t /*mip*/) {
    // Would read back from Hi-Z texture
    // In practice, this requires async readback or compute shader
    return 1.0f;  // Conservative: assume visible
}

void OcclusionCullingSystem::test_bounds_batch(const std::vector<OcclusionBounds>& bounds,
                                                 std::vector<OcclusionResult>& results) {
    results.resize(bounds.size());

    for (size_t i = 0; i < bounds.size(); ++i) {
        results[i] = test_bounds(bounds[i]);
    }
}

QueryHandle OcclusionCullingSystem::issue_query(uint32_t object_id, const OcclusionBounds& bounds) {
    for (uint32_t i = 0; i < m_queries.size(); ++i) {
        if (!m_query_used[i]) {
            OcclusionQuery& query = m_queries[i];
            query.object_id = object_id;
            query.bounds = bounds;
            query.result = OcclusionResult::Unknown;
            query.frame_issued = m_frame_number;
            query.pending = true;

            if (!bgfx::isValid(query.gpu_query)) {
                query.gpu_query = bgfx::createOcclusionQuery();
            }

            m_query_used[i] = true;
            m_stats.queries_issued++;
            m_stats.queries_pending++;

            return i;
        }
    }
    return INVALID_QUERY;
}

OcclusionResult OcclusionCullingSystem::get_query_result(QueryHandle handle) {
    if (handle >= m_queries.size() || !m_query_used[handle]) {
        return OcclusionResult::Visible;
    }

    OcclusionQuery& query = m_queries[handle];

    if (query.pending && bgfx::isValid(query.gpu_query)) {
        int32_t result = bgfx::getResult(query.gpu_query);

        if (result >= 0) {
            query.result = result > 0 ? OcclusionResult::Visible : OcclusionResult::Occluded;
            query.pending = false;
            m_stats.queries_pending--;
        }
    }

    return query.result;
}

void OcclusionCullingSystem::flush_queries() {
    for (uint32_t i = 0; i < m_queries.size(); ++i) {
        if (m_query_used[i] && !m_queries[i].pending) {
            m_query_used[i] = false;
        }
    }
}

} // namespace engine::render
