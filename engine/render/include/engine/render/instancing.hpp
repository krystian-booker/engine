#pragma once

#include <engine/core/math.hpp>
#include <bgfx/bgfx.h>
#include <vector>
#include <unordered_map>
#include <memory>

namespace engine::render {

using namespace engine::core;

// Maximum instances per batch
constexpr uint32_t MAX_INSTANCES_PER_BATCH = 4096;

// Instance data for GPU instancing
struct InstanceData {
    Mat4 transform;
    Mat4 prev_transform;     // For motion vectors
    Vec4 custom_data;        // User-defined per-instance data (color tint, etc.)

    InstanceData() : transform(Mat4::identity()),
                     prev_transform(Mat4::identity()),
                     custom_data(1.0f, 1.0f, 1.0f, 1.0f) {}
};

// Compact instance data (transform only)
struct InstanceDataCompact {
    Mat4 transform;

    InstanceDataCompact() : transform(Mat4::identity()) {}
};

// Instance batch - a group of instances sharing mesh and material
struct InstanceBatch {
    // Mesh reference
    bgfx::VertexBufferHandle vertex_buffer = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle index_buffer = BGFX_INVALID_HANDLE;

    // Material/program
    bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
    uint64_t render_state = BGFX_STATE_DEFAULT;

    // Instance data
    bgfx::InstanceDataBuffer instance_buffer;
    std::vector<InstanceData> instances;

    // Batch properties
    uint32_t batch_id = 0;
    bool dirty = true;          // Need to re-upload instance buffer
    bool visible = true;
    float cull_radius = 0.0f;   // For frustum culling (0 = no culling)
    Vec3 center = Vec3(0.0f);   // Batch center for distance sorting

    // Get instance count
    uint32_t get_instance_count() const { return static_cast<uint32_t>(instances.size()); }

    // Add instance
    uint32_t add_instance(const InstanceData& data) {
        uint32_t index = static_cast<uint32_t>(instances.size());
        instances.push_back(data);
        dirty = true;
        return index;
    }

    // Remove instance (swap with last)
    void remove_instance(uint32_t index) {
        if (index < instances.size()) {
            instances[index] = instances.back();
            instances.pop_back();
            dirty = true;
        }
    }

    // Update instance
    void update_instance(uint32_t index, const InstanceData& data) {
        if (index < instances.size()) {
            instances[index] = data;
            dirty = true;
        }
    }

    // Clear all instances
    void clear() {
        instances.clear();
        dirty = true;
    }
};

// Handle type
using InstanceBatchHandle = uint32_t;
constexpr InstanceBatchHandle INVALID_BATCH = UINT32_MAX;

// Instance handle (batch + index)
struct InstanceHandle {
    InstanceBatchHandle batch = INVALID_BATCH;
    uint32_t index = 0;

    bool is_valid() const { return batch != INVALID_BATCH; }
};

// Instancing system configuration
struct InstancingConfig {
    uint32_t max_batches = 256;
    uint32_t max_instances_per_batch = MAX_INSTANCES_PER_BATCH;
    bool auto_batching = true;           // Automatically merge compatible instances
    bool frustum_culling = true;
    bool distance_sorting = true;        // Sort batches by distance for transparency
    float lod_distance_bias = 1.0f;
};

// Instancing system
class InstancingSystem {
public:
    InstancingSystem() = default;
    ~InstancingSystem();

    // Non-copyable
    InstancingSystem(const InstancingSystem&) = delete;
    InstancingSystem& operator=(const InstancingSystem&) = delete;

    // Initialize/shutdown
    void init(const InstancingConfig& config = {});
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Configuration
    void set_config(const InstancingConfig& config) { m_config = config; }
    const InstancingConfig& get_config() const { return m_config; }

    // Batch management
    InstanceBatchHandle create_batch(bgfx::VertexBufferHandle vb,
                                      bgfx::IndexBufferHandle ib,
                                      bgfx::ProgramHandle program);
    void destroy_batch(InstanceBatchHandle handle);
    InstanceBatch* get_batch(InstanceBatchHandle handle);
    const InstanceBatch* get_batch(InstanceBatchHandle handle) const;

    // Instance management
    InstanceHandle add_instance(InstanceBatchHandle batch, const InstanceData& data);
    void remove_instance(const InstanceHandle& handle);
    void update_instance(const InstanceHandle& handle, const InstanceData& data);
    InstanceData* get_instance(const InstanceHandle& handle);

    // Bulk operations
    void add_instances(InstanceBatchHandle batch,
                       const std::vector<InstanceData>& instances,
                       std::vector<InstanceHandle>& out_handles);
    void clear_batch(InstanceBatchHandle batch);

    // Update transforms (call before render)
    void update_prev_transforms();

    // Prepare for rendering
    void prepare_render(const Mat4& view_proj, const Vec3& camera_pos);

