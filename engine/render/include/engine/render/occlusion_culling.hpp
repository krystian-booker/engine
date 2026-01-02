#pragma once

#include <engine/core/math.hpp>
#include <bgfx/bgfx.h>
#include <vector>
#include <atomic>

namespace engine::render {

using namespace engine::core;

// Occlusion culling method
enum class OcclusionMethod : uint8_t {
    None,           // No occlusion culling
    HiZ,            // Hierarchical-Z buffer (GPU)
    Software,       // Software rasterization (CPU)
    Hybrid          // GPU with CPU fallback
};

// Occluder type
enum class OccluderType : uint8_t {
    Mesh,           // Arbitrary mesh occluder
    Box,            // Axis-aligned box
    Sphere          // Sphere
};

// Occlusion query result
enum class OcclusionResult : uint8_t {
    Visible,
    Occluded,
    Unknown         // Query pending
};

// Occlusion culling configuration
struct OcclusionCullingConfig {
    OcclusionMethod method = OcclusionMethod::HiZ;

    // Hi-Z settings
    uint32_t hiz_width = 512;             // Hi-Z buffer width
    uint32_t hiz_height = 256;            // Hi-Z buffer height
    uint32_t hiz_mip_levels = 8;          // Number of mip levels

    // Software settings
    uint32_t software_width = 256;
    uint32_t software_height = 128;
    uint32_t max_software_occluders = 64;

    // Query settings
    uint32_t max_queries = 4096;
    bool conservative = true;             // Use conservative bounds
    float size_threshold = 0.01f;         // Min screen size to test

    // Temporal settings
    bool temporal_coherence = true;       // Use previous frame results
    uint32_t query_frames_delay = 1;      // Frames to wait for query
};

// Bounding volume for occlusion testing
struct OcclusionBounds {
    Vec3 center = Vec3(0.0f);
    Vec3 extents = Vec3(1.0f);          // Half-size
    float radius = 1.732f;               // Bounding sphere radius

    // Create from AABB
    static OcclusionBounds from_aabb(const Vec3& min, const Vec3& max) {
        OcclusionBounds bounds;
        bounds.center = (min + max) * 0.5f;
        bounds.extents = (max - min) * 0.5f;
        bounds.radius = length(bounds.extents);
        return bounds;
    }

    // Get AABB corners
    void get_corners(Vec3 corners[8]) const {
        corners[0] = center + Vec3(-extents.x, -extents.y, -extents.z);
        corners[1] = center + Vec3( extents.x, -extents.y, -extents.z);
        corners[2] = center + Vec3(-extents.x,  extents.y, -extents.z);
        corners[3] = center + Vec3( extents.x,  extents.y, -extents.z);
        corners[4] = center + Vec3(-extents.x, -extents.y,  extents.z);
        corners[5] = center + Vec3( extents.x, -extents.y,  extents.z);
        corners[6] = center + Vec3(-extents.x,  extents.y,  extents.z);
        corners[7] = center + Vec3( extents.x,  extents.y,  extents.z);
    }
};

// Occluder definition
struct Occluder {
    OccluderType type = OccluderType::Box;
    OcclusionBounds bounds;

    // For mesh occluders
    bgfx::VertexBufferHandle mesh_vb = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle mesh_ib = BGFX_INVALID_HANDLE;
    uint32_t index_count = 0;

    Mat4 transform = Mat4(1.0f);
    bool enabled = true;
    bool is_static = true;
};

// Occlusion query
struct OcclusionQuery {
    uint32_t object_id = 0;
    OcclusionBounds bounds;
    OcclusionResult result = OcclusionResult::Unknown;
    uint32_t frame_issued = 0;
    bool pending = false;

    // GPU query handle
    bgfx::OcclusionQueryHandle gpu_query = BGFX_INVALID_HANDLE;
};

// Handle types
using OccluderHandle = uint32_t;
using QueryHandle = uint32_t;
constexpr OccluderHandle INVALID_OCCLUDER = UINT32_MAX;
constexpr QueryHandle INVALID_QUERY = UINT32_MAX;

// Occlusion culling system
class OcclusionCullingSystem {
public:
    OcclusionCullingSystem() = default;
    ~OcclusionCullingSystem();

    // Non-copyable
    OcclusionCullingSystem(const OcclusionCullingSystem&) = delete;
    OcclusionCullingSystem& operator=(const OcclusionCullingSystem&) = delete;

    // Initialize/shutdown
    void init(const OcclusionCullingConfig& config = {});
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Configuration
    void set_config(const OcclusionCullingConfig& config);
    const OcclusionCullingConfig& get_config() const { return m_config; }

    // Occluder management
    OccluderHandle add_occluder(const Occluder& occluder);
    void remove_occluder(OccluderHandle handle);
    void update_occluder(OccluderHandle handle, const Mat4& transform);
    Occluder* get_occluder(OccluderHandle handle);

