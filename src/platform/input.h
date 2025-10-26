#pragma once
#include "core/types.h"
#include "core/math.h"

// Forward declare
struct GLFWwindow;
class Window;

// Key codes (matching GLFW values for simplicity)
enum class KeyCode {
    Space = 32,
    Apostrophe = 39,  /* ' */
    Comma = 44,       /* , */
    Minus = 45,       /* - */
    Period = 46,      /* . */
    Slash = 47,       /* / */

    D0 = 48, D1, D2, D3, D4, D5, D6, D7, D8, D9,

    Semicolon = 59,   /* ; */
    Equal = 61,       /* = */

    A = 65, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    LeftBracket = 91,  /* [ */
    Backslash = 92,    /* \ */
    RightBracket = 93, /* ] */
    GraveAccent = 96,  /* ` */

    Escape = 256,
    Enter = 257,
    Tab = 258,
    Backspace = 259,
    Insert = 260,
    Delete = 261,
    Right = 262,
    Left = 263,
    Down = 264,
    Up = 265,
    PageUp = 266,
    PageDown = 267,
    Home = 268,
    End = 269,

    CapsLock = 280,
    ScrollLock = 281,
    NumLock = 282,
    PrintScreen = 283,
    Pause = 284,

    F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    LeftShift = 340,
    LeftControl = 341,
    LeftAlt = 342,
    LeftSuper = 343,
    RightShift = 344,
    RightControl = 345,
    RightAlt = 346,
    RightSuper = 347,
};

enum class MouseButton {
    Left = 0,
    Right = 1,
    Middle = 2,
    Button4 = 3,
    Button5 = 4,
    Button6 = 5,
    Button7 = 6,
    Button8 = 7,
};

class Input {
public:
    static void Init(Window* window);
    static void Update();  // Call at start of each frame

    // Keyboard
    static bool IsKeyPressed(KeyCode key);   // Just pressed this frame
    static bool IsKeyDown(KeyCode key);      // Held down
    static bool IsKeyReleased(KeyCode key);  // Just released this frame

    // Mouse buttons
    static bool IsMouseButtonPressed(MouseButton button);
    static bool IsMouseButtonDown(MouseButton button);
    static bool IsMouseButtonReleased(MouseButton button);

    // Mouse position
    static Vec2 GetMousePosition();
    static Vec2 GetMouseDelta();  // Movement since last frame

    // Mouse scroll
    static Vec2 GetMouseScroll();  // Per-frame delta

private:
    Input() = default;

    static Window* s_Window;

    // Key states (current and previous frame)
    static bool s_Keys[512];
    static bool s_KeysPrev[512];

    // Mouse button states
    static bool s_MouseButtons[8];
    static bool s_MouseButtonsPrev[8];

    // Mouse position
    static Vec2 s_MousePos;
    static Vec2 s_MousePosPrev;

    // Mouse scroll (reset each frame)
    static Vec2 s_MouseScroll;

    // GLFW callbacks
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    friend class Window;
};
