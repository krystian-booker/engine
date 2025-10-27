#pragma once

#include "core/types.h"
#include <cstddef>

namespace Platform {

// ============================================================================
// Window Management
// ============================================================================

struct WindowHandle;

/// Creates a platform window with the specified title and dimensions
/// Returns nullptr on failure
WindowHandle* CreateWindow(const char* title, u32 width, u32 height);

/// Destroys a previously created window
void DestroyWindow(WindowHandle* window);

/// Polls and processes window events
/// Returns true if the window should stay open, false if it should close
bool PollEvents(WindowHandle* window);

// ============================================================================
// High-Resolution Timing
// ============================================================================

/// Gets the current value of the high-resolution performance counter
u64 GetPerformanceCounter();

/// Gets the frequency of the high-resolution performance counter (ticks per second)
u64 GetPerformanceFrequency();

// ============================================================================
// Virtual Memory Management
// ============================================================================

/// Allocates virtual memory with the specified size
/// Returns nullptr on failure
void* VirtualAlloc(size_t size);

/// Frees previously allocated virtual memory
void VirtualFree(void* ptr, size_t size);

// ============================================================================
// Synchronous File I/O
// ============================================================================

struct FileHandle;

/// Opens a file for reading or writing
/// write=false: opens for reading, write=true: opens for writing (creates if doesn't exist)
/// Returns nullptr on failure
FileHandle* OpenFile(const char* path, bool write);

/// Reads bytes from a file into a buffer
/// Returns the number of bytes actually read (may be less than requested)
size_t ReadFile(FileHandle* file, void* buffer, size_t bytes);

/// Closes a previously opened file
void CloseFile(FileHandle* file);

// ============================================================================
// Threading Primitives
// ============================================================================

struct Mutex;

/// Creates a mutex for synchronization
/// Returns nullptr on failure
Mutex* CreateMutex();

/// Locks the mutex (blocks if already locked)
void Lock(Mutex* mutex);

/// Unlocks the mutex
void Unlock(Mutex* mutex);

/// Destroys a previously created mutex
void DestroyMutex(Mutex* mutex);

struct Semaphore;

/// Creates a counting semaphore
/// @param initial_count Initial signal count
/// @return Pointer to created semaphore or nullptr on failure
Semaphore* CreateSemaphore(u32 initial_count = 0);

/// Destroys a previously created semaphore
void DestroySemaphore(Semaphore* semaphore);

/// Waits for the semaphore to be signaled
/// @param semaphore Semaphore to wait on
/// @param timeout_ms Timeout in milliseconds (0xFFFFFFFF for infinite)
void WaitSemaphore(Semaphore* semaphore, u32 timeout_ms = 0xFFFFFFFF);

/// Signals (releases) the semaphore a specified number of times
/// @param semaphore Semaphore to signal
/// @param count Number of permits to release
void SignalSemaphore(Semaphore* semaphore, u32 count = 1);

} // namespace Platform
