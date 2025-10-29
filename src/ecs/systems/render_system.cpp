#include "ecs/systems/render_system.h"

#include "renderer/vulkan_context.h"
#include "resources/mesh_manager.h"
#include "resources/material_manager.h"
#include "core/material_data.h"

#include <exception>
#include <iostream>
#include <unordered_set>
#include <algorithm>

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

    MaterialManager& materialMgr = MaterialManager::Instance();

    m_ECS->ForEach<Transform, Renderable>(
        [this, &materialMgr](Entity /*entity*/, Transform& transform, Renderable& renderable) {
            if (!renderable.visible || !renderable.mesh.IsValid()) {
                return;
            }

            // Load mesh if needed
            auto it = m_VulkanMeshes.find(renderable.mesh);
            if (it == m_VulkanMeshes.end()) {
                LoadMesh(renderable.mesh);
                it = m_VulkanMeshes.find(renderable.mesh);
                if (it == m_VulkanMeshes.end()) {
                    return;  // Still failed after load attempt
                }
            }

            // Get material (use default if invalid)
            MaterialHandle materialHandle = renderable.material;
            if (!materialHandle.IsValid()) {
                materialHandle = materialMgr.GetDefaultMaterial();
                if (!materialHandle.IsValid()) {
                    // Create default material on first use
                    materialHandle = materialMgr.CreateDefaultMaterial();
                }
            }

            MaterialData* material = materialMgr.Get(materialHandle);
            if (!material) {
                return;  // Invalid material
            }

            // Determine pipeline variant from material flags
            PipelineVariant variant = GetPipelineVariant(material->flags);

            RenderData data{};
            data.modelMatrix = transform.worldMatrix;
            data.meshHandle = renderable.mesh;
            data.materialHandle = materialHandle;
            data.pipelineVariant = variant;
            data.materialIndex = material->gpuMaterialIndex;
            m_RenderData.push_back(data);
        });

    // Sort render data by pipeline variant (opaque first, then masked, then blended)
    std::sort(m_RenderData.begin(), m_RenderData.end(),
        [](const RenderData& a, const RenderData& b) {
            u32 orderA = GetPipelineVariantSortOrder(a.pipelineVariant);
            u32 orderB = GetPipelineVariantSortOrder(b.pipelineVariant);
            if (orderA != orderB) {
                return orderA < orderB;
            }
            // Secondary sort by material to minimize state changes
            return a.materialIndex < b.materialIndex;
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
