#include <engine/core/profiler.hpp>
#include <engine/core/log.hpp>
#include <engine/core/time.hpp>
#include <bgfx/bgfx.h>
#include <engine/render/debug_draw.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <cstring>
#include <format>

namespace engine::core {

// Static member initialization
std::vector<ProfileSample> Profiler::s_samples;
std::vector<Profiler::SampleStack> Profiler::s_stack;
std::chrono::high_resolution_clock::time_point Profiler::s_frame_start;
FrameStats Profiler::s_current_stats;
FrameStats Profiler::s_last_stats;
std::vector<double> Profiler::s_frame_time_history(120, 0.0);  // 2 seconds at 60fps
std::vector<double> Profiler::s_gpu_time_history(120, 0.0);
size_t Profiler::s_history_index = 0;
bool Profiler::s_overlay_visible = false;
std::mutex Profiler::s_mutex;
uint32_t Profiler::s_frame_count = 0;
double Profiler::s_accumulated_time = 0.0;

// GPU profiling state
std::vector<GPUProfileSample> Profiler::s_gpu_samples;
std::vector<GPUProfileSample> Profiler::s_gpu_samples_last_frame;
std::vector<Profiler::GPUSampleStack> Profiler::s_gpu_stack;
size_t Profiler::s_gpu_memory_usage = 0;

void Profiler::begin_frame() {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_frame_start = std::chrono::high_resolution_clock::now();
    s_samples.clear();
    s_stack.clear();
    s_current_stats = FrameStats{};
}

void Profiler::end_frame() {
    std::lock_guard<std::mutex> lock(s_mutex);

    auto now = std::chrono::high_resolution_clock::now();
    double frame_time = std::chrono::duration<double, std::milli>(now - s_frame_start).count();

    s_current_stats.frame_time_ms = frame_time;

    // Update history
    s_frame_time_history[s_history_index] = frame_time;

    // Get GPU timing from bgfx stats
    const bgfx::Stats* bgfx_stats = bgfx::getStats();
    if (bgfx_stats && bgfx_stats->gpuTimerFreq > 0) {
        double gpu_time_ms = 1000.0 * double(bgfx_stats->gpuTimeEnd - bgfx_stats->gpuTimeBegin)
                           / double(bgfx_stats->gpuTimerFreq);
        s_current_stats.gpu_time_ms = gpu_time_ms;
        s_current_stats.gpu_memory_used = bgfx_stats->gpuMemoryUsed;
    } else {
        s_current_stats.gpu_time_ms = 0.0;
    }
    s_gpu_time_history[s_history_index] = s_current_stats.gpu_time_ms;

    s_history_index = (s_history_index + 1) % s_frame_time_history.size();

    // Swap GPU sample buffers
    s_gpu_samples_last_frame = std::move(s_gpu_samples);
    s_gpu_samples.clear();

    // Calculate FPS
    s_accumulated_time += frame_time;
    s_frame_count++;
    if (s_accumulated_time >= 1000.0) {
        s_current_stats.fps = static_cast<int>(s_frame_count * 1000.0 / s_accumulated_time);
        s_accumulated_time = 0.0;
        s_frame_count = 0;
    } else {
        s_current_stats.fps = s_last_stats.fps;  // Keep previous FPS until update
    }

    s_last_stats = s_current_stats;
}

void Profiler::begin_sample(const char* name) {
    std::lock_guard<std::mutex> lock(s_mutex);

    SampleStack sample;
    sample.name = name;
    sample.start = std::chrono::high_resolution_clock::now();
    s_stack.push_back(sample);
}

void Profiler::end_sample() {
    std::lock_guard<std::mutex> lock(s_mutex);

    if (s_stack.empty()) return;

    auto& top = s_stack.back();
    auto now = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double, std::milli>(now - top.start).count();
    double start_ms = std::chrono::duration<double, std::milli>(top.start - s_frame_start).count();

    ProfileSample sample;
    sample.name = std::move(top.name);
    sample.start_time_ms = start_ms;
    sample.duration_ms = duration;
    sample.depth = static_cast<uint32_t>(s_stack.size() - 1);
    sample.thread_id = static_cast<uint32_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));

    s_samples.push_back(sample);
    s_stack.pop_back();
}

