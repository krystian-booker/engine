#include <engine/core/job_system.hpp>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

namespace engine::core {

namespace {

class ThreadPool {
public:
    void init(int num_threads) {
        if (num_threads <= 0) {
            num_threads = static_cast<int>(std::thread::hardware_concurrency());
            if (num_threads > 1) num_threads--; // Leave one for main thread
        }
        if (num_threads < 1) num_threads = 1;

        m_running = true;
        m_pending_jobs = 0;

        m_workers.reserve(num_threads);
        for (int i = 0; i < num_threads; ++i) {
            m_workers.emplace_back([this, i] { worker_thread(i); });
        }
        m_thread_count = num_threads;
    }

    void shutdown() {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_running = false;
        }
        m_condition.notify_all();

        for (auto& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        m_workers.clear();
        m_thread_count = 0;
    }

    void submit(std::function<void()> job) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_jobs.push(std::move(job));
            m_pending_jobs++;
        }
        m_condition.notify_one();
    }

    void wait_all() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_completion.wait(lock, [this] { return m_pending_jobs == 0; });
    }

    int thread_count() const { return m_thread_count; }

    bool is_worker_thread() const {
        auto id = std::this_thread::get_id();
        for (const auto& worker : m_workers) {
            if (worker.get_id() == id) return true;
        }
        return false;
    }

private:
    void worker_thread(int /*thread_index*/) {
        while (true) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_condition.wait(lock, [this] {
                    return !m_running || !m_jobs.empty();
                });

                if (!m_running && m_jobs.empty()) {
                    return;
                }

                job = std::move(m_jobs.front());
                m_jobs.pop();
            }

            job();

            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_pending_jobs--;
                if (m_pending_jobs == 0) {
                    m_completion.notify_all();
                }
            }
        }
    }

    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_jobs;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::condition_variable m_completion;
    std::atomic<bool> m_running{false};
    std::atomic<int> m_pending_jobs{0};
    int m_thread_count{0};
};

ThreadPool g_pool;

} // anonymous namespace

void JobSystem::init(int num_threads) {
    g_pool.init(num_threads);
}

void JobSystem::shutdown() {
    g_pool.shutdown();
}

void JobSystem::submit(std::function<void()> job) {
    g_pool.submit(std::move(job));
}

void JobSystem::wait_all() {
    g_pool.wait_all();
}

void JobSystem::parallel_for(size_t count, std::function<void(size_t, size_t)> callback) {
    if (count == 0) return;

    int num_threads = g_pool.thread_count();
    if (num_threads <= 1 || count <= 1) {
        callback(0, count);
        return;
    }

    size_t batch_size = (count + num_threads - 1) / num_threads;

    for (int i = 0; i < num_threads; ++i) {
        size_t start = i * batch_size;
        size_t end = std::min(start + batch_size, count);
        if (start >= count) break;

        g_pool.submit([&callback, start, end] {
            callback(start, end);
        });
    }

    g_pool.wait_all();
}

int JobSystem::thread_count() {
    return g_pool.thread_count();
}

bool JobSystem::is_worker_thread() {
    return g_pool.is_worker_thread();
}

} // namespace engine::core
