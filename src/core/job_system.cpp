#include "job_system.h"
#include "memory.h"
#include "platform/platform.h"
#include <thread>
#include <deque>
#include <vector>
#include <random>

namespace JobSystem {

// ============================================================================
// Internal Data Structures
// ============================================================================

/// Per-thread work queue with mutex protection (Day 1 implementation)
struct WorkQueue {
    std::deque<Job*> jobs;
    Platform::Mutex* mutex;

    WorkQueue() : mutex(nullptr) {}

    void Init() {
        mutex = Platform::CreateMutex();
    }

    void Shutdown() {
        if (mutex) {
            Platform::DestroyMutex(mutex);
            mutex = nullptr;
        }
    }

    /// Push job to the front of the queue (LIFO for own thread)
    void Push(Job* job) {
        Platform::Lock(mutex);
        jobs.push_front(job);
        Platform::Unlock(mutex);
    }

    /// Pop job from the front of the queue (LIFO for own thread)
    Job* Pop() {
        Job* job = nullptr;
        Platform::Lock(mutex);
        if (!jobs.empty()) {
            job = jobs.front();
            jobs.pop_front();
        }
        Platform::Unlock(mutex);
        return job;
    }

    /// Steal job from the back of the queue (FIFO for stealing - better cache locality)
    Job* Steal() {
        Job* job = nullptr;
        Platform::Lock(mutex);
        if (!jobs.empty()) {
            job = jobs.back();
            jobs.pop_back();
        }
        Platform::Unlock(mutex);
        return job;
    }
};

// ============================================================================
// Global State
// ============================================================================

static bool g_initialized = false;
static bool g_shutdown = false;

static u32 g_num_threads = 0;
static std::vector<std::thread> g_worker_threads;
static std::vector<WorkQueue> g_work_queues;

static PoolAllocator<Job, 64> g_job_allocator;

/// Thread-local index for identifying which queue belongs to this thread
thread_local u32 t_thread_index = 0xFFFFFFFF;

// ============================================================================
// Worker Thread Implementation
// ============================================================================

/// Get a random thread index different from current thread (for work stealing)
static u32 GetRandomVictimThread(u32 current_thread) {
    if (g_num_threads <= 1) {
        return current_thread;
    }

    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<u32> dist(0, g_num_threads - 1);

    u32 victim;
    do {
        victim = dist(rng);
    } while (victim == current_thread);

    return victim;
}

/// Execute a job and handle counter decrements
static void ExecuteJob(Job* job) {
    if (!job) return;

    // Call the job function
    job->function(job->data);

    // Decrement this job's counter
    u32 unfinished = job->unfinished_jobs.fetch_sub(1, std::memory_order_release);

    // If this was the last unfinished job (counter was 1, now 0)
    if (unfinished == 1) {
        // Decrement parent's counter if exists
        if (job->parent) {
            u32 parent_unfinished = job->parent->unfinished_jobs.fetch_sub(1, std::memory_order_release);

            // If parent also completed, it will be cleaned up when someone waits on it
            // or when it's the last reference
            (void)parent_unfinished;
        }

        // Job is complete, free it
        g_job_allocator.Free(job);
    }
}

/// Worker thread main loop
static void WorkerThreadMain(u32 thread_index) {
    t_thread_index = thread_index;

    while (!g_shutdown) {
        Job* job = nullptr;

        // 1. Try to get job from own queue
        job = g_work_queues[thread_index].Pop();

        // 2. If own queue is empty, try to steal from another thread
        if (!job) {
            u32 victim = GetRandomVictimThread(thread_index);
            job = g_work_queues[victim].Steal();
        }

        // 3. If we got a job, execute it
        if (job) {
            ExecuteJob(job);
        } else {
            // 4. No work available, yield to avoid busy-waiting
            std::this_thread::yield();
        }
    }
}

// ============================================================================
// Public API Implementation
// ============================================================================

void Init(u32 num_threads) {
    if (g_initialized) {
        return; // Already initialized
    }

    // Detect number of cores if not specified
    if (num_threads == 0) {
        u32 cores = std::thread::hardware_concurrency();
        // Use N-1 worker threads (leave one core for main thread)
        num_threads = cores > 1 ? cores - 1 : 1;
    }

    g_num_threads = num_threads;
    g_shutdown = false;

    // Initialize work queues
    g_work_queues.resize(g_num_threads);
    for (u32 i = 0; i < g_num_threads; i++) {
        g_work_queues[i].Init();
    }

    // Spawn worker threads
    g_worker_threads.reserve(g_num_threads);
    for (u32 i = 0; i < g_num_threads; i++) {
        g_worker_threads.emplace_back(WorkerThreadMain, i);
    }

    g_initialized = true;
}

void Shutdown() {
    if (!g_initialized) {
        return;
    }

    // Signal all threads to shut down
    g_shutdown = true;

    // Wait for all worker threads to finish
    for (auto& thread : g_worker_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    // Clean up work queues
    for (u32 i = 0; i < g_num_threads; i++) {
        // Free any remaining jobs in the queues
        Job* job;
        while ((job = g_work_queues[i].Pop()) != nullptr) {
            g_job_allocator.Free(job);
        }
        g_work_queues[i].Shutdown();
    }

    g_worker_threads.clear();
    g_work_queues.clear();
    g_num_threads = 0;
    g_initialized = false;
}

Job* CreateJob(void (*func)(void*), void* data) {
    if (!func) {
        return nullptr;
    }

    Job* job = g_job_allocator.Alloc();
    if (!job) {
        return nullptr;
    }

    job->function = func;
    job->data = data;
    job->unfinished_jobs.store(1, std::memory_order_relaxed); // 1 = the job itself
    job->parent = nullptr;

    return job;
}

Job* CreateJobAsChild(Job* parent, void (*func)(void*), void* data) {
    if (!parent || !func) {
        return nullptr;
    }

    Job* job = g_job_allocator.Alloc();
    if (!job) {
        return nullptr;
    }

    job->function = func;
    job->data = data;
    job->unfinished_jobs.store(1, std::memory_order_relaxed); // 1 = the job itself
    job->parent = parent;

    // Increment parent's unfinished counter (atomic)
    parent->unfinished_jobs.fetch_add(1, std::memory_order_acquire);

    return job;
}

void Run(Job* job) {
    if (!job || !g_initialized) {
        return;
    }

    // Get the current thread's queue index
    u32 thread_index = t_thread_index;

    // If called from main thread or unknown thread, use queue 0
    if (thread_index >= g_num_threads) {
        thread_index = 0;
    }

    // Push job to the queue
    g_work_queues[thread_index].Push(job);
}

void Wait(Job* job) {
    if (!job) {
        return;
    }

    // Brief spin-wait (avoid context switches for short jobs)
    constexpr u32 MAX_SPIN_COUNT = 100;
    u32 spin_count = 0;

    while (job->unfinished_jobs.load(std::memory_order_acquire) > 0) {
        if (spin_count < MAX_SPIN_COUNT) {
            spin_count++;
            // Just spin
        } else {
            // After spinning, help execute jobs while waiting
            Job* work = nullptr;

            // Try to get work from a random queue
            u32 queue_index = GetRandomVictimThread(t_thread_index >= g_num_threads ? 0 : t_thread_index);
            work = g_work_queues[queue_index].Steal();

            if (work) {
                ExecuteJob(work);
            } else {
                // No work available, yield
                std::this_thread::yield();
            }
        }
    }
}

} // namespace JobSystem
