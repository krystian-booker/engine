#include "job_system.h"
#include "memory.h"
#include "platform/platform.h"
#include <thread>
#include <deque>
#include <vector>
#include <random>
#include <array>

namespace JobSystem {

// ============================================================================
// Internal Data Structures
// ============================================================================

/// Per-thread work queue with mutex protection (Day 1 implementation)
struct WorkQueue {
    std::array<std::deque<Job*>, kPriorityCount> jobs;
    Platform::MutexPtr mutex;

    WorkQueue() = default;

    void Init() {
        mutex = Platform::CreateMutex();
    }

    void Shutdown() {
        if (mutex) {
        // Mutex automatically cleaned up by unique_ptr
        mutex.reset();
        }
    }

    /// Push job to the front of the queue (LIFO for own thread)
    void Push(Job* job) {
        Platform::Lock(mutex.get());
        u32 priority_index = static_cast<u32>(job->priority);
        if (priority_index >= kPriorityCount) {
            priority_index = static_cast<u32>(JobPriority::Normal);
        }
        jobs[priority_index].push_front(job);
        Platform::Unlock(mutex.get());
    }

    /// Pop job from the front of the queue (LIFO for own thread)
    Job* Pop() {
        Job* job = nullptr;
        Platform::Lock(mutex.get());
        for (u32 priority = 0; priority < kPriorityCount; ++priority) {
            auto& queue = jobs[priority];
            if (!queue.empty()) {
                job = queue.front();
                queue.pop_front();
                break;
            }
        }
        Platform::Unlock(mutex.get());
        return job;
    }