    // Render all batches
    void render(bgfx::ViewId view_id);

    // Render specific batch
    void render_batch(bgfx::ViewId view_id, InstanceBatchHandle batch);

    // Frustum culling
    void set_frustum(const Mat4& view_proj);

    // Statistics
    struct Stats {
        uint32_t total_batches = 0;
        uint32_t total_instances = 0;
        uint32_t visible_batches = 0;
        uint32_t visible_instances = 0;
        uint32_t draw_calls = 0;
    };
    Stats get_stats() const { return m_stats; }

private:
    void upload_instance_buffer(InstanceBatch& batch);
    bool is_batch_visible(const InstanceBatch& batch) const;

    InstancingConfig m_config;
    bool m_initialized = false;

    // Batches
    std::vector<InstanceBatch> m_batches;
    std::vector<bool> m_batch_used;
    uint32_t m_next_batch_id = 1;

    // Frustum planes for culling
    Vec4 m_frustum_planes[6];
    Vec3 m_camera_position;

    // Sorted batch indices for rendering
    std::vector<uint32_t> m_render_order;

    // Stats
    Stats m_stats;
};

// Global instancing system
InstancingSystem& get_instancing_system();

// ECS Component for instanced rendering
struct InstancedRendererComponent {
    InstanceHandle instance_handle;

    // LOD override
    float lod_bias = 1.0f;

    // Per-instance custom data
    Vec4 custom_data = Vec4(1.0f);
};

// Instancing utilities
namespace InstancingUtils {

// Check if instance buffer available
inline bool check_avail(uint32_t num_instances, uint16_t stride) {
    return bgfx::getAvailInstanceDataBuffer(num_instances, stride) == num_instances;
}

// Create instance buffer with transform only
inline void fill_transform_buffer(bgfx::InstanceDataBuffer& buffer,
                                   const std::vector<Mat4>& transforms) {
    uint32_t count = static_cast<uint32_t>(transforms.size());
    uint16_t stride = sizeof(Mat4);

    if (!check_avail(count, stride)) return;

    bgfx::allocInstanceDataBuffer(&buffer, count, stride);
    memcpy(buffer.data, transforms.data(), count * stride);
}

// Create instance buffer with full data
inline void fill_instance_buffer(bgfx::InstanceDataBuffer& buffer,
                                  const std::vector<InstanceData>& instances) {
    uint32_t count = static_cast<uint32_t>(instances.size());
    uint16_t stride = sizeof(InstanceData);

    if (!check_avail(count, stride)) return;

    bgfx::allocInstanceDataBuffer(&buffer, count, stride);
    memcpy(buffer.data, instances.data(), count * stride);
}

// Generate grid of transforms
inline void generate_grid(std::vector<Mat4>& out_transforms,
                           uint32_t count_x, uint32_t count_y, uint32_t count_z,
                           const Vec3& spacing,
                           const Vec3& offset = Vec3(0.0f)) {
    out_transforms.reserve(count_x * count_y * count_z);

    for (uint32_t z = 0; z < count_z; ++z) {
        for (uint32_t y = 0; y < count_y; ++y) {
            for (uint32_t x = 0; x < count_x; ++x) {
                Vec3 pos = offset + Vec3(
                    x * spacing.x,
                    y * spacing.y,
                    z * spacing.z
                );
                out_transforms.push_back(Mat4::from_translation(pos));
            }
        }
    }
}

// Generate random transforms within bounds
inline void generate_random(std::vector<Mat4>& out_transforms,
                             uint32_t count,
                             const Vec3& min_bounds,
                             const Vec3& max_bounds,
                             bool random_rotation = true,
                             const Vec3& scale_range = Vec3(1.0f)) {
    out_transforms.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        // Random position
        Vec3 pos = Vec3(
            min_bounds.x + (max_bounds.x - min_bounds.x) * (static_cast<float>(rand()) / RAND_MAX),
            min_bounds.y + (max_bounds.y - min_bounds.y) * (static_cast<float>(rand()) / RAND_MAX),
            min_bounds.z + (max_bounds.z - min_bounds.z) * (static_cast<float>(rand()) / RAND_MAX)
        );

        Mat4 transform = Mat4::from_translation(pos);

        if (random_rotation) {
            float angle = (static_cast<float>(rand()) / RAND_MAX) * 6.28318f;
            transform = transform * Mat4::from_rotation(Quat::from_axis_angle(Vec3(0, 1, 0), angle));
        }

        if (scale_range.x != 1.0f || scale_range.y != 1.0f || scale_range.z != 1.0f) {
            float scale = 1.0f + (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 2.0f * (scale_range.x - 1.0f);
            transform = transform * Mat4::from_scale(Vec3(scale));
        }

        out_transforms.push_back(transform);
    }
}

} // namespace InstancingUtils

} // namespace engine::render
