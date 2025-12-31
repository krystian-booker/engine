#include <engine/core/time.hpp>
#include <chrono>

namespace engine::core {

using Clock = std::chrono::steady_clock;

static Clock::time_point s_start_time;
static Clock::time_point s_last_time;
static double s_delta_time = 0.0;
static double s_total_time = 0.0;
static uint64_t s_frame_count = 0;

void Time::init() {
    s_start_time = Clock::now();
    s_last_time = s_start_time;
}

void Time::update() {
    auto now = Clock::now();
    s_delta_time = std::chrono::duration<double>(now - s_last_time).count();
    s_total_time = std::chrono::duration<double>(now - s_start_time).count();
    s_last_time = now;
    s_frame_count++;
}

double Time::delta_time() { return s_delta_time; }
double Time::total_time() { return s_total_time; }
uint64_t Time::frame_count() { return s_frame_count; }

} // namespace engine::core
