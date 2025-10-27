#pragma once
#include "core/types.h"
#include <string>
#include <functional>

// Forward declare GLFW types to avoid including GLFW in header
struct GLFWwindow;

// Window events
enum class WindowEvent {
    None,
    Close,
    Resize,
    Focus,
    LostFocus,
    Moved
};

struct WindowProperties {
    std::string title = "Game Engine";
    u32 width = 1280;
    u32 height = 720;
    bool vsync = true;
    bool resizable = true;
    bool fullscreen = false;
};

class Window {
public:
    Window(const WindowProperties& props);
    ~Window();

    // Prevent copying
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Window lifecycle
    void PollEvents();
    void SwapBuffers();  // For future OpenGL compat; Vulkan uses swapchain
    bool ShouldClose() const;

    // Properties
    u32 GetWidth() const { return m_Width; }
    u32 GetHeight() const { return m_Height; }
    f32 GetAspectRatio() const { return (f32)m_Width / (f32)m_Height; }

    void SetTitle(const std::string& title);
    void SetVSync(bool enabled);

    // Event callbacks
    using EventCallback = std::function<void(WindowEvent, u32, u32)>;
    void SetEventCallback(EventCallback callback) { m_EventCallback = callback; }

    // Get native window (for rendering backend - Vulkan/D3D12)
    GLFWwindow* GetNativeWindow() const { return m_Window; }

    // Cursor management
    void SetCursorMode(bool locked);
    void SetCursorVisible(bool visible);

private:
    GLFWwindow* m_Window;
    WindowProperties m_Properties;
    u32 m_Width, m_Height;
    EventCallback m_EventCallback;

    // GLFW callbacks (static, then forward to instance)
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void WindowCloseCallback(GLFWwindow* window);
    static void WindowFocusCallback(GLFWwindow* window, int focused);
};
