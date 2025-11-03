#pragma once

#include "core/types.h"
#include <cstddef>
#include <memory>
#include <string>

namespace Platform {

// ============================================================================
// Window Management
// ============================================================================

struct WindowHandle;

/// Custom deleter for WindowHandle (calls platform-specific cleanup)
struct WindowHandleDeleter {
    void operator()(WindowHandle* window) const;
};

/// Smart pointer type for WindowHandle with automatic cleanup
using WindowPtr = std::unique_ptr<WindowHandle, WindowHandleDeleter>;

/// Creates a platform window with the specified title and dimensions
/// Returns nullptr on failure
WindowPtr CreateWindow(const char* title, u32 width, u32 height);

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

/// Custom deleter for FileHandle (calls platform-specific cleanup)
struct FileHandleDeleter {
    void operator()(FileHandle* file) const;
};

/// Smart pointer type for FileHandle with automatic cleanup
using FilePtr = std::unique_ptr<FileHandle, FileHandleDeleter>;

/// Opens a file for reading or writing
/// write=false: opens for reading, write=true: opens for writing (creates if doesn't exist)
/// Returns nullptr on failure
FilePtr OpenFile(const char* path, bool write);

/// Reads bytes from a file into a buffer
/// Returns the number of bytes actually read (may be less than requested)
size_t ReadFile(FileHandle* file, void* buffer, size_t bytes);

// ============================================================================
// Threading Primitives
// ============================================================================

struct Mutex;

/// Custom deleter for Mutex (calls platform-specific cleanup)
struct MutexDeleter {
    void operator()(Mutex* mutex) const;
};

/// Smart pointer type for Mutex with automatic cleanup
using MutexPtr = std::unique_ptr<Mutex, MutexDeleter>;

/// Creates a mutex for synchronization
/// Returns nullptr on failure
MutexPtr CreateMutex();

/// Locks the mutex (blocks if already locked)
void Lock(Mutex* mutex);

/// Unlocks the mutex
void Unlock(Mutex* mutex);

struct Semaphore;

/// Custom deleter for Semaphore (calls platform-specific cleanup)
struct SemaphoreDeleter {
    void operator()(Semaphore* semaphore) const;
};

/// Smart pointer type for Semaphore with automatic cleanup
using SemaphorePtr = std::unique_ptr<Semaphore, SemaphoreDeleter>;

/// Creates a counting semaphore
/// @param initial_count Initial signal count
/// @return Pointer to created semaphore or nullptr on failure
SemaphorePtr CreateSemaphore(u32 initial_count = 0);

/// Waits for the semaphore to be signaled
/// @param semaphore Semaphore to wait on
/// @param timeout_ms Timeout in milliseconds (0xFFFFFFFF for infinite)
void WaitSemaphore(Semaphore* semaphore, u32 timeout_ms = 0xFFFFFFFF);

/// Signals (releases) the semaphore a specified number of times
/// @param semaphore Semaphore to signal
/// @param count Number of permits to release
void SignalSemaphore(Semaphore* semaphore, u32 count = 1);

// ============================================================================
// Application Data Directory
// ============================================================================

/// Gets the platform-specific application data directory path
/// Windows: %APPDATA%\appName (e.g., C:\Users\Username\AppData\Roaming\appName)
/// Linux: ~/.config/appName
/// macOS: ~/Library/Application Support/appName
/// Creates the directory if it doesn't exist
/// @param appName Name of the application subdirectory
/// @return Full path to the application data directory, or empty string on failure
std::string GetAppDataDirectory(const char* appName);

} // namespace Platform
