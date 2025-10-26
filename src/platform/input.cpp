#include "input.h"
#include "window.h"
#include <GLFW/glfw3.h>
#include <cstring>
#include <iostream>

// Static member initialization
Window* Input::s_Window = nullptr;
bool Input::s_Keys[512] = {false};
bool Input::s_KeysPrev[512] = {false};
bool Input::s_MouseButtons[8] = {false};
bool Input::s_MouseButtonsPrev[8] = {false};
Vec2 Input::s_MousePos = Vec2(0, 0);
Vec2 Input::s_MousePosPrev = Vec2(0, 0);
Vec2 Input::s_MouseScroll = Vec2(0, 0);

void Input::Init(Window* window) {
    s_Window = window;

    GLFWwindow* glfwWindow = window->GetNativeWindow();

    // Set GLFW input callbacks
    glfwSetKeyCallback(glfwWindow, KeyCallback);
    glfwSetMouseButtonCallback(glfwWindow, MouseButtonCallback);
    glfwSetCursorPosCallback(glfwWindow, CursorPosCallback);
    glfwSetScrollCallback(glfwWindow, ScrollCallback);

    // Get initial mouse position
    double x, y;
    glfwGetCursorPos(glfwWindow, &x, &y);
    s_MousePos = Vec2(static_cast<f32>(x), static_cast<f32>(y));
    s_MousePosPrev = s_MousePos;

    std::cout << "Input system initialized" << std::endl;
}

void Input::Update() {
    // Copy current state to previous
    std::memcpy(s_KeysPrev, s_Keys, sizeof(s_Keys));
    std::memcpy(s_MouseButtonsPrev, s_MouseButtons, sizeof(s_MouseButtons));
    s_MousePosPrev = s_MousePos;

    // Reset per-frame deltas
    s_MouseScroll = Vec2(0, 0);
}

bool Input::IsKeyPressed(KeyCode key) {
    int keyIndex = static_cast<int>(key);
    if (keyIndex < 0 || keyIndex >= 512) return false;
    return s_Keys[keyIndex] && !s_KeysPrev[keyIndex];
}

bool Input::IsKeyDown(KeyCode key) {
    int keyIndex = static_cast<int>(key);
    if (keyIndex < 0 || keyIndex >= 512) return false;
    return s_Keys[keyIndex];
}

bool Input::IsKeyReleased(KeyCode key) {
    int keyIndex = static_cast<int>(key);
    if (keyIndex < 0 || keyIndex >= 512) return false;
    return !s_Keys[keyIndex] && s_KeysPrev[keyIndex];
}

bool Input::IsMouseButtonPressed(MouseButton button) {
    int btnIndex = static_cast<int>(button);
    if (btnIndex < 0 || btnIndex >= 8) return false;
    return s_MouseButtons[btnIndex] && !s_MouseButtonsPrev[btnIndex];
}

bool Input::IsMouseButtonDown(MouseButton button) {
    int btnIndex = static_cast<int>(button);
    if (btnIndex < 0 || btnIndex >= 8) return false;
    return s_MouseButtons[btnIndex];
}

bool Input::IsMouseButtonReleased(MouseButton button) {
    int btnIndex = static_cast<int>(button);
    if (btnIndex < 0 || btnIndex >= 8) return false;
    return !s_MouseButtons[btnIndex] && s_MouseButtonsPrev[btnIndex];
}

Vec2 Input::GetMousePosition() {
    return s_MousePos;
}

Vec2 Input::GetMouseDelta() {
    return s_MousePos - s_MousePosPrev;
}

Vec2 Input::GetMouseScroll() {
    return s_MouseScroll;
}

// GLFW Callback Implementations
void Input::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)window;
    (void)scancode;
    (void)mods;

    if (key >= 0 && key < 512) {
        if (action == GLFW_PRESS) {
            s_Keys[key] = true;
        } else if (action == GLFW_RELEASE) {
            s_Keys[key] = false;
        }
        // GLFW_REPEAT is ignored (handled by IsKeyDown)
    }
}

void Input::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    (void)window;
    (void)mods;

    if (button >= 0 && button < 8) {
        if (action == GLFW_PRESS) {
            s_MouseButtons[button] = true;
        } else if (action == GLFW_RELEASE) {
            s_MouseButtons[button] = false;
        }
    }
}

void Input::CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    s_MousePos = Vec2(static_cast<f32>(xpos), static_cast<f32>(ypos));
}

void Input::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)window;
    s_MouseScroll = Vec2(static_cast<f32>(xoffset), static_cast<f32>(yoffset));
}
