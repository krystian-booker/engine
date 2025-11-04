#pragma once

#include "core/types.h"
#include "core/math.h"
#include "renderer/vulkan_shadow_map.h"
#include "renderer/vulkan_shadow_atlas.h"

#include <vulkan/vulkan.h>
#include <memory>

class VulkanContext;
class ECSCoordinator;
class ShadowSystem;
class ShadowProfiler;

// Shadow renderer - manages shadow map rendering for all light types
class VulkanShadowRenderer {
public:
    VulkanShadowRenderer() = default;
    ~VulkanShadowRenderer();

    void Init(VulkanContext* context, ECSCoordinator* ecs);
    void Shutdown();

    // Render all shadow maps
    void RenderShadows(VkCommandBuffer cmd, u32 frameIndex);

    // Accessors for shadow map resources
    VkImage GetDirectionalShadowDepthImage() const;
    VkImageView GetDirectionalShadowImageView() const;
    VkSampler GetDirectionalShadowSampler() const;  // Comparison sampler
    VkSampler GetDirectionalRawDepthSampler() const;  // Non-comparison sampler for PCSS
    VkFormat GetShadowFormat() const;
    u32 GetDirectionalShadowResolution() const;
    u32 GetNumCascades() const;

    bool IsInitialized() const { return m_Context != nullptr; }
    bool HasShadowCastingLights() const;

    // Set shadow system (called by renderer during initialization)
    void SetShadowSystem(class ShadowSystem* shadowSystem) { m_ShadowSystem = shadowSystem; }

    // Get profiler for external access (ImGui, etc.)
    ShadowProfiler* GetProfiler() { return m_Profiler.get(); }

private:
    void CreateShadowPipeline();
    void DestroyShadowPipeline();

    void RenderDirectionalShadows(VkCommandBuffer cmd, u32 frameIndex);
    void RenderCascade(VkCommandBuffer cmd, u32 cascadeIndex, const Mat4& lightViewProj);

    VulkanContext* m_Context = nullptr;
    ECSCoordinator* m_ECS = nullptr;
    class ShadowSystem* m_ShadowSystem = nullptr;

    // Directional light cascaded shadow map
    std::unique_ptr<VulkanShadowMap> m_DirectionalShadowMap;

    // Performance profiler
    std::unique_ptr<ShadowProfiler> m_Profiler;

    // Point/spot light shadow atlas (future use)
    std::unique_ptr<VulkanShadowAtlas> m_PointSpotAtlas;

    // Shadow rendering pipeline (depth-only)
    VkPipeline m_ShadowPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_ShadowPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_ShadowDescriptorLayout = VK_NULL_HANDLE;

    // Configuration
    u32 m_ShadowResolution = 2048;
    u32 m_NumCascades = 4;
};
