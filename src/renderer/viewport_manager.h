#pragma once

#include "core/types.h"
#include "ecs/entity.h"
#include "renderer/viewport.h"

#include <unordered_map>
#include <vector>

class VulkanContext;

// Manages a dynamic collection of viewports
class ViewportManager {
public:
    ViewportManager() = default;
    ~ViewportManager();

    // Initialize with Vulkan context
    void Init(VulkanContext* context);

    // Shutdown and destroy all viewports
    void Shutdown();

    // Create a new viewport and return its ID
    u32 CreateViewport(u32 width, u32 height, Entity cameraEntity, ViewportType type);

    // Destroy a viewport by ID
    void DestroyViewport(u32 id);

    // Get viewport by ID (returns nullptr if not found)
    Viewport* GetViewport(u32 id);
    const Viewport* GetViewport(u32 id) const;

    // Get all viewports
    std::vector<Viewport*> GetAllViewports();
    std::vector<const Viewport*> GetAllViewports() const;

    // Get number of viewports
    u32 GetViewportCount() const { return static_cast<u32>(m_Viewports.size()); }

    // Check if a viewport exists
    bool HasViewport(u32 id) const { return m_Viewports.find(id) != m_Viewports.end(); }

    // Prevent copying
    ViewportManager(const ViewportManager&) = delete;
    ViewportManager& operator=(const ViewportManager&) = delete;

private:
    u32 GenerateID();

    VulkanContext* m_Context = nullptr;
    std::unordered_map<u32, Viewport> m_Viewports;
    u32 m_NextID = 1;  // Start at 1, 0 reserved for invalid
};
