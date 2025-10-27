#pragma once

#include "ecs/ecs_coordinator.h"
#include "ecs/components/transform.h"
#include "ecs/components/renderable.h"
#include "renderer/vulkan_mesh.h"

#include <memory>
#include <unordered_map>
#include <vector>

class VulkanContext;
struct MeshData;

struct RenderData {
    Mat4 modelMatrix;
    MeshHandle meshHandle;
};

class RenderSystem {
public:
    RenderSystem(ECSCoordinator* ecs, VulkanContext* context);
    virtual ~RenderSystem();

    void Update();
    void UploadMeshes();
    void Shutdown();

    const std::vector<RenderData>& GetRenderData() const { return m_RenderData; }
    VulkanMesh* GetVulkanMesh(MeshHandle handle);

protected:
    virtual std::unique_ptr<VulkanMesh> CreateVulkanMesh(MeshHandle handle, const MeshData* meshData);

private:
    void LoadMesh(MeshHandle handle);

    ECSCoordinator* m_ECS = nullptr;
    VulkanContext* m_Context = nullptr;

    std::vector<RenderData> m_RenderData;
    std::unordered_map<MeshHandle, std::unique_ptr<VulkanMesh>> m_VulkanMeshes;
};
