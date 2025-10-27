#pragma once
#include "core/types.h"

// State tracking for asynchronous texture loading
enum class AsyncLoadState : u8 {
    Pending,        // Job created, not yet started
    Loading,        // File I/O in progress on worker thread
    ReadyForUpload, // Image data loaded, awaiting GPU upload on main thread
    Uploading,      // GPU upload in progress (main thread)
    Completed,      // Fully loaded and uploaded to GPU
    Failed          // Load failed (file not found, decode error, etc.)
};
