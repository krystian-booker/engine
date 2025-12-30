#pragma once

#include <functional>

namespace engine::core {

struct JobSystem {
    static void init(int num_threads = 0);
    static void shutdown();
    static void submit(std::function<void()> job);
    static void wait_all();
};

} // namespace engine::core
