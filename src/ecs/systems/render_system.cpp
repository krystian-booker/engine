#include "ecs/systems/render_system.h"

#include "renderer/vulkan_context.h"
#include "resources/mesh_manager.h"

#include <exception>
#include <iostream>
#include <unordered_set>

RenderSystem::RenderSystem(ECSCoordinator* ecs, VulkanContext* context)
    : m_ECS(ecs)
    , m_Context(context) {
}

RenderSystem::~RenderSystem() = default;

void RenderSystem::Update() {
    const size_t previousCount = m_RenderData.size();
    m_RenderData.clear();
    m_RenderData.reserve(previousCount);

    if (!m_ECS) {
        return;
    }

    m_ECS->ForEach<Transform, Renderable>(
        [this](Entity /*entity*/, Transform& transform, Renderable& renderable) {
            if (!renderable.visible || !renderable.mesh.IsValid()) {
                return;
            }

            auto it = m_VulkanMeshes.find(renderable.mesh);
            if (it == m_VulkanMeshes.end()) {
                LoadMesh(renderable.mesh);
                it = m_VulkanMeshes.find(renderable.mesh);
                if (it == m_VulkanMeshes.end()) {
                    return;  // Still failed after load attempt
                }
            }

            RenderData data{};
            data.modelMatrix = transform.worldMatrix;
            data.meshHandle = renderable.mesh;
            m_RenderData.push_back(data);
        });
}

void RenderSystem::UploadMeshes() {
    if (!m_ECS) {
        return;
    }

    std::unordered_set<MeshHandle> uniqueMeshes;

    m_ECS->ForEach<Renderable>([&](Entity /*entity*/, Renderable& renderable) {
        if (!renderable.visible || !renderable.mesh.IsValid()) {
            return;
        }
        uniqueMeshes.insert(renderable.mesh);
    });

    for (MeshHandle handle : uniqueMeshes) {
        if (m_VulkanMeshes.find(handle) == m_VulkanMeshes.end()) {
            LoadMesh(handle);
        }
    }
}

void RenderSystem::Shutdown() {
    if (!m_VulkanMeshes.empty()) {
        for (auto& pair : m_VulkanMeshes) {
            if (pair.second) {
                try {
                    pair.second->Destroy();
                } catch (const std::exception& e) {
                    std::cerr << "RenderSystem::Shutdown failed to destroy mesh: " << e.what() << std::endl;
                }
            }
        }
        m_VulkanMeshes.clear();
    }
}

VulkanMesh* RenderSystem::GetVulkanMesh(MeshHandle handle) {
    auto it = m_VulkanMeshes.find(handle);
    if (it != m_VulkanMeshes.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::unique_ptr<VulkanMesh> RenderSystem::CreateVulkanMesh(MeshHandle /*handle*/, const MeshData* meshData) {
    if (!m_Context) {
        std::cerr << "RenderSystem::CreateVulkanMesh skipped: VulkanContext not set" << std::endl;
        return nullptr;
    }

    auto vulkanMesh = std::make_unique<VulkanMesh>();
    vulkanMesh->Create(m_Context, meshData);
    return vulkanMesh;
}

void RenderSystem::LoadMesh(MeshHandle handle) {
    MeshData* meshData = MeshManager::Instance().Get(handle);
    if (!meshData) {
        std::cerr << "RenderSystem::LoadMesh failed: missing mesh data (index " << handle.index
                  << ", generation " << handle.generation << ")" << std::endl;
        return;
    }

    try {
        auto vulkanMesh = CreateVulkanMesh(handle, meshData);
        if (!vulkanMesh) {
            std::cerr << "RenderSystem::LoadMesh failed: mesh creation returned null" << std::endl;
            return;
        }

        m_VulkanMeshes[handle] = std::move(vulkanMesh);
    } catch (const std::exception& e) {
        std::cerr << "RenderSystem::LoadMesh failed: " << e.what() << std::endl;
    }
}
