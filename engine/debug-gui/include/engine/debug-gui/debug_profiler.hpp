#pragma once

#include <engine/debug-gui/debug_window.hpp>
#include <array>
#include <cstddef>

namespace engine::debug_gui {

// Debug profiler window showing performance metrics
class DebugProfiler : public IDebugWindow {
public:
    const char* get_name() const override { return "profiler"; }
    const char* get_title() const override { return "Performance"; }
    uint32_t get_shortcut_key() const override;

    void draw() override;

private:
    void draw_frame_time_graph();
    void draw_memory_stats();
    void draw_gpu_stats();
    void draw_cpu_samples();

    // Circular buffer for efficient O(1) insertion
    static constexpr size_t HISTORY_SIZE = 120;
    std::array<float, HISTORY_SIZE> m_frame_times{};
    std::array<float, HISTORY_SIZE> m_gpu_times{};
    size_t m_write_index = 0;
    size_t m_sample_count = 0;

    float m_max_frame_time = 33.3f; // 30 FPS target as default scale
    bool m_pause_updates = false;
};

} // namespace engine::debug_gui
