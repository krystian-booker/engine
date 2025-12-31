#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <cstdint>

namespace engine::core {

// Macro for easy profiling scope
#define PROFILE_SCOPE(name) ::engine::core::ProfileScope _profile_scope_##__LINE__(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)

// Forward declaration
class IRenderer;

// Profile sample data
struct ProfileSample {
    std::string name;
    double start_time_ms;
    double duration_ms;
    uint32_t depth;
    uint32_t thread_id;
};

// Frame timing data
struct FrameStats {
    double frame_time_ms = 0.0;
    double update_time_ms = 0.0;
    double render_time_ms = 0.0;
    double physics_time_ms = 0.0;
    uint32_t draw_calls = 0;
    uint32_t triangles = 0;
    size_t memory_used = 0;
    int fps = 0;
};

// Profiler for measuring and reporting performance
class Profiler {
public:
    // Frame lifecycle
    static void begin_frame();
    static void end_frame();

    // Manual profiling
    static void begin_sample(const char* name);
    static void end_sample();

    // GPU profiling
    static void begin_gpu_sample(const char* name);
    static void end_gpu_sample();

    // Frame stats
    static const FrameStats& get_frame_stats();
    static void set_draw_stats(uint32_t draw_calls, uint32_t triangles);

    // Reporting
    static std::string get_report();
    static std::string get_frame_time_graph(int width = 60);

    // Overlay rendering
    static void draw_overlay(void* renderer);
    static void set_overlay_visible(bool visible);
    static bool is_overlay_visible();

    // History
    static const std::vector<double>& get_frame_time_history();
    static double get_average_frame_time();
    static double get_max_frame_time();
    static double get_min_frame_time();

    // Reset
    static void reset();

private:
    struct SampleStack {
        std::string name;
        std::chrono::high_resolution_clock::time_point start;
    };

    static std::vector<ProfileSample> s_samples;
    static std::vector<SampleStack> s_stack;
    static std::chrono::high_resolution_clock::time_point s_frame_start;
    static FrameStats s_current_stats;
    static FrameStats s_last_stats;
    static std::vector<double> s_frame_time_history;
    static size_t s_history_index;
    static bool s_overlay_visible;
    static std::mutex s_mutex;
    static uint32_t s_frame_count;
    static double s_accumulated_time;
};

// RAII profiling scope
class ProfileScope {
public:
    explicit ProfileScope(const char* name) {
        Profiler::begin_sample(name);
    }

    ~ProfileScope() {
        Profiler::end_sample();
    }

    // Non-copyable
    ProfileScope(const ProfileScope&) = delete;
    ProfileScope& operator=(const ProfileScope&) = delete;
};

// Memory tracking
class MemoryTracker {
public:
    // Allocation tracking
    static void* allocate(size_t size, const char* tag = nullptr);
    static void deallocate(void* ptr);

    // Statistics
    static size_t total_allocated();
    static size_t total_freed();
    static size_t current_usage();
    static size_t peak_usage();
    static size_t allocation_count();

    // Leak detection
    static void dump_leaks();
    static void check_leaks();

    // Tagged allocations
    static size_t get_tagged_usage(const char* tag);
    static std::string get_usage_report();

    // Reset
    static void reset();

private:
    struct Allocation {
        void* ptr;
        size_t size;
        const char* tag;
        uint32_t frame;
    };

    static std::unordered_map<void*, Allocation> s_allocations;
    static size_t s_total_allocated;
    static size_t s_total_freed;
    static size_t s_peak_usage;
    static size_t s_allocation_count;
    static std::mutex s_mutex;
};

} // namespace engine::core
