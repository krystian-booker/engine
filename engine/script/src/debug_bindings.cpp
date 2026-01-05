#include <engine/script/bindings.hpp>
#include <engine/render/debug_draw.hpp>

namespace engine::script {

void register_debug_bindings(sol::state& lua) {
    using namespace engine::render;
    using namespace engine::core;

    // Create Debug table
    auto debug = lua.create_named_table("Debug");

    // --- Line Drawing ---

    // Draw a line between two points
    debug.set_function("line", sol::overload(
        [](const Vec3& a, const Vec3& b) {
            DebugDraw::line(a, b);
        },
        [](const Vec3& a, const Vec3& b, uint32_t color) {
            DebugDraw::line(a, b, color);
        }
    ));

    // --- Basic Shapes ---

    // Draw a box
    debug.set_function("box", sol::overload(
        [](const Vec3& center, const Vec3& size) {
            DebugDraw::box(center, size);
        },
        [](const Vec3& center, const Vec3& size, uint32_t color) {
            DebugDraw::box(center, size, color);
        },
        [](const Vec3& center, const Vec3& size, const Quat& rotation, uint32_t color) {
            DebugDraw::box(center, size, rotation, color);
        }
    ));

    // Draw a wireframe box
    debug.set_function("wire_box", sol::overload(
        [](const Vec3& min, const Vec3& max) {
            DebugDraw::wire_box(min, max);
        },
        [](const Vec3& min, const Vec3& max, uint32_t color) {
            DebugDraw::wire_box(min, max, color);
        }
    ));

    // Draw a sphere
    debug.set_function("sphere", sol::overload(
        [](const Vec3& center, float radius) {
            DebugDraw::sphere(center, radius);
        },
        [](const Vec3& center, float radius, uint32_t color) {
            DebugDraw::sphere(center, radius, color);
        },
        [](const Vec3& center, float radius, uint32_t color, int segments) {
            DebugDraw::sphere(center, radius, color, segments);
        }
    ));

    // Draw a circle
    debug.set_function("circle", sol::overload(
        [](const Vec3& center, float radius, const Vec3& normal) {
            DebugDraw::circle(center, radius, normal);
        },
        [](const Vec3& center, float radius, const Vec3& normal, uint32_t color) {
            DebugDraw::circle(center, radius, normal, color);
        }
    ));

    // Draw a capsule
    debug.set_function("capsule", sol::overload(
        [](const Vec3& a, const Vec3& b, float radius) {
            DebugDraw::capsule(a, b, radius);
        },
        [](const Vec3& a, const Vec3& b, float radius, uint32_t color) {
            DebugDraw::capsule(a, b, radius, color);
        }
    ));

    // Draw a cylinder
    debug.set_function("cylinder", sol::overload(
        [](const Vec3& a, const Vec3& b, float radius) {
            DebugDraw::cylinder(a, b, radius);
        },
        [](const Vec3& a, const Vec3& b, float radius, uint32_t color) {
            DebugDraw::cylinder(a, b, radius, color);
        }
    ));

    // Draw a cone
    debug.set_function("cone", sol::overload(
        [](const Vec3& apex, const Vec3& base, float radius) {
            DebugDraw::cone(apex, base, radius);
        },
        [](const Vec3& apex, const Vec3& base, float radius, uint32_t color) {
            DebugDraw::cone(apex, base, radius, color);
        }
    ));

    // --- Arrows and Axes ---

    // Draw an arrow
    debug.set_function("arrow", sol::overload(
        [](const Vec3& from, const Vec3& to) {
            DebugDraw::arrow(from, to);
        },
        [](const Vec3& from, const Vec3& to, uint32_t color) {
            DebugDraw::arrow(from, to, color);
        },
        [](const Vec3& from, const Vec3& to, uint32_t color, float head_size) {
            DebugDraw::arrow(from, to, color, head_size);
        }
    ));

    // Draw coordinate axes at position
    debug.set_function("axes", sol::overload(
        [](const Vec3& origin) {
            DebugDraw::axes(origin);
        },
        [](const Vec3& origin, float size) {
            DebugDraw::axes(origin, size);
        }
    ));

    // --- Grid and Ground ---

    // Draw a grid
    debug.set_function("grid", sol::overload(
        [](const Vec3& center, float size, int divisions) {
            DebugDraw::grid(center, size, divisions);
        },
        [](const Vec3& center, float size, int divisions, uint32_t color) {
            DebugDraw::grid(center, size, divisions, color);
        }
    ));

    // Draw ground plane
    debug.set_function("ground_plane", sol::overload(
        []() {
            DebugDraw::ground_plane();
        },
        [](float size) {
            DebugDraw::ground_plane(size);
        },
        [](float size, int divisions) {
            DebugDraw::ground_plane(size, divisions);
        }
    ));

    // --- Points and Crosses ---

    // Draw a point (small cross)
    debug.set_function("point", sol::overload(
        [](const Vec3& pos) {
            DebugDraw::point(pos);
        },
        [](const Vec3& pos, float size) {
            DebugDraw::point(pos, size);
        },
        [](const Vec3& pos, float size, uint32_t color) {
            DebugDraw::point(pos, size, color);
        }
    ));

    // Draw a 3D crosshair
    debug.set_function("cross", sol::overload(
        [](const Vec3& center, float size) {
            DebugDraw::cross(center, size);
        },
        [](const Vec3& center, float size, uint32_t color) {
            DebugDraw::cross(center, size, color);
        }
    ));

    // --- Text ---

    // Draw 3D text at world position
    debug.set_function("text_3d", sol::overload(
        [](const Vec3& pos, const std::string& text) {
            DebugDraw::text_3d(pos, text);
        },
        [](const Vec3& pos, const std::string& text, uint32_t color) {
            DebugDraw::text_3d(pos, text, color);
        }
    ));

    // Draw 2D overlay text
    debug.set_function("text_2d", sol::overload(
        [](float x, float y, const std::string& text) {
            DebugDraw::text_2d(x, y, text);
        },
        [](float x, float y, const std::string& text, uint32_t color) {
            DebugDraw::text_2d(x, y, text, color);
        }
    ));

    // --- Persistence and Settings ---

    // Set duration for subsequent draws (0 = single frame)
    debug.set_function("set_duration", &DebugDraw::set_duration);
    debug.set_function("reset_duration", &DebugDraw::reset_duration);

    // Enable/disable depth testing
    debug.set_function("set_depth_test", &DebugDraw::set_depth_test);

    // --- Color Constants ---
    debug["RED"] = DebugDraw::RED;
    debug["GREEN"] = DebugDraw::GREEN;
    debug["BLUE"] = DebugDraw::BLUE;
    debug["YELLOW"] = DebugDraw::YELLOW;
    debug["CYAN"] = DebugDraw::CYAN;
    debug["MAGENTA"] = DebugDraw::MAGENTA;
    debug["WHITE"] = DebugDraw::WHITE;
    debug["BLACK"] = DebugDraw::BLACK;
    debug["ORANGE"] = DebugDraw::ORANGE;

    // Helper to create RGBA color
    debug.set_function("color", [](int r, int g, int b, sol::optional<int> a) -> uint32_t {
        uint8_t alpha = static_cast<uint8_t>(a.value_or(255));
        return (static_cast<uint32_t>(r) << 24) |
               (static_cast<uint32_t>(g) << 16) |
               (static_cast<uint32_t>(b) << 8) |
               static_cast<uint32_t>(alpha);
    });
}

} // namespace engine::script
