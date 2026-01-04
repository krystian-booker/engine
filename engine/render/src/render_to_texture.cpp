#include <engine/render/render_to_texture.hpp>
#include <engine/render/renderer.hpp>
#include <engine/core/log.hpp>
#include <algorithm>

namespace engine::render {

using namespace engine::core;

// Global instance
static RenderToTextureSystem s_rtt_system;

RenderToTextureSystem& get_rtt_system() {
    return s_rtt_system;
}

RenderToTextureSystem::~RenderToTextureSystem() {
    if (m_initialized) {
        shutdown();
    }
}

void RenderToTextureSystem::init(IRenderer* renderer) {
    m_renderer = renderer;
    m_initialized = true;

    log(LogLevel::Info, "Render-to-texture system initialized");
}

void RenderToTextureSystem::shutdown() {
    if (!m_initialized) return;

    m_cameras.clear();
    m_initialized = false;
    m_renderer = nullptr;

    log(LogLevel::Info, "Render-to-texture system shutdown");
}

RTTTarget RenderToTextureSystem::create_target(uint32_t width, uint32_t height,
                                                TextureFormat color_format,
                                                bool has_depth) {
    RTTTarget target;
    target.width = width;
    target.height = height;

    RenderTargetDesc desc;
    desc.width = width;
    desc.height = height;
    desc.color_attachment_count = 1;
    desc.color_format = color_format;
    desc.has_depth = has_depth;
    desc.depth_format = TextureFormat::Depth32F;
    desc.samplable = true;
    desc.debug_name = "RTT_Target";

    target.target = m_renderer->create_render_target(desc);

    if (target.target.valid()) {
        target.color_texture = m_renderer->get_render_target_texture(target.target, 0);
        if (has_depth) {
            target.depth_texture = m_renderer->get_render_target_texture(target.target, UINT32_MAX);
        }
        target.valid = true;
        m_active_target_count++;

        log(LogLevel::Debug, "Created RTT target {}x{}", width, height);
    }

    return target;
}

void RenderToTextureSystem::destroy_target(RTTTarget& target) {
    if (target.valid && target.target.valid()) {
        m_renderer->destroy_render_target(target.target);
        m_active_target_count--;

        log(LogLevel::Debug, "Destroyed RTT target");
    }

    target = RTTTarget{};
}

void RenderToTextureSystem::resize_target(RTTTarget& target, uint32_t width, uint32_t height) {
    if (!target.valid) return;

    if (target.width == width && target.height == height) {
        return;  // No change needed
    }

    m_renderer->resize_render_target(target.target, width, height);
    target.width = width;
    target.height = height;

    // Re-fetch texture handles after resize
    target.color_texture = m_renderer->get_render_target_texture(target.target, 0);
    target.depth_texture = m_renderer->get_render_target_texture(target.target, UINT32_MAX);

    log(LogLevel::Debug, "Resized RTT target to {}x{}", width, height);
}

void RenderToTextureSystem::add_camera(const CameraRenderEntry& entry) {
    m_cameras.push_back(entry);

    // Keep sorted by priority
    std::sort(m_cameras.begin(), m_cameras.end(),
        [](const CameraRenderEntry& a, const CameraRenderEntry& b) {
            return a.priority < b.priority;
        });
}

void RenderToTextureSystem::clear_cameras() {
    m_cameras.clear();
}

} // namespace engine::render
