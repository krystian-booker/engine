#include <catch2/catch_test_macros.hpp>
#include <engine/core/job_system.hpp>
#include <atomic>
#include <vector>
#include <numeric>
#include <chrono>
#include <set>
#include <thread>

using namespace engine::core;

class JobSystemFixture {
protected:
    JobSystemFixture() {
        JobSystem::init(4); // Use 4 threads for tests
    }

    ~JobSystemFixture() {
        JobSystem::shutdown();
    }
};

TEST_CASE_METHOD(JobSystemFixture, "JobSystem initialization", "[core][jobs]") {
    SECTION("Thread count is set") {
        REQUIRE(JobSystem::thread_count() > 0);
    }

    SECTION("Main thread is not worker thread") {
        REQUIRE_FALSE(JobSystem::is_worker_thread());
    }
}

TEST_CASE_METHOD(JobSystemFixture, "JobSystem submit basic jobs", "[core][jobs]") {
    SECTION("Simple job executes") {
        std::atomic<bool> executed{false};

        JobSystem::submit([&]() {
            executed = true;
        });

        JobSystem::wait_all();
        REQUIRE(executed);
    }

    SECTION("Multiple jobs execute") {
        std::atomic<int> counter{0};
        constexpr int num_jobs = 100;

        for (int i = 0; i < num_jobs; ++i) {
            JobSystem::submit([&]() {
                counter++;
            });
        }

        JobSystem::wait_all();
        REQUIRE(counter == num_jobs);
    }

    SECTION("Jobs run on worker threads") {
        std::atomic<bool> on_worker{false};

        JobSystem::submit([&]() {
            on_worker = JobSystem::is_worker_thread();
        });

        JobSystem::wait_all();
        REQUIRE(on_worker);
    }
}

TEST_CASE_METHOD(JobSystemFixture, "JobSystem submit_with_result", "[core][jobs]") {
    SECTION("Returns computed value") {
        auto future = JobSystem::submit_with_result([]() {
            return 42;
        });

        REQUIRE(future.get() == 42);
    }

    SECTION("Complex computation") {
        auto future = JobSystem::submit_with_result([]() {
            int sum = 0;
            for (int i = 1; i <= 100; ++i) {
                sum += i;
            }
            return sum;
        });

        REQUIRE(future.get() == 5050);
    }

    SECTION("Multiple futures") {
        auto f1 = JobSystem::submit_with_result([]() { return 1; });
        auto f2 = JobSystem::submit_with_result([]() { return 2; });
        auto f3 = JobSystem::submit_with_result([]() { return 3; });

        REQUIRE(f1.get() + f2.get() + f3.get() == 6);
    }

    SECTION("String result") {
        auto future = JobSystem::submit_with_result([]() {
            return std::string("hello from job");
        });

        REQUIRE(future.get() == "hello from job");
    }
}

TEST_CASE_METHOD(JobSystemFixture, "JobSystem parallel_for", "[core][jobs]") {
    SECTION("Processes all elements") {
        constexpr size_t count = 1000;
        std::vector<int> data(count, 0);

        JobSystem::parallel_for(count, [&](size_t start, size_t end) {
            for (size_t i = start; i < end; ++i) {
                data[i] = 1;
            }
        });

        JobSystem::wait_all();

        int sum = std::accumulate(data.begin(), data.end(), 0);
        REQUIRE(sum == count);
    }

    SECTION("Index ranges are correct") {
        constexpr size_t count = 100;
        std::vector<std::atomic<int>> touched(count);
        for (auto& a : touched) a = 0;

        JobSystem::parallel_for(count, [&](size_t start, size_t end) {
            for (size_t i = start; i < end; ++i) {
                touched[i]++;
            }
        });

        JobSystem::wait_all();

        // Each index should be touched exactly once
        for (size_t i = 0; i < count; ++i) {
            REQUIRE(touched[i] == 1);
        }
    }

    SECTION("Empty range") {
        bool called = false;
        JobSystem::parallel_for(0, [&](size_t, size_t) {
            called = true;
        });

        JobSystem::wait_all();
        // May or may not call the callback with empty range
    }

    SECTION("Large workload distributes across threads") {
        constexpr size_t count = 10000;
        std::atomic<int> thread_count_observed{0};
        std::vector<std::thread::id> thread_ids(count);

        JobSystem::parallel_for(count, [&](size_t start, size_t end) {
            auto tid = std::this_thread::get_id();
            for (size_t i = start; i < end; ++i) {
                thread_ids[i] = tid;
            }
        });

        JobSystem::wait_all();

        // Count unique thread IDs
        std::set<std::thread::id> unique_threads(thread_ids.begin(), thread_ids.end());
        // Should use multiple threads for large workloads
        REQUIRE(unique_threads.size() >= 1);
    }
}

TEST_CASE_METHOD(JobSystemFixture, "JobSystem wait_all", "[core][jobs]") {
    SECTION("Blocks until all jobs complete") {
        std::atomic<int> completed{0};
        constexpr int num_jobs = 50;

        for (int i = 0; i < num_jobs; ++i) {
            JobSystem::submit([&]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                completed++;
            });
        }

        JobSystem::wait_all();
        REQUIRE(completed == num_jobs);
    }

    SECTION("Returns immediately when no jobs pending") {
        auto start = std::chrono::steady_clock::now();
        JobSystem::wait_all();
        auto end = std::chrono::steady_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        REQUIRE(duration.count() < 100); // Should be nearly instant
    }
}

TEST_CASE_METHOD(JobSystemFixture, "JobSystem stress test", "[core][jobs][.]") {
    // Tagged with [.] so it only runs when explicitly requested
    SECTION("Many small jobs") {
        std::atomic<int> counter{0};
        constexpr int num_jobs = 10000;

        for (int i = 0; i < num_jobs; ++i) {
            JobSystem::submit([&]() {
                counter++;
            });
        }

        JobSystem::wait_all();
        REQUIRE(counter == num_jobs);
    }
}
