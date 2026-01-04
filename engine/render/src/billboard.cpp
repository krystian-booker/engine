#include <engine/render/billboard.hpp>
#include <engine/render/renderer.hpp>
#include <engine/render/render_pipeline.hpp>
#include <engine/core/log.hpp>
#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

namespace engine::render {

using namespace engine::core;

// Global instance
static BillboardRenderer s_billboard_renderer;

BillboardRenderer& get_billboard_renderer() {
    return s_billboard_renderer;
}

BillboardRenderer::~BillboardRenderer() {
    if (m_initialized) {
        shutdown();
    }
}

void BillboardRenderer::init(IRenderer* renderer) {
    m_renderer = renderer;
    m_initialized = true;

    create_quad_mesh();

    log(LogLevel::Info, "Billboard renderer initialized");
}

void BillboardRenderer::shutdown() {
    if (!m_initialized) return;

    if (m_quad_mesh.valid()) {
        m_renderer->destroy_mesh(m_quad_mesh);
        m_quad_mesh = MeshHandle{};
    }

    m_pending_batches.clear();
    m_initialized = false;
    m_renderer = nullptr;

    log(LogLevel::Info, "Billboard renderer shutdown");
}

void BillboardRenderer::create_quad_mesh() {
    // Create a unit quad mesh centered at origin
    // The vertex shader will expand this based on billboard size
    MeshData data;

    // 4 vertices for a quad
    data.vertices = {
        // Position            Normal              TexCoord           Color              Tangent
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}}
    };

    // Two triangles
    data.indices = {0, 1, 2, 0, 2, 3};

    data.bounds.min = Vec3(-0.5f, -0.5f, 0.0f);
    data.bounds.max = Vec3(0.5f, 0.5f, 0.0f);

    m_quad_mesh = m_renderer->create_mesh(data);
}

void BillboardRenderer::submit_batch(const BillboardBatch& batch) {
    if (!batch.instances.empty()) {
        m_pending_batches.push_back(batch);
    }
}

void BillboardRenderer::render(RenderView view, const CameraData& camera) {
    if (!m_initialized || m_pending_batches.empty()) {
        return;
    }

    m_billboard_count = 0;

    Mat4 mat_identity(1.0f);
    Vec3 axis_y(0.0f, 1.0f, 0.0f);
    Vec3 axis_z(0.0f, 0.0f, 1.0f);

    // Use index-based loops to avoid range-for syntax issues
    for (size_t b = 0; b < m_pending_batches.size(); ++b) {
        const auto& batch = m_pending_batches[b];
        
        if (!batch.texture.valid() || batch.instances.empty()) {
            continue;
        }

        // Calculate billboard orientations and render each instance
        for (size_t i = 0; i < batch.instances.size(); ++i) {
            const auto& inst = batch.instances[i];
            Mat4 transform = mat_identity;

            // Calculate billboard orientation based on mode
            if (batch.mode == BillboardMode::ScreenAligned) {
                // Full billboard - always face camera
                Vec3 inst_pos = inst.position;
                Vec3 cam_pos = camera.position;
                Vec3 diff = cam_pos - inst_pos;
                
                Vec3 look = glm::normalize(diff);
                
                // Handle edge case where look is parallel to up
                Vec3 right;
                if (std::abs(glm::dot(look, camera.up)) > 0.99f) {
                    right = glm::normalize(glm::cross(axis_z, look));
                } else {
                    right = glm::normalize(glm::cross(camera.up, look));
                }
                
                Vec3 up = glm::cross(look, right);

                // Build rotation matrix manually because we want to scale columns
                transform[0] = Vec4(right * inst.size.x, 0.0f);
                transform[1] = Vec4(up * inst.size.y, 0.0f);
                transform[2] = Vec4(look, 0.0f);
                transform[3] = Vec4(inst.position, 1.0f);

                // Apply Z-axis rotation if specified
                if (inst.rotation != 0.0f) {
                    transform = glm::rotate(transform, inst.rotation, look);
                }
            }
            else if (batch.mode == BillboardMode::AxisAligned) {
                // Cylindrical billboard - rotate around Y axis only
                Vec3 diff = camera.position - inst.position;
                diff.y = 0.0f;  // Project onto XZ plane
                
                Vec3 look;
                if (glm::length(diff) > 0.001f) {
                     look = glm::normalize(diff);
                } else {
                     look = axis_z;
                }

                Vec3 right = glm::normalize(glm::cross(axis_y, look));
                Vec3 up = axis_y;

                transform[0] = Vec4(right * inst.size.x, 0.0f);
                transform[1] = Vec4(up * inst.size.y, 0.0f);
                transform[2] = Vec4(look, 0.0f);
                transform[3] = Vec4(inst.position, 1.0f);
            }
            else if (batch.mode == BillboardMode::Fixed) {
                // No rotation - just scale and translate
                transform = glm::translate(mat_identity, inst.position);
                transform = glm::scale(transform, Vec3(inst.size.x, inst.size.y, 1.0f));

                if (inst.rotation != 0.0f) {
                    transform = glm::rotate(transform, inst.rotation, axis_z);
                }
            }

            // Submit the billboard quad with this transform
            m_renderer->submit_billboard(view, m_quad_mesh, batch.texture, transform,
                                          inst.color, inst.uv_offset, inst.uv_scale,
                                          batch.depth_test, batch.depth_write);

            m_billboard_count++;
        }
    }
}

void BillboardRenderer::clear() {
    m_pending_batches.clear();
}

} // namespace engine::render
