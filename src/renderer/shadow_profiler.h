#pragma once

#include "core/types.h"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <array>

class VulkanContext;

// GPU timestamp-based profiler for shadow rendering performance
class ShadowProfiler {
public:
    static constexpr u32 MAX_SAMPLES = 120;  // 2 seconds at 60 FPS

    struct PerformanceMetrics {
        f32 avgTimeMs = 0.0f;
        f32 minTimeMs = 0.0f;
        f32 maxTimeMs = 0.0f;
        u32 sampleCount = 0;
    };

    ShadowProfiler() = default;
    ~ShadowProfiler();

    void Init(VulkanContext* context, u32 framesInFlight);
    void Shutdown();

    // Begin/end profiling for a specific pass
    void BeginPass(VkCommandBuffer cmd, u32 frameIndex, const std::string& passName);
    void EndPass(VkCommandBuffer cmd, u32 frameIndex, const std::string& passName);

    // Retrieve results (call after frame submission)
    void UpdateResults(u32 frameIndex);

    // Get metrics for a specific pass
    PerformanceMetrics GetMetrics(const std::string& passName) const;

    // Get all recorded pass names
    std::vector<std::string> GetPassNames() const;

    // Export to CSV
    void ExportToCSV(const std::string& filename) const;

    // Reset all statistics
    void Reset();

private:
    struct TimestampQuery {
        VkQueryPool queryPool = VK_NULL_HANDLE;
        u32 queryIndex = 0;
        bool active = false;
    };

    struct PassData {
        std::string name;
        std::vector<f32> samples;  // Time in milliseconds
        u32 currentSample = 0;
        TimestampQuery beginQuery;
        TimestampQuery endQuery;
    };

    VulkanContext* m_Context = nullptr;
    u32 m_FramesInFlight = 0;

    // Query pools (one per frame)
    std::vector<VkQueryPool> m_QueryPools;
    u32 m_NextQueryIndex = 0;
    static constexpr u32 MAX_QUERIES_PER_FRAME = 64;

    // Pass tracking
    std::vector<PassData> m_Passes;

    // Timestamp period (nanoseconds per timestamp unit)
    f32 m_TimestampPeriod = 1.0f;

    PassData* FindOrCreatePass(const std::string& passName);
    u32 AllocateQuery(u32 frameIndex);
};