void Profiler::begin_gpu_sample(const char* name, uint16_t view_id) {
    std::lock_guard<std::mutex> lock(s_mutex);

    GPUSampleStack sample;
    sample.name = name;
    sample.view_id = view_id;
    s_gpu_stack.push_back(sample);

    // In a real implementation, this would start a GPU timing query
    // using bgfx::createOcclusionQuery() or platform-specific timing queries
    // bgfx::setViewName(view_id, name);
}

void Profiler::end_gpu_sample() {
    std::lock_guard<std::mutex> lock(s_mutex);

    if (s_gpu_stack.empty()) return;

    auto& top = s_gpu_stack.back();

    GPUProfileSample sample;
    sample.name = std::move(top.name);
    sample.view_id = top.view_id;
    sample.frame_captured = s_frame_count;

    // In a real implementation, this would end the GPU timing query
    // and retrieve results. GPU timing is asynchronous, so results
    // from queries are typically available 1-2 frames later.
    //
    // For bgfx, you would use:
    // 1. bgfx::createOcclusionQuery() to create a query
    // 2. bgfx::setCondition() to start timing
    // 3. bgfx::getResult() to retrieve timing (async)
    //
    // Alternatively, use bgfx::Stats from bgfx::getStats() which provides:
    // - gpuTimeBegin, gpuTimeEnd for frame timing
    // - gpuTimerFreq for converting to milliseconds
    //
    // For now, mark as invalid until real GPU timing is connected
    sample.valid = false;
    sample.gpu_time_ms = 0.0;

    s_gpu_samples.push_back(sample);
    s_gpu_stack.pop_back();
}

double Profiler::get_gpu_frame_time() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_last_stats.gpu_time_ms;
}

double Profiler::get_gpu_pass_time(const char* name) {
    std::lock_guard<std::mutex> lock(s_mutex);

    for (const auto& sample : s_gpu_samples_last_frame) {
        if (sample.name == name && sample.valid) {
            return sample.gpu_time_ms;
        }
    }
    return 0.0;
}

const std::vector<GPUProfileSample>& Profiler::get_gpu_samples() {
    // Note: Not thread-safe for reading, caller should ensure no concurrent writes
    return s_gpu_samples_last_frame;
}

void Profiler::set_gpu_memory_usage(size_t bytes) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_gpu_memory_usage = bytes;
}

size_t Profiler::get_gpu_memory_usage() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_gpu_memory_usage;
}

const FrameStats& Profiler::get_frame_stats() {
    return s_last_stats;
}

void Profiler::set_draw_stats(uint32_t draw_calls, uint32_t triangles) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_current_stats.draw_calls = draw_calls;
    s_current_stats.triangles = triangles;
}

std::string Profiler::get_report() {
    std::lock_guard<std::mutex> lock(s_mutex);

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);

    ss << "=== Frame Profile ===\n";
    ss << "Frame Time: " << s_last_stats.frame_time_ms << " ms\n";
    ss << "GPU Time: " << s_last_stats.gpu_time_ms << " ms\n";
    ss << "FPS: " << s_last_stats.fps << "\n";
    ss << "Draw Calls: " << s_last_stats.draw_calls << "\n";
    ss << "Triangles: " << s_last_stats.triangles << "\n";
    if (s_gpu_memory_usage > 0) {
        ss << "GPU Memory: " << s_gpu_memory_usage / (1024 * 1024) << " MB\n";
    }
    ss << "\n--- CPU Samples ---\n";

    for (const auto& sample : s_samples) {
        for (uint32_t i = 0; i < sample.depth; ++i) {
            ss << "  ";
        }
        ss << sample.name << ": " << sample.duration_ms << " ms\n";
    }

    if (!s_gpu_samples_last_frame.empty()) {
        ss << "\n--- GPU Samples ---\n";
        for (const auto& sample : s_gpu_samples_last_frame) {
            if (sample.valid) {
                ss << "[View " << sample.view_id << "] " << sample.name
                   << ": " << sample.gpu_time_ms << " ms\n";
            } else {
                ss << "[View " << sample.view_id << "] " << sample.name << ": (pending)\n";
            }
        }
    }

    return ss.str();
}

