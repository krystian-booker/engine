#pragma once

#include <functional>
#include <future>
#include <cstdint>

namespace engine::core {

struct JobSystem {
    // Initialize the job system with specified number of threads
    // If num_threads is 0, uses hardware_concurrency - 1
    static void init(int num_threads = 0);
    static void shutdown();

    // Submit a job for execution
    static void submit(std::function<void()> job);

    // Submit a job and get a future for the result
    template<typename F, typename R = std::invoke_result_t<F>>
    static std::future<R> submit_with_result(F&& func);

    // Wait for all submitted jobs to complete
    static void wait_all();

    // Parallel for loop - splits work across threads
    // Callback receives (start_index, end_index)
    static void parallel_for(size_t count, std::function<void(size_t, size_t)> callback);

    // Get number of worker threads
    static int thread_count();

    // Check if running on a worker thread
    static bool is_worker_thread();
};

// Template implementation
template<typename F, typename R>
std::future<R> JobSystem::submit_with_result(F&& func) {
    auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(func));
    auto future = task->get_future();
    submit([task]() { (*task)(); });
    return future;
}

} // namespace engine::core
