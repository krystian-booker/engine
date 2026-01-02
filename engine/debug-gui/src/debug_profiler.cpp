#include <engine/debug-gui/debug_profiler.hpp>
#include <engine/core/profiler.hpp>
#include <engine/core/input.hpp>
#include <engine/core/log.hpp>

#include <imgui.h>
#include <cstdio>

namespace engine::debug_gui {

uint32_t DebugProfiler::get_shortcut_key() const {
    return static_cast<uint32_t>(core::Key::F3);
}

void DebugProfiler::draw() {
    ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(get_title(), &m_open)) {
        ImGui::End();
        return;
    }

    // Update history if not paused
    if (!m_pause_updates) {
        const auto& stats = core::Profiler::get_frame_stats();

        m_frame_times[m_write_index] = static_cast<float>(stats.frame_time_ms);
        m_gpu_times[m_write_index] = static_cast<float>(stats.gpu_time_ms);
        m_write_index = (m_write_index + 1) % HISTORY_SIZE;
        if (m_sample_count < HISTORY_SIZE) {
            m_sample_count++;
        }
    }

    // Controls
    ImGui::Checkbox("Pause", &m_pause_updates);
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        m_frame_times.fill(0.0f);
        m_gpu_times.fill(0.0f);
        m_write_index = 0;
        m_sample_count = 0;
        core::Profiler::reset();
    }

    ImGui::Separator();

    if (ImGui::BeginTabBar("ProfilerTabs")) {
        if (ImGui::BeginTabItem("Overview")) {
            draw_frame_time_graph();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Memory")) {
            draw_memory_stats();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("GPU")) {
            draw_gpu_stats();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("CPU")) {
            draw_cpu_samples();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void DebugProfiler::draw_frame_time_graph() {
    const auto& stats = core::Profiler::get_frame_stats();

    // Summary stats
    ImGui::Text("FPS: %d", stats.fps);
    ImGui::SameLine(150);
    ImGui::Text("Frame: %.2f ms", stats.frame_time_ms);

    double avg = core::Profiler::get_average_frame_time();
    double min_time = core::Profiler::get_min_frame_time();
    double max_time = core::Profiler::get_max_frame_time();

    ImGui::Text("Avg: %.2f ms", avg);
    ImGui::SameLine(150);
    ImGui::Text("Min: %.2f ms", min_time);
    ImGui::SameLine(300);
    ImGui::Text("Max: %.2f ms", max_time);

    ImGui::Separator();

    // Frame time graph
    ImGui::Text("Frame Time (ms)");
    ImGui::SliderFloat("Scale", &m_max_frame_time, 16.6f, 100.0f, "%.1f ms");

    if (m_sample_count > 0) {
        // Getter for circular buffer access
        auto frame_time_getter = [](void* data, int idx) -> float {
            auto* profiler = static_cast<DebugProfiler*>(data);
            size_t count = profiler->m_sample_count;
            size_t write_idx = profiler->m_write_index;
            // Calculate the actual index in circular buffer (oldest sample first)
            size_t start = (count < HISTORY_SIZE) ? 0 : write_idx;
            size_t actual_idx = (start + static_cast<size_t>(idx)) % HISTORY_SIZE;
            return profiler->m_frame_times[actual_idx];
        };
        ImGui::PlotLines("##FrameTime", frame_time_getter, this,
            static_cast<int>(m_sample_count), 0, nullptr,
            0.0f, m_max_frame_time, ImVec2(-1, 80));
    }

    // Reference lines
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "60 FPS: 16.6 ms");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "30 FPS: 33.3 ms");

    ImGui::Separator();

    // Timing breakdown
    ImGui::Text("Timing Breakdown:");
    ImGui::Text("  Update: %.2f ms", stats.update_time_ms);
    ImGui::Text("  Render: %.2f ms", stats.render_time_ms);
    ImGui::Text("  Physics: %.2f ms", stats.physics_time_ms);
    ImGui::Text("  GPU: %.2f ms", stats.gpu_time_ms);

    ImGui::Separator();

    // Draw call stats
    ImGui::Text("Draw Calls: %u", stats.draw_calls);
    ImGui::Text("Triangles: %u", stats.triangles);
}