std::string Profiler::get_frame_time_graph(int width) {
    std::lock_guard<std::mutex> lock(s_mutex);

    // Find max for scaling
    double max_time = 33.33;  // Default to 30 FPS as max
    for (double t : s_frame_time_history) {
        if (t > max_time) max_time = t;
    }

    std::ostringstream ss;
    int bar_count = 8;

    size_t step = s_frame_time_history.size() / static_cast<size_t>(width);
    if (step == 0) step = 1;

    for (int i = 0; i < width; ++i) {
        size_t idx = (s_history_index + i * step) % s_frame_time_history.size();
        double normalized = s_frame_time_history[idx] / max_time;
        int bar_idx = static_cast<int>(normalized * bar_count);
        bar_idx = std::clamp(bar_idx, 0, bar_count);
        // For ASCII output, just use simple chars
        ss << (bar_idx > 4 ? '#' : (bar_idx > 2 ? '|' : (bar_idx > 0 ? '.' : ' ')));
    }

    return ss.str();
}

void Profiler::draw_overlay(void* renderer_ptr) {
    if (!s_overlay_visible) return;

    auto* renderer = static_cast<render::IRenderer*>(renderer_ptr);
    if (!renderer) return;

    const auto& stats = s_last_stats;
    float y = 10.0f;
    const float line_height = 16.0f;

    // FPS and frame time
    render::DebugDraw::text_2d(10.0f, y, std::format("FPS: {} ({:.2f}ms)",
        stats.fps, stats.frame_time_ms), render::DebugDraw::WHITE);
    y += line_height;

    // GPU time
    if (stats.gpu_time_ms > 0.0) {
        render::DebugDraw::text_2d(10.0f, y, std::format("GPU: {:.2f}ms",
            stats.gpu_time_ms), render::DebugDraw::CYAN);
        y += line_height;
    }

    // Draw calls and triangles
    render::DebugDraw::text_2d(10.0f, y, std::format("Draw calls: {} | Tris: {}",
        stats.draw_calls, stats.triangles), render::DebugDraw::YELLOW);
    y += line_height;

    // Memory usage
    size_t mem_mb = MemoryTracker::current_usage() / (1024 * 1024);
    render::DebugDraw::text_2d(10.0f, y, std::format("Memory: {} MB", mem_mb),
        render::DebugDraw::GREEN);
}

void Profiler::set_overlay_visible(bool visible) {
    s_overlay_visible = visible;
}

bool Profiler::is_overlay_visible() {
    return s_overlay_visible;
}

const std::vector<double>& Profiler::get_frame_time_history() {
    return s_frame_time_history;
}

double Profiler::get_average_frame_time() {
    std::lock_guard<std::mutex> lock(s_mutex);

    double sum = 0.0;
    int count = 0;
    for (double t : s_frame_time_history) {
        if (t > 0.0) {
            sum += t;
            count++;
        }
    }
    return count > 0 ? sum / count : 0.0;
}

double Profiler::get_max_frame_time() {
    std::lock_guard<std::mutex> lock(s_mutex);

    double max_val = 0.0;
    for (double t : s_frame_time_history) {
        if (t > max_val) max_val = t;
    }
    return max_val;
}

double Profiler::get_min_frame_time() {
    std::lock_guard<std::mutex> lock(s_mutex);

    double min_val = std::numeric_limits<double>::max();
    for (double t : s_frame_time_history) {
        if (t > 0.0 && t < min_val) min_val = t;
    }
    return min_val == std::numeric_limits<double>::max() ? 0.0 : min_val;
}

void Profiler::reset() {
    std::lock_guard<std::mutex> lock(s_mutex);

    s_samples.clear();
    s_stack.clear();
    std::fill(s_frame_time_history.begin(), s_frame_time_history.end(), 0.0);
    std::fill(s_gpu_time_history.begin(), s_gpu_time_history.end(), 0.0);
    s_history_index = 0;
    s_current_stats = FrameStats{};
    s_last_stats = FrameStats{};
    s_frame_count = 0;
    s_accumulated_time = 0.0;

    // Reset GPU profiling state
    s_gpu_samples.clear();
    s_gpu_samples_last_frame.clear();
    s_gpu_stack.clear();
    s_gpu_memory_usage = 0;
}