    // Begin frame (generates Hi-Z from depth)
    void begin_frame(bgfx::ViewId hiz_view,
                     bgfx::TextureHandle depth_texture,
                     const Mat4& view_matrix,
                     const Mat4& proj_matrix);

    // Test visibility
    OcclusionResult test_bounds(const OcclusionBounds& bounds);
    OcclusionResult test_sphere(const Vec3& center, float radius);
    OcclusionResult test_aabb(const Vec3& min, const Vec3& max);

    // Batch testing
    void test_bounds_batch(const std::vector<OcclusionBounds>& bounds,
                           std::vector<OcclusionResult>& results);

    // Async queries (GPU)
    QueryHandle issue_query(uint32_t object_id, const OcclusionBounds& bounds);
    OcclusionResult get_query_result(QueryHandle handle);
    void flush_queries();

    // Get Hi-Z texture for debugging
    bgfx::TextureHandle get_hiz_texture() const { return m_hiz_texture; }

    // Statistics
    struct Stats {
        uint32_t objects_tested = 0;
        uint32_t objects_visible = 0;
        uint32_t objects_occluded = 0;
        uint32_t queries_issued = 0;
        uint32_t queries_pending = 0;
        float hiz_generation_ms = 0.0f;
    };
    Stats get_stats() const { return m_stats; }

private:
    void generate_hiz(bgfx::ViewId view_id, bgfx::TextureHandle depth_texture);
    bool test_hiz(const OcclusionBounds& bounds);
    bool test_software(const OcclusionBounds& bounds);
    void rasterize_occluders();
    Vec4 project_bounds(const OcclusionBounds& bounds);
    float sample_hiz(float u, float v, uint32_t mip);

    OcclusionCullingConfig m_config;
    bool m_initialized = false;

    // View/projection
    Mat4 m_view_matrix;
    Mat4 m_proj_matrix;
    Mat4 m_view_proj_matrix;
    uint32_t m_frame_number = 0;

    // Occluders
    std::vector<Occluder> m_occluders;
    std::vector<bool> m_occluder_used;

    // Queries
    std::vector<OcclusionQuery> m_queries;
    std::vector<bool> m_query_used;

    // Hi-Z resources
    bgfx::TextureHandle m_hiz_texture = BGFX_INVALID_HANDLE;
    std::vector<bgfx::FrameBufferHandle> m_hiz_fbs;
    bgfx::ProgramHandle m_hiz_program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_hiz_params = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_depth = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_hiz = BGFX_INVALID_HANDLE;

    // Software depth buffer
    std::vector<float> m_software_depth;

    // Stats
    Stats m_stats;
};

// Global occlusion culling system
OcclusionCullingSystem& get_occlusion_system();

// Occlusion culling utilities
namespace OcclusionUtils {

// Calculate screen-space AABB from world bounds
inline Vec4 calculate_screen_aabb(const OcclusionBounds& bounds,
                                   const Mat4& view_proj,
                                   float screen_width,
                                   float screen_height) {
    Vec3 corners[8];
    bounds.get_corners(corners);

    Vec2 min_ss(FLT_MAX);
    Vec2 max_ss(-FLT_MAX);
    float min_depth = FLT_MAX;

    for (int i = 0; i < 8; ++i) {
        Vec4 clip = view_proj * Vec4(corners[i], 1.0f);

        if (clip.w <= 0.0f) {
            // Behind camera
            return Vec4(0, 0, screen_width, screen_height);
        }

        Vec2 ndc(clip.x / clip.w, clip.y / clip.w);
        Vec2 screen(
            (ndc.x * 0.5f + 0.5f) * screen_width,
            (1.0f - (ndc.y * 0.5f + 0.5f)) * screen_height
        );

        min_ss = min(min_ss, screen);
        max_ss = max(max_ss, screen);
        min_depth = std::min(min_depth, clip.z / clip.w);
    }

    return Vec4(min_ss.x, min_ss.y, max_ss.x - min_ss.x, max_ss.y - min_ss.y);
}

// Calculate mip level for Hi-Z test
inline uint32_t calculate_hiz_mip(float screen_width, float screen_height,
                                   uint32_t hiz_width, uint32_t max_mips) {
    float max_dim = std::max(screen_width, screen_height);
    if (max_dim <= 0.0f) return max_mips - 1;

    float mip = std::log2(max_dim);
    return std::min(static_cast<uint32_t>(mip), max_mips - 1);
}

// Conservative depth test
inline bool depth_test_conservative(float sample_depth, float object_depth) {
    return object_depth <= sample_depth;
}

} // namespace OcclusionUtils

// ECS Component for occlusion culling
struct OcclusionCullableComponent {
    OcclusionBounds bounds;
    OcclusionResult last_result = OcclusionResult::Visible;
    uint32_t last_visible_frame = 0;
    bool use_temporal = true;
};

// ECS Component for occluders
struct OccluderComponent {
    OccluderHandle occluder_handle = INVALID_OCCLUDER;
    bool is_static = true;
};

} // namespace engine::render