void DebugProfiler::draw_memory_stats() {
    ImGui::Text("Memory Usage");
    ImGui::Separator();

    size_t current = core::MemoryTracker::current_usage();
    size_t peak = core::MemoryTracker::peak_usage();
    size_t total_alloc = core::MemoryTracker::total_allocated();
    size_t alloc_count = core::MemoryTracker::allocation_count();

    auto format_bytes = [](size_t bytes) -> std::pair<double, const char*> {
        if (bytes >= 1024ULL * 1024 * 1024) return {bytes / (1024.0 * 1024.0 * 1024.0), "GB"};
        if (bytes >= 1024ULL * 1024) return {bytes / (1024.0 * 1024.0), "MB"};
        if (bytes >= 1024ULL) return {bytes / 1024.0, "KB"};
        return {static_cast<double>(bytes), "B"};
    };

    auto [curr_val, curr_unit] = format_bytes(current);
    auto [peak_val, peak_unit] = format_bytes(peak);
    auto [total_val, total_unit] = format_bytes(total_alloc);

    ImGui::Text("Current: %.2f %s", curr_val, curr_unit);
    ImGui::Text("Peak: %.2f %s", peak_val, peak_unit);
    ImGui::Text("Total Allocated: %.2f %s", total_val, total_unit);
    ImGui::Text("Allocation Count: %zu", alloc_count);

    ImGui::Separator();

    const auto& stats = core::Profiler::get_frame_stats();
    auto [gpu_val, gpu_unit] = format_bytes(stats.gpu_memory_used);
    ImGui::Text("GPU Memory: %.2f %s", gpu_val, gpu_unit);

    ImGui::Separator();

    if (ImGui::Button("Dump Leaks")) {
        core::MemoryTracker::dump_leaks();
    }
    ImGui::SameLine();
    if (ImGui::Button("Get Report")) {
        std::string report = core::MemoryTracker::get_usage_report();
        core::log(core::LogLevel::Info, report.c_str());
    }
}

void DebugProfiler::draw_gpu_stats() {
    ImGui::Text("GPU Performance");
    ImGui::Separator();

    double gpu_time = core::Profiler::get_gpu_frame_time();
    ImGui::Text("GPU Frame Time: %.2f ms", gpu_time);

    // GPU time graph
    if (m_sample_count > 0) {
        auto gpu_time_getter = [](void* data, int idx) -> float {
            auto* profiler = static_cast<DebugProfiler*>(data);
            size_t count = profiler->m_sample_count;
            size_t write_idx = profiler->m_write_index;
            size_t start = (count < HISTORY_SIZE) ? 0 : write_idx;
            size_t actual_idx = (start + static_cast<size_t>(idx)) % HISTORY_SIZE;
            return profiler->m_gpu_times[actual_idx];
        };
        ImGui::PlotLines("##GPUTime", gpu_time_getter, this,
            static_cast<int>(m_sample_count), 0, nullptr,
            0.0f, m_max_frame_time, ImVec2(-1, 60));
    }

    ImGui::Separator();
    ImGui::Text("GPU Passes:");

    const auto& samples = core::Profiler::get_gpu_samples();
    for (const auto& sample : samples) {
        if (sample.valid) {
            ImGui::Text("  %s: %.2f ms", sample.name.c_str(), sample.gpu_time_ms);
        }
    }

    if (samples.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  (no GPU samples)");
    }
}

void DebugProfiler::draw_cpu_samples() {
    ImGui::Text("CPU Profile Samples");
    ImGui::Separator();

    std::string report = core::Profiler::get_report();
    if (report.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(no samples collected)");
    } else {
        ImGui::TextUnformatted(report.c_str());
    }
}

} // namespace engine::debug_gui