    /// Steal job from the back of the queue (FIFO for stealing - better cache locality)
    Job* Steal() {
        Job* job = nullptr;
        Platform::Lock(mutex.get());
        for (u32 priority = 0; priority < kPriorityCount; ++priority) {
            auto& queue = jobs[priority];
            if (!queue.empty()) {
                job = queue.back();
                queue.pop_back();
                break;
            }
        }
        Platform::Unlock(mutex.get());
        return job;
    }
};

// ============================================================================
// Global State
// ============================================================================

static std::atomic<bool> g_initialized{false};
static std::atomic<bool> g_shutdown{false};

static u32 g_num_threads = 0;
static std::vector<std::thread> g_worker_threads;
static std::vector<WorkQueue> g_work_queues;
static Platform::SemaphorePtr g_work_semaphore;
static std::atomic<u32> g_submission_index{0};
static std::vector<LinearAllocator> g_worker_scratch_allocators;
static constexpr size_t kScratchAllocatorSize = 128 * 1024; // 128 KB per worker

static PoolAllocator<Job, 64> g_job_allocator;

/// Thread-local index for identifying which queue belongs to this thread
thread_local u32 t_thread_index = 0xFFFFFFFF;
thread_local LinearAllocator* t_scratch_allocator = nullptr;
thread_local LinearAllocator t_external_scratch_allocator;
thread_local bool t_external_scratch_initialized = false;

// ============================================================================
// Worker Thread Implementation
// ============================================================================

/// Attempt to steal work for a worker thread using round-robin starting at a rotating offset
static Job* StealWorkForWorker(u32 thread_index) {
    if (g_num_threads <= 1) {
        return nullptr;
    }

    thread_local std::mt19937 rng(std::random_device{}());
    thread_local u32 steal_offset = rng() % g_num_threads;

    Job* job = nullptr;
    for (u32 attempt = 0; attempt < g_num_threads && !job; ++attempt) {
        u32 victim = (thread_index + 1 + steal_offset + attempt) % g_num_threads;
        if (victim == thread_index) {
            continue;
        }

        job = g_work_queues[victim].Steal();
    }

    if (g_num_threads > 0) {
        steal_offset = (steal_offset + 1) % g_num_threads;
    }
    return job;
}

/// Attempt to steal work from any queue (used by Waiters on external threads)
static Job* StealWorkFromAny(u32 avoid_thread) {
    if (g_num_threads == 0) {
        return nullptr;
    }

    static thread_local std::mt19937 rng(std::random_device{}());
    u32 start = rng() % g_num_threads;

    for (u32 attempt = 0; attempt < g_num_threads; ++attempt) {
        u32 victim = (start + attempt) % g_num_threads;
        if (victim == avoid_thread) {
            continue;
        }

        Job* job = g_work_queues[victim].Steal();
        if (job) {
            return job;
        }
    }

    return nullptr;
}

/// Decrement the unfinished counter, cascade to parent, and free when complete
static void TryCompleteAndFree(Job* job) {
    if (!job) return;

    Job* parent = job->parent;
    TaskGroup* group = job->group;
    if (job->unfinished_jobs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        job->group = nullptr;
        TryCompleteAndFree(parent);
        if (group) {
            CompleteTaskGroupWork(*group);
        }
        g_job_allocator.Free(job);
    }
}

/// Execute a job and handle counter decrements
static void ExecuteJob(Job* job) {
    if (!job) return;

    LinearAllocator* scratch = t_scratch_allocator;
    if (!scratch) {
        if (!t_external_scratch_initialized) {
            t_external_scratch_allocator.Init(kScratchAllocatorSize);
            t_external_scratch_initialized = true;
        }
        scratch = &t_external_scratch_allocator;
        t_scratch_allocator = scratch;
    }

    if (scratch) {
        scratch->Reset();
    }

    job->function(job->data);

    if (scratch) {
        scratch->Reset();
    }

    TryCompleteAndFree(job);
}

/// Worker thread main loop
static void WorkerThreadMain(u32 thread_index) {
    t_thread_index = thread_index;
    t_scratch_allocator = &g_worker_scratch_allocators[thread_index];

    while (!g_shutdown.load(std::memory_order_acquire)) {
        Job* job = nullptr;

        // 1. Try to get job from own queue
        job = g_work_queues[thread_index].Pop();

        // 2. If own queue is empty, try to steal from another thread
        if (!job) {
            job = StealWorkForWorker(thread_index);
        }

        // 3. If we got a job, execute it
        if (job) {
            ExecuteJob(job);
        } else {
            // 4. No work available, wait on semaphore to avoid busy-waiting
            Platform::WaitSemaphore(g_work_semaphore.get());
        }
    }
}

// ============================================================================
// Public API Implementation
// ============================================================================

void Init(u32 num_threads) {
    if (g_initialized.load(std::memory_order_acquire)) {
        return; // Already initialized
    }

    // Detect number of cores if not specified
    if (num_threads == 0) {
        u32 cores = std::thread::hardware_concurrency();
        // Use N-1 worker threads (leave one core for main thread)
        num_threads = cores > 1 ? cores - 1 : 1;
    }

    g_num_threads = num_threads;
    g_shutdown.store(false, std::memory_order_relaxed);

    // Initialize work queues
    g_work_queues.resize(g_num_threads);
    for (u32 i = 0; i < g_num_threads; i++) {
        g_work_queues[i].Init();
    }

    g_worker_scratch_allocators.resize(g_num_threads);
    for (u32 i = 0; i < g_num_threads; ++i) {
        g_worker_scratch_allocators[i].Init(kScratchAllocatorSize);
    }

    g_work_semaphore = Platform::CreateSemaphore(0);
    if (!g_work_semaphore) {
        for (u32 i = 0; i < g_num_threads; ++i) {
            g_work_queues[i].Shutdown();
        }
        for (u32 i = 0; i < g_worker_scratch_allocators.size(); ++i) {
            g_worker_scratch_allocators[i].Shutdown();
        }
        g_work_queues.clear();
        g_worker_scratch_allocators.clear();
        g_num_threads = 0;
        return;
    }

    g_submission_index.store(0, std::memory_order_relaxed);

    // Spawn worker threads
    g_worker_threads.reserve(g_num_threads);
    for (u32 i = 0; i < g_num_threads; i++) {
        g_worker_threads.emplace_back(WorkerThreadMain, i);
    }

    g_initialized.store(true, std::memory_order_release);
}

void Shutdown() {
    if (!g_initialized.load(std::memory_order_acquire)) {
        return;
    }

    // Signal all threads to shut down
    g_shutdown.store(true, std::memory_order_release);
    if (g_work_semaphore) {
        Platform::SignalSemaphore(g_work_semaphore.get(), g_num_threads > 0 ? g_num_threads : 1);
    }

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

    for (u32 i = 0; i < g_worker_scratch_allocators.size(); ++i) {
        g_worker_scratch_allocators[i].Shutdown();
    }

    g_worker_threads.clear();
    g_work_queues.clear();
    g_worker_scratch_allocators.clear();
    g_num_threads = 0;
    g_submission_index.store(0, std::memory_order_relaxed);

    if (g_work_semaphore) {
        g_work_semaphore = nullptr;
    }

    g_initialized.store(false, std::memory_order_release);
    g_shutdown.store(false, std::memory_order_relaxed);
}

Job* CreateJob(void (*func)(void*), void* data, JobPriority priority, TaskGroup* group) {
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
    job->priority = priority;
    job->group = group;
    if (group) {
        group->counter.fetch_add(1, std::memory_order_relaxed);
    }

    return job;
}

Job* CreateJobAsChild(Job* parent, void (*func)(void*), void* data, JobPriority priority, TaskGroup* group) {
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
    JobPriority effective_priority = priority;
    if (priority == JobPriority::Normal) {
        effective_priority = parent->priority;
    }
    job->priority = effective_priority;
    job->group = group ? group : parent->group;
    if (job->group) {
        job->group->counter.fetch_add(1, std::memory_order_relaxed);
    }

    // Increment parent's unfinished counter (atomic)
    parent->unfinished_jobs.fetch_add(1, std::memory_order_relaxed);

    return job;
}

void Run(Job* job) {
    if (!job || !g_initialized.load(std::memory_order_acquire)) {
        return;
    }

    // Get the current thread's queue index
    u32 thread_index = t_thread_index;

    // If called from main thread or unknown thread, use queue 0
    if (thread_index >= g_num_threads) {
        u32 rr = g_submission_index.fetch_add(1, std::memory_order_relaxed);
        thread_index = g_num_threads > 0 ? rr % g_num_threads : 0;
    }

    // Push job to the queue
    g_work_queues[thread_index].Push(job);

    if (g_work_semaphore) {
        Platform::SignalSemaphore(g_work_semaphore.get(), 1);
    }
}

void Wait(Job* job) {
    if (!job) {
        return;
    }

    // Hold a reference so the job memory remains valid while we wait
    job->unfinished_jobs.fetch_add(1, std::memory_order_acq_rel);

    // Brief spin-wait (avoid context switches for short jobs)
    constexpr u32 MAX_SPIN_COUNT = 100;
    u32 spin_count = 0;

    while (job->unfinished_jobs.load(std::memory_order_acquire) > 1) {
        if (spin_count < MAX_SPIN_COUNT) {
            spin_count++;
            // Just spin
        } else {
            // After spinning, help execute jobs while waiting
            Job* work = nullptr;

            // Try to get work from a random queue
            u32 current_thread = t_thread_index < g_num_threads ? t_thread_index : 0xFFFFFFFF;
            work = StealWorkFromAny(current_thread);

            if (work) {
                ExecuteJob(work);
            } else {
                // No work available, yield
                std::this_thread::yield();
            }
        }
    }

    // Release the reference we acquired above
    TryCompleteAndFree(job);
}

void SetPriority(Job* job, JobPriority priority) {
    if (!job) {
        return;
    }
    job->priority = priority;
}

LinearAllocator* GetScratchAllocator() {
    return t_scratch_allocator;
}

void InitTaskGroup(TaskGroup& group) {
    group.counter.store(0, std::memory_order_relaxed);
}

void AttachToTaskGroup(TaskGroup& group, Job* job) {
    if (!job) {
        return;
    }

    if (job->group == &group) {
        return;
    }

    if (!job->group) {
        job->group = &group;
        group.counter.fetch_add(1, std::memory_order_relaxed);
    }
}

void AddToTaskGroup(TaskGroup& group, u32 count) {
    if (count == 0) {
        return;
    }
    group.counter.fetch_add(count, std::memory_order_relaxed);
}

void CompleteTaskGroupWork(TaskGroup& group, u32 count) {
    if (count == 0) {
        return;
    }
    group.counter.fetch_sub(count, std::memory_order_acq_rel);
}

void Wait(TaskGroup& group) {
    constexpr u32 MAX_SPIN_COUNT = 100;
    u32 spin_count = 0;

    while (group.counter.load(std::memory_order_acquire) > 0) {
        if (spin_count < MAX_SPIN_COUNT) {
            spin_count++;
        } else {
            Job* work = StealWorkFromAny(t_thread_index < g_num_threads ? t_thread_index : 0xFFFFFFFF);
            if (work) {
                ExecuteJob(work);
            } else {
                std::this_thread::yield();
            }
        }
    }
}

} // namespace JobSystem
