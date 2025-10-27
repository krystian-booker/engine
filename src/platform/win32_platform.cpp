#include "platform.h"

#ifdef PLATFORM_WINDOWS

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// Undefine Windows macros that conflict with our function names
#undef CreateWindow
#undef DestroyWindow
#undef CreateMutex
#undef CreateSemaphore
#undef ReadFile

namespace Platform {

// ============================================================================
// Window Implementation
// ============================================================================

struct WindowHandle {
    HWND hwnd;
    bool shouldClose;
};

static const char* WINDOW_CLASS_NAME = "EngineWindowClass";
static bool windowClassRegistered = false;

// Window procedure callback
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WindowHandle* window = reinterpret_cast<WindowHandle*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_CLOSE:
            if (window) {
                window->shouldClose = true;
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_SIZE: {
            // Handle window resize
            // UINT width = LOWORD(lParam);
            // UINT height = HIWORD(lParam);
            // Unused for now - will be used when implementing resize callbacks
            return 0;
        }

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

// Custom deleter implementation
void WindowHandleDeleter::operator()(WindowHandle* window) const {
    if (!window) return;

    if (window->hwnd) {
        ::DestroyWindow(window->hwnd);
    }

    delete window;
}

WindowPtr CreateWindow(const char* title, u32 width, u32 height) {
    // Register window class (only once)
    if (!windowClassRegistered) {
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = WINDOW_CLASS_NAME;

        if (!RegisterClassExA(&wc)) {
            return nullptr;
        }
        windowClassRegistered = true;
    }

    // Calculate window size to ensure client area matches requested size
    RECT rect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    LONG windowWidth = rect.right - rect.left;
    LONG windowHeight = rect.bottom - rect.top;

    // Create window
    HWND hwnd = CreateWindowExA(
        0,                      // Extended style
        WINDOW_CLASS_NAME,      // Class name
        title,                  // Window title
        WS_OVERLAPPEDWINDOW,    // Style
        CW_USEDEFAULT,          // X position
        CW_USEDEFAULT,          // Y position
        windowWidth,            // Width
        windowHeight,           // Height
        nullptr,                // Parent window
        nullptr,                // Menu
        GetModuleHandle(nullptr), // Instance
        nullptr                 // Additional data
    );

    if (!hwnd) {
        return nullptr;
    }

    // Allocate window handle structure
    WindowHandle* window = new WindowHandle();
    window->hwnd = hwnd;
    window->shouldClose = false;

    // Store window pointer in window user data for callback access
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));

    // Show the window
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    return WindowPtr(window);
}

bool PollEvents(WindowHandle* window) {
    if (!window) return false;

    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            window->shouldClose = true;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return !window->shouldClose;
}

// ============================================================================
// Timing Implementation
// ============================================================================

u64 GetPerformanceCounter() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return static_cast<u64>(counter.QuadPart);
}

u64 GetPerformanceFrequency() {
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return static_cast<u64>(frequency.QuadPart);
}

// ============================================================================
// Virtual Memory Implementation
// ============================================================================

void* VirtualAlloc(size_t size) {
    if (size == 0) return nullptr;

    void* ptr = ::VirtualAlloc(
        nullptr,              // Let system choose address
        size,                 // Size in bytes
        MEM_COMMIT | MEM_RESERVE, // Commit and reserve
        PAGE_READWRITE        // Read/write access
    );

    return ptr;
}

void VirtualFree(void* ptr, size_t size) {
    if (!ptr) return;
    (void)size; // Size parameter not used on Windows (entire region is freed)

    ::VirtualFree(ptr, 0, MEM_RELEASE);
}

// ============================================================================
// File I/O Implementation
// ============================================================================

struct FileHandle {
    HANDLE handle;
};

// Custom deleter implementation
void FileHandleDeleter::operator()(FileHandle* file) const {
    if (!file) return;

    if (file->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(file->handle);
    }

    delete file;
}

FilePtr OpenFile(const char* path, bool write) {
    if (!path) return nullptr;

    DWORD access = write ? GENERIC_WRITE : GENERIC_READ;
    DWORD creation = write ? CREATE_ALWAYS : OPEN_EXISTING;

    HANDLE handle = CreateFileA(
        path,
        access,
        0,                  // No sharing
        nullptr,            // Default security
        creation,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (handle == INVALID_HANDLE_VALUE) {
        return nullptr;
    }

    FileHandle* file = new FileHandle();
    file->handle = handle;
    return FilePtr(file);
}

size_t ReadFile(FileHandle* file, void* buffer, size_t bytes) {
    if (!file || !buffer || bytes == 0) return 0;

    DWORD bytesRead = 0;
    BOOL result = ::ReadFile(
        file->handle,
        buffer,
        static_cast<DWORD>(bytes),
        &bytesRead,
        nullptr
    );

    return result ? static_cast<size_t>(bytesRead) : 0;
}

// ============================================================================
// Threading Primitives Implementation
// ============================================================================

struct Mutex {
    CRITICAL_SECTION criticalSection;
};

struct Semaphore {
    HANDLE handle;
};

// Custom deleter implementation
void MutexDeleter::operator()(Mutex* mutex) const {
    if (!mutex) return;

    DeleteCriticalSection(&mutex->criticalSection);
    delete mutex;
}

MutexPtr CreateMutex() {
    Mutex* mutex = new Mutex();
    InitializeCriticalSection(&mutex->criticalSection);
    return MutexPtr(mutex);
}

void Lock(Mutex* mutex) {
    if (!mutex) return;
    EnterCriticalSection(&mutex->criticalSection);
}

void Unlock(Mutex* mutex) {
    if (!mutex) return;
    LeaveCriticalSection(&mutex->criticalSection);
}

// Custom deleter implementation
void SemaphoreDeleter::operator()(Semaphore* semaphore) const {
    if (!semaphore) return;
    if (semaphore->handle) {
        ::CloseHandle(semaphore->handle);
        semaphore->handle = nullptr;
    }
    delete semaphore;
}

SemaphorePtr CreateSemaphore(u32 initial_count) {
    Semaphore* semaphore = new Semaphore();
    semaphore->handle = ::CreateSemaphoreA(nullptr, static_cast<LONG>(initial_count), LONG_MAX, nullptr);
    if (!semaphore->handle) {
        delete semaphore;
        return nullptr;
    }
    return SemaphorePtr(semaphore);
}

void WaitSemaphore(Semaphore* semaphore, u32 timeout_ms) {
    if (!semaphore || !semaphore->handle) return;
    DWORD wait_time = timeout_ms == 0xFFFFFFFF ? INFINITE : timeout_ms;
    ::WaitForSingleObject(semaphore->handle, wait_time);
}

void SignalSemaphore(Semaphore* semaphore, u32 count) {
    if (!semaphore || !semaphore->handle) return;
    ::ReleaseSemaphore(semaphore->handle, static_cast<LONG>(count), nullptr);
}

} // namespace Platform

#endif // PLATFORM_WINDOWS
