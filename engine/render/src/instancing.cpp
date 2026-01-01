#include <engine/render/instancing.hpp>
#include <algorithm>
#include <cmath>

namespace engine::render {

// Global instance
static InstancingSystem* s_instancing_system = nullptr;

InstancingSystem& get_instancing_system() {
    if (!s_instancing_system) {
        static InstancingSystem instance;
        s_instancing_system = &instance;
    }
    return *s_instancing_system;
}

InstancingSystem::~InstancingSystem() {
    if (m_initialized) {
        shutdown();
    }
}

void InstancingSystem::init(const InstancingConfig& config) {
    if (m_initialized) return;

    m_config = config;

    m_batches.resize(config.max_batches);
    m_batch_used.resize(config.max_batches, false);
    m_render_order.reserve(config.max_batches);

    m_initialized = true;
}

void InstancingSystem::shutdown() {
    if (!m_initialized) return;

    m_batches.clear();
    m_batch_used.clear();
    m_render_order.clear();

    m_initialized = false;
}

InstanceBatchHandle InstancingSystem::create_batch(bgfx::VertexBufferHandle vb,
                                                     bgfx::IndexBufferHandle ib,
                                                     bgfx::ProgramHandle program) {
    for (uint32_t i = 0; i < m_batches.size(); ++i) {
        if (!m_batch_used[i]) {
            InstanceBatch& batch = m_batches[i];
            batch = InstanceBatch{};
            batch.vertex_buffer = vb;
            batch.index_buffer = ib;
            batch.program = program;
            batch.batch_id = m_next_batch_id++;
            batch.instances.reserve(256);

            m_batch_used[i] = true;
            m_stats.total_batches++;

            return i;
        }
    }

    return INVALID_BATCH;
}

void InstancingSystem::destroy_batch(InstanceBatchHandle handle) {
    if (handle >= m_batches.size() || !m_batch_used[handle]) {
        return;
    }

    m_stats.total_instances -= m_batches[handle].get_instance_count();
    m_stats.total_batches--;

    m_batches[handle] = InstanceBatch{};
    m_batch_used[handle] = false;
}

InstanceBatch* InstancingSystem::get_batch(InstanceBatchHandle handle) {
    if (handle >= m_batches.size() || !m_batch_used[handle]) {
        return nullptr;
    }
    return &m_batches[handle];
}

const InstanceBatch* InstancingSystem::get_batch(InstanceBatchHandle handle) const {
    if (handle >= m_batches.size() || !m_batch_used[handle]) {
        return nullptr;
    }
    return &m_batches[handle];
}

InstanceHandle InstancingSystem::add_instance(InstanceBatchHandle batch_handle,
                                                const InstanceData& data) {
    InstanceHandle handle;
    handle.batch = batch_handle;

    InstanceBatch* batch = get_batch(batch_handle);
    if (!batch) return handle;

    if (batch->get_instance_count() >= m_config.max_instances_per_batch) {
        return handle;  // Batch full
    }

    handle.index = batch->add_instance(data);
    m_stats.total_instances++;

    return handle;
}

void InstancingSystem::remove_instance(const InstanceHandle& handle) {
    InstanceBatch* batch = get_batch(handle.batch);
    if (!batch) return;

    batch->remove_instance(handle.index);
    m_stats.total_instances--;
}

void InstancingSystem::update_instance(const InstanceHandle& handle, const InstanceData& data) {
    InstanceBatch* batch = get_batch(handle.batch);
    if (!batch) return;

    batch->update_instance(handle.index, data);
}

InstanceData* InstancingSystem::get_instance(const InstanceHandle& handle) {
    InstanceBatch* batch = get_batch(handle.batch);
    if (!batch || handle.index >= batch->instances.size()) return nullptr;

    return &batch->instances[handle.index];
}

void InstancingSystem::add_instances(InstanceBatchHandle batch_handle,
                                       const std::vector<InstanceData>& instances,
                                       std::vector<InstanceHandle>& out_handles) {
    out_handles.clear();
    out_handles.reserve(instances.size());

    for (const auto& instance : instances) {
        out_handles.push_back(add_instance(batch_handle, instance));
    }
}

void InstancingSystem::clear_batch(InstanceBatchHandle batch_handle) {
    InstanceBatch* batch = get_batch(batch_handle);
    if (!batch) return;

    m_stats.total_instances -= batch->get_instance_count();
    batch->clear();
}

void InstancingSystem::update_prev_transforms() {
    for (uint32_t i = 0; i < m_batches.size(); ++i) {
        if (!m_batch_used[i]) continue;

        InstanceBatch& batch = m_batches[i];
        for (auto& instance : batch.instances) {
            instance.prev_transform = instance.transform;
        }
    }
}

void InstancingSystem::set_frustum(const Mat4& view_proj) {
    // Extract frustum planes from view-projection matrix
    // Left plane
    m_frustum_planes[0] = Vec4(
        view_proj.m[0][3] + view_proj.m[0][0],
        view_proj.m[1][3] + view_proj.m[1][0],
        view_proj.m[2][3] + view_proj.m[2][0],
        view_proj.m[3][3] + view_proj.m[3][0]
    );

    // Right plane
    m_frustum_planes[1] = Vec4(
        view_proj.m[0][3] - view_proj.m[0][0],
        view_proj.m[1][3] - view_proj.m[1][0],
        view_proj.m[2][3] - view_proj.m[2][0],
        view_proj.m[3][3] - view_proj.m[3][0]
    );

    // Bottom plane
    m_frustum_planes[2] = Vec4(
        view_proj.m[0][3] + view_proj.m[0][1],
        view_proj.m[1][3] + view_proj.m[1][1],
        view_proj.m[2][3] + view_proj.m[2][1],
        view_proj.m[3][3] + view_proj.m[3][1]
    );

    // Top plane
    m_frustum_planes[3] = Vec4(
        view_proj.m[0][3] - view_proj.m[0][1],
        view_proj.m[1][3] - view_proj.m[1][1],
        view_proj.m[2][3] - view_proj.m[2][1],
        view_proj.m[3][3] - view_proj.m[3][1]
    );

    // Near plane
    m_frustum_planes[4] = Vec4(
        view_proj.m[0][2],
        view_proj.m[1][2],
        view_proj.m[2][2],
        view_proj.m[3][2]
    );

    // Far plane
    m_frustum_planes[5] = Vec4(
        view_proj.m[0][3] - view_proj.m[0][2],
        view_proj.m[1][3] - view_proj.m[1][2],
        view_proj.m[2][3] - view_proj.m[2][2],
        view_proj.m[3][3] - view_proj.m[3][2]
    );

    // Normalize planes
    for (int i = 0; i < 6; ++i) {
        float len = std::sqrt(
            m_frustum_planes[i].x * m_frustum_planes[i].x +
            m_frustum_planes[i].y * m_frustum_planes[i].y +
            m_frustum_planes[i].z * m_frustum_planes[i].z
        );
        if (len > 0.0f) {
            m_frustum_planes[i] = m_frustum_planes[i] * (1.0f / len);
        }
    }
}

bool InstancingSystem::is_batch_visible(const InstanceBatch& batch) const {
    if (!m_config.frustum_culling || batch.cull_radius <= 0.0f) {
        return true;
    }

    // Sphere-frustum test
    for (int i = 0; i < 6; ++i) {
        float dist = m_frustum_planes[i].x * batch.center.x +
                     m_frustum_planes[i].y * batch.center.y +
                     m_frustum_planes[i].z * batch.center.z +
                     m_frustum_planes[i].w;

        if (dist < -batch.cull_radius) {
            return false;
        }
    }

    return true;
}

void InstancingSystem::prepare_render(const Mat4& view_proj, const Vec3& camera_pos) {
    m_camera_position = camera_pos;
    set_frustum(view_proj);

    // Build render order
    m_render_order.clear();
    m_stats.visible_batches = 0;
    m_stats.visible_instances = 0;

    for (uint32_t i = 0; i < m_batches.size(); ++i) {
        if (!m_batch_used[i]) continue;

        InstanceBatch& batch = m_batches[i];
        if (!batch.visible || batch.instances.empty()) continue;

        batch.visible = is_batch_visible(batch);
        if (!batch.visible) continue;

        m_render_order.push_back(i);
        m_stats.visible_batches++;
        m_stats.visible_instances += batch.get_instance_count();
    }

    // Sort by distance if enabled
    if (m_config.distance_sorting) {
        std::sort(m_render_order.begin(), m_render_order.end(),
            [this, &camera_pos](uint32_t a, uint32_t b) {
                float dist_a = length(m_batches[a].center - camera_pos);
                float dist_b = length(m_batches[b].center - camera_pos);
                return dist_a < dist_b;
            }
        );
    }
}

void InstancingSystem::upload_instance_buffer(InstanceBatch& batch) {
    if (batch.instances.empty()) return;

    uint32_t num_instances = batch.get_instance_count();
    uint16_t stride = sizeof(InstanceData);

    // Check if buffer available
    if (!InstancingUtils::check_avail(num_instances, stride)) {
        return;
    }

    bgfx::allocInstanceDataBuffer(&batch.instance_buffer, num_instances, stride);
    memcpy(batch.instance_buffer.data, batch.instances.data(), num_instances * stride);

    batch.dirty = false;
}

void InstancingSystem::render(bgfx::ViewId view_id) {
    m_stats.draw_calls = 0;

    for (uint32_t batch_index : m_render_order) {
        render_batch(view_id, batch_index);
    }
}

void InstancingSystem::render_batch(bgfx::ViewId view_id, InstanceBatchHandle handle) {
    InstanceBatch* batch = get_batch(handle);
    if (!batch || batch->instances.empty()) return;

    // Upload instance buffer if dirty
    if (batch->dirty) {
        upload_instance_buffer(*batch);
    }

    // Set vertex/index buffers
    bgfx::setVertexBuffer(0, batch->vertex_buffer);
    bgfx::setIndexBuffer(batch->index_buffer);

    // Set instance buffer
    bgfx::setInstanceDataBuffer(&batch->instance_buffer);

    // Set state
    bgfx::setState(batch->render_state);

    // Submit
    bgfx::submit(view_id, batch->program);
    m_stats.draw_calls++;
}

} // namespace engine::render
