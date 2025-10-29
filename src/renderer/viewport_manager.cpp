#include "renderer/viewport_manager.h"

#include "renderer/vulkan_context.h"

#include <stdexcept>

ViewportManager::~ViewportManager() {
    Shutdown();
}

void ViewportManager::Init(VulkanContext* context) {
    if (!context) {
        throw std::invalid_argument("ViewportManager::Init requires valid context");
    }

    Shutdown();
    m_Context = context;
    m_NextID = 1;
}

void ViewportManager::Shutdown() {
    m_Viewports.clear();
    m_Context = nullptr;
    m_NextID = 1;
}

u32 ViewportManager::CreateViewport(u32 width, u32 height, Entity cameraEntity, ViewportType type) {
    if (!m_Context) {
        throw std::runtime_error("ViewportManager::CreateViewport requires initialized context");
    }

    u32 id = GenerateID();

    // Use emplace to construct Viewport in-place
    auto [it, inserted] = m_Viewports.try_emplace(id);
    if (inserted) {
        it->second.Create(m_Context, width, height, cameraEntity, type);
        it->second.SetID(id);
    }

    return id;
}

void ViewportManager::DestroyViewport(u32 id) {
    auto it = m_Viewports.find(id);
    if (it != m_Viewports.end()) {
        it->second.Destroy();
        m_Viewports.erase(it);
    }
}

Viewport* ViewportManager::GetViewport(u32 id) {
    auto it = m_Viewports.find(id);
    return (it != m_Viewports.end()) ? &it->second : nullptr;
}

const Viewport* ViewportManager::GetViewport(u32 id) const {
    auto it = m_Viewports.find(id);
    return (it != m_Viewports.end()) ? &it->second : nullptr;
}

std::vector<Viewport*> ViewportManager::GetAllViewports() {
    std::vector<Viewport*> viewports;
    viewports.reserve(m_Viewports.size());
    for (auto& pair : m_Viewports) {
        viewports.push_back(&pair.second);
    }
    return viewports;
}

std::vector<const Viewport*> ViewportManager::GetAllViewports() const {
    std::vector<const Viewport*> viewports;
    viewports.reserve(m_Viewports.size());
    for (const auto& pair : m_Viewports) {
        viewports.push_back(&pair.second);
    }
    return viewports;
}

u32 ViewportManager::GenerateID() {
    // Simple incrementing ID
    // In production, could implement ID recycling or handle wraparound
    return m_NextID++;
}
