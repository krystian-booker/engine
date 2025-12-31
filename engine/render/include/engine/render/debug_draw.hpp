#pragma once

#include <engine/core/math.hpp>
#include <engine/render/renderer.hpp>
#include <vector>
#include <string>
#include <cstdint>

namespace engine::render {

using namespace engine::core;

// Debug drawing for visualization of physics, AI, and other debug info
class DebugDraw {
public:
    // Color helpers
    static constexpr uint32_t RED     = 0xFF0000FF;
    static constexpr uint32_t GREEN   = 0x00FF00FF;
    static constexpr uint32_t BLUE    = 0x0000FFFF;
    static constexpr uint32_t YELLOW  = 0xFFFF00FF;
    static constexpr uint32_t CYAN    = 0x00FFFFFF;
    static constexpr uint32_t MAGENTA = 0xFF00FFFF;
    static constexpr uint32_t WHITE   = 0xFFFFFFFF;
    static constexpr uint32_t BLACK   = 0x000000FF;
    static constexpr uint32_t ORANGE  = 0xFFA500FF;

    // Line drawing
    static void line(const Vec3& a, const Vec3& b, uint32_t color = WHITE);
    static void line(const Vec3& a, const Vec3& b, uint32_t color_a, uint32_t color_b);

    // Basic shapes
    static void box(const Vec3& center, const Vec3& size, uint32_t color = WHITE);
    static void box(const Vec3& center, const Vec3& size, const Quat& rotation, uint32_t color = WHITE);
    static void wire_box(const Vec3& min, const Vec3& max, uint32_t color = WHITE);

    static void sphere(const Vec3& center, float radius, uint32_t color = WHITE, int segments = 16);
    static void circle(const Vec3& center, float radius, const Vec3& normal, uint32_t color = WHITE, int segments = 32);

    static void capsule(const Vec3& a, const Vec3& b, float radius, uint32_t color = WHITE, int segments = 8);
    static void cylinder(const Vec3& a, const Vec3& b, float radius, uint32_t color = WHITE, int segments = 16);
    static void cone(const Vec3& apex, const Vec3& base, float radius, uint32_t color = WHITE, int segments = 16);

    // Arrows and coordinate axes
    static void arrow(const Vec3& from, const Vec3& to, uint32_t color = WHITE, float head_size = 0.1f);
    static void axes(const Vec3& origin, float size = 1.0f);
    static void axes(const Mat4& transform, float size = 1.0f);

    // Grid
    static void grid(const Vec3& center, float size, int divisions, uint32_t color = 0x808080FF);
    static void ground_plane(float size = 100.0f, int divisions = 20);

    // Frustum
    static void frustum(const Mat4& view_proj_inverse, uint32_t color = WHITE);
    static void camera_frustum(const Vec3& pos, const Quat& rot, float fov, float aspect, float near, float far, uint32_t color = WHITE);

    // AABB
    static void aabb(const AABB& bounds, uint32_t color = WHITE);

    // Cross (3D crosshair)
    static void cross(const Vec3& center, float size, uint32_t color = WHITE);

    // Point (drawn as small cross)
    static void point(const Vec3& pos, float size = 0.1f, uint32_t color = WHITE);

    // 3D text (requires text rendering support)
    static void text_3d(const Vec3& pos, const std::string& text, uint32_t color = WHITE);

    // 2D overlay text
    static void text_2d(float x, float y, const std::string& text, uint32_t color = WHITE);

    // Persistence control
    static void set_duration(float seconds);  // Lines drawn after this will persist
    static void reset_duration();             // Back to single-frame

    // Depth test control
    static void set_depth_test(bool enabled);

    // Clear all debug draws
    static void clear();

    // Flush and render all debug draws
    static void flush(IRenderer* renderer);

private:
    struct DebugLine {
        Vec3 a, b;
        uint32_t color_a, color_b;
        float expire_time;
        bool depth_test;
    };

    struct DebugText3D {
        Vec3 pos;
        std::string text;
        uint32_t color;
        float expire_time;
    };

    struct DebugText2D {
        float x, y;
        std::string text;
        uint32_t color;
        float expire_time;
    };

    static std::vector<DebugLine> s_lines;
    static std::vector<DebugText3D> s_texts_3d;
    static std::vector<DebugText2D> s_texts_2d;
    static float s_current_duration;
    static bool s_depth_test;
    static float s_current_time;
};

} // namespace engine::render
