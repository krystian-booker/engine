#include "shadow_profiler.h"
#include "vulkan_context.h"

#include <fstream>
#include <iostream>
#include <algorithm>
#include <numeric>
#include <cmath>

ShadowProfiler::~ShadowProfiler() {
    Shutdown();
}

void ShadowProfiler::Init(VulkanContext* context, u32 framesInFlight) {
    if (!context) {
        throw std::runtime_error("ShadowProfiler::Init requires valid context");
    }

    m_Context = context;
    m_FramesInFlight = framesInFlight;

    VkDevice device = m_Context->GetDevice();
    VkPhysicalDevice physicalDevice = m_Context->GetPhysicalDevice();

    // Get timestamp period
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    m_TimestampPeriod = properties.limits.timestampPeriod;

    // Create query pools (one per frame)
    m_QueryPools.resize(framesInFlight);
    for (u32 i = 0; i < framesInFlight; ++i) {
        VkQueryPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        poolInfo.queryCount = MAX_QUERIES_PER_FRAME;

        if (vkCreateQueryPool(device, &poolInfo, nullptr, &m_QueryPools[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create timestamp query pool");
        }
    }

    std::cout << "ShadowProfiler initialized (timestamp period: " << m_TimestampPeriod << " ns)" << std::endl;
}

void ShadowProfiler::Shutdown() {
    if (!m_Context) {
        return;
    }

    VkDevice device = m_Context->GetDevice();

    for (VkQueryPool pool : m_QueryPools) {
        if (pool != VK_NULL_HANDLE) {
            vkDestroyQueryPool(device, pool, nullptr);
        }
    }

    m_QueryPools.clear();
    m_Passes.clear();
    m_Context = nullptr;
}

void ShadowProfiler::BeginPass(VkCommandBuffer cmd, u32 frameIndex, const std::string& passName) {
    if (!m_Context || frameIndex >= m_FramesInFlight) {
        return;
    }

    PassData* pass = FindOrCreatePass(passName);
    if (!pass) {
        return;
    }

    // Allocate query index
    u32 queryIndex = AllocateQuery(frameIndex);
    if (queryIndex >= MAX_QUERIES_PER_FRAME) {
        return;  // Out of queries
    }

    // Write timestamp
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_QueryPools[frameIndex], queryIndex);

    pass->beginQuery.queryPool = m_QueryPools[frameIndex];
    pass->beginQuery.queryIndex = queryIndex;
    pass->beginQuery.active = true;
}

void ShadowProfiler::EndPass(VkCommandBuffer cmd, u32 frameIndex, const std::string& passName) {
    if (!m_Context || frameIndex >= m_FramesInFlight) {
        return;
    }

    PassData* pass = FindOrCreatePass(passName);
    if (!pass || !pass->beginQuery.active) {
        return;
    }

    // Allocate query index
    u32 queryIndex = AllocateQuery(frameIndex);
    if (queryIndex >= MAX_QUERIES_PER_FRAME) {
        return;  // Out of queries
    }

    // Write timestamp
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_QueryPools[frameIndex], queryIndex);

    pass->endQuery.queryPool = m_QueryPools[frameIndex];
    pass->endQuery.queryIndex = queryIndex;
    pass->endQuery.active = true;
}

void ShadowProfiler::UpdateResults(u32 frameIndex) {
    if (!m_Context || frameIndex >= m_FramesInFlight) {
        return;
    }

    VkDevice device = m_Context->GetDevice();

    for (PassData& pass : m_Passes) {
        if (!pass.beginQuery.active || !pass.endQuery.active) {
            continue;
        }

        // Read timestamps
        u64 timestamps[2] = {0, 0};
        VkResult result = vkGetQueryPoolResults(
            device,
            m_QueryPools[frameIndex],
            pass.beginQuery.queryIndex,
            1,
            sizeof(u64),
            &timestamps[0],
            sizeof(u64),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
        );

        if (result != VK_SUCCESS) {
            continue;
        }

        result = vkGetQueryPoolResults(
            device,
            m_QueryPools[frameIndex],
            pass.endQuery.queryIndex,
            1,
            sizeof(u64),
            &timestamps[1],
            sizeof(u64),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
        );

        if (result != VK_SUCCESS) {
            continue;
        }

        // Calculate time in milliseconds
        u64 delta = timestamps[1] - timestamps[0];
        f32 timeMs = static_cast<f32>(delta) * m_TimestampPeriod / 1000000.0f;

        // Store sample
        if (pass.samples.size() < MAX_SAMPLES) {
            pass.samples.push_back(timeMs);
        } else {
            pass.samples[pass.currentSample] = timeMs;
            pass.currentSample = (pass.currentSample + 1) % MAX_SAMPLES;
        }

        // Mark queries as inactive
        pass.beginQuery.active = false;
        pass.endQuery.active = false;
    }

    // Reset query pool for next use
    vkResetQueryPool(device, m_QueryPools[frameIndex], 0, MAX_QUERIES_PER_FRAME);
    m_NextQueryIndex = 0;
}

ShadowProfiler::PerformanceMetrics ShadowProfiler::GetMetrics(const std::string& passName) const {
    PerformanceMetrics metrics;

    for (const PassData& pass : m_Passes) {
        if (pass.name == passName && !pass.samples.empty()) {
            metrics.sampleCount = static_cast<u32>(pass.samples.size());

            f32 sum = std::accumulate(pass.samples.begin(), pass.samples.end(), 0.0f);
            metrics.avgTimeMs = sum / static_cast<f32>(metrics.sampleCount);

            metrics.minTimeMs = *std::min_element(pass.samples.begin(), pass.samples.end());
            metrics.maxTimeMs = *std::max_element(pass.samples.begin(), pass.samples.end());

            break;
        }
    }

    return metrics;
}

std::vector<std::string> ShadowProfiler::GetPassNames() const {
    std::vector<std::string> names;
    names.reserve(m_Passes.size());

    for (const PassData& pass : m_Passes) {
        names.push_back(pass.name);
    }

    return names;
}

void ShadowProfiler::ExportToCSV(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open CSV file: " << filename << std::endl;
        return;
    }

    // Header
    file << "Pass,Sample,TimeMs\n";

    // Data
    for (const PassData& pass : m_Passes) {
        for (u32 i = 0; i < pass.samples.size(); ++i) {
            file << pass.name << "," << i << "," << pass.samples[i] << "\n";
        }
    }

    file.close();
    std::cout << "Exported performance data to " << filename << std::endl;
}

void ShadowProfiler::Reset() {
    for (PassData& pass : m_Passes) {
        pass.samples.clear();
        pass.currentSample = 0;
        pass.beginQuery.active = false;
        pass.endQuery.active = false;
    }
}

ShadowProfiler::PassData* ShadowProfiler::FindOrCreatePass(const std::string& passName) {
    // Find existing pass
    for (PassData& pass : m_Passes) {
        if (pass.name == passName) {
            return &pass;
        }
    }

    // Create new pass
    PassData newPass;
    newPass.name = passName;
    newPass.samples.reserve(MAX_SAMPLES);
    m_Passes.push_back(newPass);

    return &m_Passes.back();
}

u32 ShadowProfiler::AllocateQuery(u32 frameIndex) {
    (void)frameIndex;  // Query index is shared across frames via m_NextQueryIndex

    if (m_NextQueryIndex >= MAX_QUERIES_PER_FRAME) {
        std::cerr << "ShadowProfiler: Out of query slots!" << std::endl;
        return MAX_QUERIES_PER_FRAME;
    }

    return m_NextQueryIndex++;
}
