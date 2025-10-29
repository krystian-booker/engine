#include "window.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <cassert>

// Global state for GLFW initialization
static bool s_GLFWInitialized = false;
static u32 s_WindowCount = 0;

static void GLFWErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error (" << error << "): " << description << std::endl;
}

Window::Window(const WindowProperties& props)
    : m_Properties(props), m_Width(props.width), m_Height(props.height) {

    // Initialize GLFW once (refcounted by window count)
    if (!s_GLFWInitialized) {
        int success = glfwInit();
        (void)success;  // Suppress unused variable warning in Release builds
        assert(success && "Failed to initialize GLFW");
        glfwSetErrorCallback(GLFWErrorCallback);
        s_GLFWInitialized = true;
        std::cout << "GLFW initialized" << std::endl;
    }

    // Configure GLFW for Vulkan (no OpenGL context)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, props.resizable ? GLFW_TRUE : GLFW_FALSE);

    // Create window
    m_Window = glfwCreateWindow(
        props.width,
        props.height,
        props.title.c_str(),
        props.fullscreen ? glfwGetPrimaryMonitor() : nullptr,
        nullptr
    );

    assert(m_Window && "Failed to create GLFW window");
    s_WindowCount++;

    // Store 'this' pointer in GLFW window user data for callbacks
    glfwSetWindowUserPointer(m_Window, this);

    // Register GLFW callbacks
    glfwSetFramebufferSizeCallback(m_Window, FramebufferSizeCallback);
    glfwSetWindowCloseCallback(m_Window, WindowCloseCallback);
    glfwSetWindowFocusCallback(m_Window, WindowFocusCallback);

    std::cout << "Window created: " << props.width << "x" << props.height << std::endl;
}

Window::~Window() {
    glfwDestroyWindow(m_Window);
    s_WindowCount--;

    // Shutdown GLFW when last window closes
    if (s_WindowCount == 0) {
        glfwTerminate();
        s_GLFWInitialized = false;
        std::cout << "GLFW terminated" << std::endl;
    }
}

void Window::PollEvents() {
    glfwPollEvents();
}

void Window::SwapBuffers() {
    // For OpenGL: glfwSwapBuffers(m_Window);
    // For Vulkan: handled by swapchain present
    // Keeping this for API compatibility
}

bool Window::ShouldClose() const {
    return glfwWindowShouldClose(m_Window);
}

void Window::SetTitle(const std::string& title) {
    m_Properties.title = title;
    glfwSetWindowTitle(m_Window, title.c_str());
}

void Window::SetVSync(bool enabled) {
    m_Properties.vsync = enabled;
    // For OpenGL: glfwSwapInterval(enabled ? 1 : 0);
    // For Vulkan: configured in swapchain creation
}

// GLFW Callback Implementations
void Window::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    win->m_Width = width;
    win->m_Height = height;

    std::cout << "Window resized: " << width << "x" << height << std::endl;

    if (win->m_EventCallback) {
        win->m_EventCallback(WindowEvent::Resize, width, height);
    }
}

void Window::WindowCloseCallback(GLFWwindow* window) {
    Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));

    std::cout << "Window close requested" << std::endl;

    if (win->m_EventCallback) {
        win->m_EventCallback(WindowEvent::Close, 0, 0);
    }
}

void Window::WindowFocusCallback(GLFWwindow* window, int focused) {
    Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));

    WindowEvent event = focused ? WindowEvent::Focus : WindowEvent::LostFocus;
    std::cout << "Window " << (focused ? "gained" : "lost") << " focus" << std::endl;

    if (win->m_EventCallback) {
        win->m_EventCallback(event, 0, 0);
    }
}

void Window::SetCursorMode(bool locked) {
    if (locked) {
        glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else {
        glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void Window::SetCursorVisible(bool visible) {
    if (visible) {
        glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    } else {
        glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    }
}
