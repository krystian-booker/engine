#include <engine/core/time.hpp>
#include <bx/timer.h>

namespace engine::core {

static int64_t s_start_time = 0;
static int64_t s_last_time = 0;
static double s_delta_time = 0.0;
static double s_total_time = 0.0;
static uint64_t s_frame_count = 0;

void Time::init() {
    s_start_time = bx::getHPCounter();
    s_last_time = s_start_time;
}

void Time::update() {
    int64_t now = bx::getHPCounter();
    int64_t freq = bx::getHPFrequency();
    s_delta_time = double(now - s_last_time) / double(freq);
    s_total_time = double(now - s_start_time) / double(freq);
    s_last_time = now;
    s_frame_count++;
}

double Time::delta_time() { return s_delta_time; }
double Time::total_time() { return s_total_time; }
uint64_t Time::frame_count() { return s_frame_count; }

} // namespace engine::core
