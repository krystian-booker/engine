#pragma once

#include <cstdint>

namespace engine::core {

struct Time {
    static void init();
    static void update();

    static double delta_time();
    static double total_time();
    static uint64_t frame_count();
};

} // namespace engine::core