// MemoryTracker implementation
std::unordered_map<void*, MemoryTracker::Allocation> MemoryTracker::s_allocations;
size_t MemoryTracker::s_total_allocated = 0;
size_t MemoryTracker::s_total_freed = 0;
size_t MemoryTracker::s_peak_usage = 0;
size_t MemoryTracker::s_allocation_count = 0;
std::mutex MemoryTracker::s_mutex;

void* MemoryTracker::allocate(size_t size, const char* tag) {
    void* ptr = std::malloc(size);
    if (!ptr) return nullptr;

    std::lock_guard<std::mutex> lock(s_mutex);

    Allocation alloc;
    alloc.ptr = ptr;
    alloc.size = size;
    alloc.tag = tag;
    alloc.frame = static_cast<uint32_t>(Time::frame_count());

    s_allocations[ptr] = alloc;
    s_total_allocated += size;
    s_allocation_count++;

    size_t current = current_usage();
    if (current > s_peak_usage) {
        s_peak_usage = current;
    }

    return ptr;
}

void MemoryTracker::deallocate(void* ptr) {
    if (!ptr) return;

    std::lock_guard<std::mutex> lock(s_mutex);

    auto it = s_allocations.find(ptr);
    if (it != s_allocations.end()) {
        s_total_freed += it->second.size;
        s_allocations.erase(it);
    }

    std::free(ptr);
}

size_t MemoryTracker::total_allocated() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_total_allocated;
}

size_t MemoryTracker::total_freed() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_total_freed;
}

size_t MemoryTracker::current_usage() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_total_allocated - s_total_freed;
}

size_t MemoryTracker::peak_usage() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_peak_usage;
}

size_t MemoryTracker::allocation_count() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_allocation_count;
}

void MemoryTracker::dump_leaks() {
    std::lock_guard<std::mutex> lock(s_mutex);

    if (s_allocations.empty()) {
        return;
    }

    log(LogLevel::Warn, "=== Memory Leak Report ===");
    log(LogLevel::Warn, "Leaked allocations: {}", s_allocations.size());

    for (const auto& [ptr, alloc] : s_allocations) {
        log(LogLevel::Warn, "  Leak: {} bytes at {} (tag: {}, frame: {})",
            alloc.size, ptr, alloc.tag ? alloc.tag : "none", alloc.frame);
    }
}

void MemoryTracker::check_leaks() {
    dump_leaks();
}

size_t MemoryTracker::get_tagged_usage(const char* tag) {
    std::lock_guard<std::mutex> lock(s_mutex);

    size_t usage = 0;
    for (const auto& [ptr, alloc] : s_allocations) {
        (void)ptr;
        if (alloc.tag && tag && std::strcmp(alloc.tag, tag) == 0) {
            usage += alloc.size;
        }
    }
    return usage;
}

std::string MemoryTracker::get_usage_report() {
    std::lock_guard<std::mutex> lock(s_mutex);

    std::ostringstream ss;
    ss << "=== Memory Usage ===\n";
    ss << "Total Allocated: " << s_total_allocated / 1024 << " KB\n";
    ss << "Total Freed: " << s_total_freed / 1024 << " KB\n";
    ss << "Current Usage: " << (s_total_allocated - s_total_freed) / 1024 << " KB\n";
    ss << "Peak Usage: " << s_peak_usage / 1024 << " KB\n";
    ss << "Active Allocations: " << s_allocations.size() << "\n";

    // Group by tag
    std::unordered_map<std::string, size_t> tag_usage;
    for (const auto& [ptr, alloc] : s_allocations) {
        (void)ptr;
        std::string tag_name = alloc.tag ? alloc.tag : "untagged";
        tag_usage[tag_name] += alloc.size;
    }

    ss << "\n--- By Tag ---\n";
    for (const auto& [tag, size] : tag_usage) {
        ss << tag << ": " << size / 1024 << " KB\n";
    }

    return ss.str();
}

void MemoryTracker::reset() {
    std::lock_guard<std::mutex> lock(s_mutex);

    s_allocations.clear();
    s_total_allocated = 0;
    s_total_freed = 0;
    s_peak_usage = 0;
    s_allocation_count = 0;
}

} // namespace engine::core
