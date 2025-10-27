#pragma once

#include "types.h"
#include <atomic>

class LinearAllocator;

namespace JobSystem {
enum class JobPriority : u8;
struct TaskGroup;
} // namespace JobSystem

// ============================================================================
// Work-Stealing Job System
// ============================================================================
// Parallel task execution system using work-stealing for load balancing.
// Each worker thread has its own queue and can steal work from others when idle.
//
// Usage:
//   JobSystem::Init();  // Initialize with auto-detected thread count
//
//   Job* job = JobSystem::CreateJob(MyFunction, myData);
//   JobSystem::Run(job);
//   JobSystem::Wait(job);  // Block until complete
//
//   JobSystem::Shutdown();

struct Job {
    /// Function to execute - called with data parameter
    void (*function)(void* data);

    /// User data passed to function
    void* data;

    /// Number of unfinished jobs (self + children)
    /// When this reaches 0, the job is complete
    std::atomic<u32> unfinished_jobs;

    /// Parent job (nullptr if root job)
    /// When a child completes, parent's counter is decremented
    Job* parent;

    /// Optional priority band for scheduling
    JobSystem::JobPriority priority;

    /// Optional task group to signal on completion
    JobSystem::TaskGroup* group;
};

namespace JobSystem {

constexpr u32 kPriorityCount = 3;

enum class JobPriority : u8 {
    High = 0,
    Normal = 1,
    Low = 2
};

struct TaskGroup {
    std::atomic<u32> counter;

    TaskGroup() : counter(0) {}
};

/// Initialize the job system with specified number of worker threads
/// @param num_threads Number of worker threads to create (0 = auto-detect cores - 1)
void Init(u32 num_threads = 0);

/// Shutdown the job system and wait for all threads to finish
void Shutdown();

/// Create a new job with the specified function and data
/// The job is NOT automatically queued - call Run() to execute it
/// @param func Function pointer to execute
/// @param data User data passed to function
/// @param priority Priority band for the job (High/Normal/Low)
/// @param group Optional task group to signal when job completes
/// @return Pointer to created job (null on failure)
Job* CreateJob(void (*func)(void*), void* data, JobPriority priority = JobPriority::Normal, TaskGroup* group = nullptr);

/// Create a new job as a child of an existing parent job
/// When created, parent's unfinished_jobs counter is incremented
/// When child completes, parent's counter is decremented
/// @param parent Parent job (must not be null)
/// @param func Function pointer to execute
/// @param data User data passed to function
/// @param priority Priority band for the job
/// @param group Optional task group to signal when job completes (defaults to parent's group)
/// @return Pointer to created job (null on failure)
Job* CreateJobAsChild(Job* parent, void (*func)(void*), void* data, JobPriority priority = JobPriority::Normal, TaskGroup* group = nullptr);

/// Queue a job for execution
/// The job will be picked up by a worker thread (or stolen by another)
/// @param job Job to execute (must not be null)
void Run(Job* job);

/// Wait for a job to complete (blocking)
/// Spins briefly, then yields while helping to execute other jobs
/// @param job Job to wait for (must not be null)
void Wait(Job* job);

/// Set the priority band for a job before running
void SetPriority(Job* job, JobPriority priority);

/// Access the worker-local scratch allocator for the current thread
/// Returns nullptr if called outside job system threads before any job executed
LinearAllocator* GetScratchAllocator();

/// Initialize a task group counter to zero
void InitTaskGroup(TaskGroup& group);

/// Associate a job with a task group (increments the group's counter)
void AttachToTaskGroup(TaskGroup& group, Job* job);

/// Manually add work to a task group counter (for non-job producer tasks)
void AddToTaskGroup(TaskGroup& group, u32 count = 1);

/// Manually signal completion for external work tracked in a task group
void CompleteTaskGroupWork(TaskGroup& group, u32 count = 1);

/// Wait for all work tracked by the task group to complete
void Wait(TaskGroup& group);

} // namespace JobSystem
