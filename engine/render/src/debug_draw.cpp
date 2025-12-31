#include <engine/render/debug_draw.hpp>
#include <engine/render/renderer.hpp>
#include <cmath>
#include <algorithm>

namespace engine::render {

// Static members
std::vector<DebugDraw::DebugLine> DebugDraw::s_lines;
std::vector<DebugDraw::DebugText3D> DebugDraw::s_texts_3d;
std::vector<DebugDraw::DebugText2D> DebugDraw::s_texts_2d;
float DebugDraw::s_current_duration = 0.0f;
bool DebugDraw::s_depth_test = true;
float DebugDraw::s_current_time = 0.0f;

// GPU resources (initialized lazily)
static bool s_gpu_initialized = false;
static VertexBufferHandle s_line_vb;
static ProgramHandle s_line_program;
static constexpr uint32_t MAX_DEBUG_LINES = 65536;

void DebugDraw::line(const Vec3& a, const Vec3& b, uint32_t color) {
    line(a, b, color, color);
}

void DebugDraw::line(const Vec3& a, const Vec3& b, uint32_t color_a, uint32_t color_b) {
    DebugLine ln;
    ln.a = a;
    ln.b = b;
    ln.color_a = color_a;
    ln.color_b = color_b;
    ln.expire_time = s_current_duration > 0 ? s_current_time + s_current_duration : 0;
    ln.depth_test = s_depth_test;
    s_lines.push_back(ln);
}

void DebugDraw::box(const Vec3& center, const Vec3& size, uint32_t color) {
    Vec3 half = size * 0.5f;
    Vec3 min = center - half;
    Vec3 max = center + half;
    wire_box(min, max, color);
}

void DebugDraw::box(const Vec3& center, const Vec3& size, const Quat& rotation, uint32_t color) {
    Vec3 half = size * 0.5f;

    // 8 corners of the box
    Vec3 corners[8] = {
        Vec3{-half.x, -half.y, -half.z},
        Vec3{ half.x, -half.y, -half.z},
        Vec3{ half.x,  half.y, -half.z},
        Vec3{-half.x,  half.y, -half.z},
        Vec3{-half.x, -half.y,  half.z},
        Vec3{ half.x, -half.y,  half.z},
        Vec3{ half.x,  half.y,  half.z},
        Vec3{-half.x,  half.y,  half.z}
    };

    // Transform corners
    for (auto& c : corners) {
        c = center + rotation * c;
    }

    // Draw 12 edges
    // Bottom face
    line(corners[0], corners[1], color);
    line(corners[1], corners[2], color);
    line(corners[2], corners[3], color);
    line(corners[3], corners[0], color);
    // Top face
    line(corners[4], corners[5], color);
    line(corners[5], corners[6], color);
    line(corners[6], corners[7], color);
    line(corners[7], corners[4], color);
    // Vertical edges
    line(corners[0], corners[4], color);
    line(corners[1], corners[5], color);
    line(corners[2], corners[6], color);
    line(corners[3], corners[7], color);
}

void DebugDraw::wire_box(const Vec3& min, const Vec3& max, uint32_t color) {
    Vec3 corners[8] = {
        {min.x, min.y, min.z},
        {max.x, min.y, min.z},
        {max.x, max.y, min.z},
        {min.x, max.y, min.z},
        {min.x, min.y, max.z},
        {max.x, min.y, max.z},
        {max.x, max.y, max.z},
        {min.x, max.y, max.z}
    };

    // Bottom face
    line(corners[0], corners[1], color);
    line(corners[1], corners[2], color);
    line(corners[2], corners[3], color);
    line(corners[3], corners[0], color);
    // Top face
    line(corners[4], corners[5], color);
    line(corners[5], corners[6], color);
    line(corners[6], corners[7], color);
    line(corners[7], corners[4], color);
    // Vertical edges
    line(corners[0], corners[4], color);
    line(corners[1], corners[5], color);
    line(corners[2], corners[6], color);
    line(corners[3], corners[7], color);
}

void DebugDraw::sphere(const Vec3& center, float radius, uint32_t color, int segments) {
    // Draw 3 circles for XY, XZ, YZ planes
    circle(center, radius, Vec3{0, 0, 1}, color, segments);  // XY plane
    circle(center, radius, Vec3{0, 1, 0}, color, segments);  // XZ plane
    circle(center, radius, Vec3{1, 0, 0}, color, segments);  // YZ plane
}

void DebugDraw::circle(const Vec3& center, float radius, const Vec3& normal, uint32_t color, int segments) {
    // Create orthonormal basis
    Vec3 n = glm::normalize(normal);
    Vec3 t = glm::abs(n.y) < 0.99f ? glm::cross(n, Vec3{0, 1, 0}) : glm::cross(n, Vec3{1, 0, 0});
    t = glm::normalize(t);
    Vec3 b = glm::cross(n, t);

    float step = glm::two_pi<float>() / static_cast<float>(segments);
    Vec3 prev = center + t * radius;

    for (int i = 1; i <= segments; ++i) {
        float angle = step * static_cast<float>(i);
        float c = std::cos(angle);
        float s = std::sin(angle);
        Vec3 curr = center + (t * c + b * s) * radius;
        line(prev, curr, color);
        prev = curr;
    }
}

void DebugDraw::capsule(const Vec3& a, const Vec3& b, float radius, uint32_t color, int segments) {
    Vec3 dir = b - a;
    float len = glm::length(dir);
    if (len < 0.0001f) {
        sphere(a, radius, color, segments);
        return;
    }

    Vec3 n = dir / len;
    Vec3 t = glm::abs(n.y) < 0.99f ? glm::cross(n, Vec3{0, 1, 0}) : glm::cross(n, Vec3{1, 0, 0});
    t = glm::normalize(t);
    Vec3 bt = glm::cross(n, t);

    // Draw cylinder body
    float step = glm::two_pi<float>() / static_cast<float>(segments);
    for (int i = 0; i < segments; ++i) {
        float angle = step * static_cast<float>(i);
        float c = std::cos(angle);
        float s = std::sin(angle);
        Vec3 offset = (t * c + bt * s) * radius;
        line(a + offset, b + offset, color);
    }

    // Draw caps
    circle(a, radius, n, color, segments);
    circle(b, radius, n, color, segments);

    // Draw half-spheres at caps
    for (int i = 0; i < segments / 2; ++i) {
        float phi = step * static_cast<float>(i);
        float r = radius * std::cos(phi);
        float h = radius * std::sin(phi);
        circle(a - n * h, r, n, color, segments);
        circle(b + n * h, r, n, color, segments);
    }
}

void DebugDraw::cylinder(const Vec3& a, const Vec3& b, float radius, uint32_t color, int segments) {
    Vec3 dir = b - a;
    float len = glm::length(dir);
    if (len < 0.0001f) return;

    Vec3 n = dir / len;
    Vec3 t = glm::abs(n.y) < 0.99f ? glm::cross(n, Vec3{0, 1, 0}) : glm::cross(n, Vec3{1, 0, 0});
    t = glm::normalize(t);
    Vec3 bt = glm::cross(n, t);

    // Draw vertical lines
    float step = glm::two_pi<float>() / static_cast<float>(segments);
    for (int i = 0; i < segments; ++i) {
        float angle = step * static_cast<float>(i);
        float c = std::cos(angle);
        float s = std::sin(angle);
        Vec3 offset = (t * c + bt * s) * radius;
        line(a + offset, b + offset, color);
    }

    // Draw end caps
    circle(a, radius, n, color, segments);
    circle(b, radius, n, color, segments);
}

void DebugDraw::cone(const Vec3& apex, const Vec3& base, float radius, uint32_t color, int segments) {
    Vec3 dir = base - apex;
    float len = glm::length(dir);
    if (len < 0.0001f) return;

    Vec3 n = dir / len;

    // Draw base circle
    circle(base, radius, n, color, segments);

    // Draw lines from apex to base
    Vec3 t = glm::abs(n.y) < 0.99f ? glm::cross(n, Vec3{0, 1, 0}) : glm::cross(n, Vec3{1, 0, 0});
    t = glm::normalize(t);
    Vec3 bt = glm::cross(n, t);

    float step = glm::two_pi<float>() / static_cast<float>(segments);
    for (int i = 0; i < segments; i += 2) {
        float angle = step * static_cast<float>(i);
        float c = std::cos(angle);
        float s = std::sin(angle);
        Vec3 edge = base + (t * c + bt * s) * radius;
        line(apex, edge, color);
    }
}

void DebugDraw::arrow(const Vec3& from, const Vec3& to, uint32_t color, float head_size) {
    line(from, to, color);

    Vec3 dir = to - from;
    float len = glm::length(dir);
    if (len < 0.0001f) return;

    dir /= len;
    float actual_head = std::min(head_size, len * 0.3f);

    Vec3 t = glm::abs(dir.y) < 0.99f ? glm::cross(dir, Vec3{0, 1, 0}) : glm::cross(dir, Vec3{1, 0, 0});
    t = glm::normalize(t) * actual_head * 0.5f;
    Vec3 bt = glm::cross(dir, t);

    Vec3 head_base = to - dir * actual_head;
    line(to, head_base + t, color);
    line(to, head_base - t, color);
    line(to, head_base + bt, color);
    line(to, head_base - bt, color);
}

void DebugDraw::axes(const Vec3& origin, float size) {
    line(origin, origin + Vec3{size, 0, 0}, RED);
    line(origin, origin + Vec3{0, size, 0}, GREEN);
    line(origin, origin + Vec3{0, 0, size}, BLUE);
}

void DebugDraw::axes(const Mat4& transform, float size) {
    Vec3 origin = Vec3{transform[3]};
    Vec3 x = Vec3{transform[0]} * size;
    Vec3 y = Vec3{transform[1]} * size;
    Vec3 z = Vec3{transform[2]} * size;

    arrow(origin, origin + x, RED);
    arrow(origin, origin + y, GREEN);
    arrow(origin, origin + z, BLUE);
}

void DebugDraw::grid(const Vec3& center, float size, int divisions, uint32_t color) {
    float half = size * 0.5f;
    float step = size / static_cast<float>(divisions);

    for (int i = 0; i <= divisions; ++i) {
        float t = static_cast<float>(i) * step - half;
        // Lines parallel to X
        line(center + Vec3{-half, 0, t}, center + Vec3{half, 0, t}, color);
        // Lines parallel to Z
        line(center + Vec3{t, 0, -half}, center + Vec3{t, 0, half}, color);
    }
}

void DebugDraw::ground_plane(float size, int divisions) {
    grid(Vec3{0}, size, divisions, 0x404040FF);
    // Highlight center axes
    line(Vec3{-size * 0.5f, 0, 0}, Vec3{size * 0.5f, 0, 0}, 0x800000FF);
    line(Vec3{0, 0, -size * 0.5f}, Vec3{0, 0, size * 0.5f}, 0x000080FF);
}

void DebugDraw::frustum(const Mat4& view_proj_inverse, uint32_t color) {
    // NDC corners
    Vec4 ndc_corners[8] = {
        {-1, -1, -1, 1}, { 1, -1, -1, 1}, { 1,  1, -1, 1}, {-1,  1, -1, 1},
        {-1, -1,  1, 1}, { 1, -1,  1, 1}, { 1,  1,  1, 1}, {-1,  1,  1, 1}
    };

    Vec3 corners[8];
    for (int i = 0; i < 8; ++i) {
        Vec4 world = view_proj_inverse * ndc_corners[i];
        corners[i] = Vec3{world} / world.w;
    }

    // Near plane
    line(corners[0], corners[1], color);
    line(corners[1], corners[2], color);
    line(corners[2], corners[3], color);
    line(corners[3], corners[0], color);

    // Far plane
    line(corners[4], corners[5], color);
    line(corners[5], corners[6], color);
    line(corners[6], corners[7], color);
    line(corners[7], corners[4], color);

    // Connecting edges
    line(corners[0], corners[4], color);
    line(corners[1], corners[5], color);
    line(corners[2], corners[6], color);
    line(corners[3], corners[7], color);
}

void DebugDraw::camera_frustum(const Vec3& pos, const Quat& rot, float fov, float aspect, float near_plane, float far_plane, uint32_t color) {
    Mat4 view = glm::inverse(glm::translate(Mat4{1.0f}, pos) * glm::mat4_cast(rot));
    Mat4 proj = glm::perspective(glm::radians(fov), aspect, near_plane, far_plane);
    Mat4 view_proj_inverse = glm::inverse(proj * view);
    frustum(view_proj_inverse, color);
}

void DebugDraw::aabb(const AABB& bounds, uint32_t color) {
    wire_box(bounds.min, bounds.max, color);
}

void DebugDraw::cross(const Vec3& center, float size, uint32_t color) {
    float half = size * 0.5f;
    line(center - Vec3{half, 0, 0}, center + Vec3{half, 0, 0}, color);
    line(center - Vec3{0, half, 0}, center + Vec3{0, half, 0}, color);
    line(center - Vec3{0, 0, half}, center + Vec3{0, 0, half}, color);
}

void DebugDraw::point(const Vec3& pos, float size, uint32_t color) {
    cross(pos, size, color);
}

void DebugDraw::text_3d(const Vec3& pos, const std::string& text, uint32_t color) {
    DebugText3D t;
    t.pos = pos;
    t.text = text;
    t.color = color;
    t.expire_time = s_current_duration > 0 ? s_current_time + s_current_duration : 0;
    s_texts_3d.push_back(t);
}

void DebugDraw::text_2d(float x, float y, const std::string& text, uint32_t color) {
    DebugText2D t;
    t.x = x;
    t.y = y;
    t.text = text;
    t.color = color;
    t.expire_time = s_current_duration > 0 ? s_current_time + s_current_duration : 0;
    s_texts_2d.push_back(t);
}

void DebugDraw::set_duration(float seconds) {
    s_current_duration = seconds;
}

void DebugDraw::reset_duration() {
    s_current_duration = 0.0f;
}

void DebugDraw::set_depth_test(bool enabled) {
    s_depth_test = enabled;
}

void DebugDraw::clear() {
    s_lines.clear();
    s_texts_3d.clear();
    s_texts_2d.clear();
}

void DebugDraw::update_time(float delta_time) {
    s_current_time += delta_time;
}

void DebugDraw::flush(IRenderer* renderer) {
    if (!renderer) return;

    // Remove expired items
    s_lines.erase(
        std::remove_if(s_lines.begin(), s_lines.end(),
            [](const DebugLine& l) {
                return l.expire_time > 0 && l.expire_time < s_current_time;
            }),
        s_lines.end());

    s_texts_3d.erase(
        std::remove_if(s_texts_3d.begin(), s_texts_3d.end(),
            [](const DebugText3D& t) {
                return t.expire_time > 0 && t.expire_time < s_current_time;
            }),
        s_texts_3d.end());

    s_texts_2d.erase(
        std::remove_if(s_texts_2d.begin(), s_texts_2d.end(),
            [](const DebugText2D& t) {
                return t.expire_time > 0 && t.expire_time < s_current_time;
            }),
        s_texts_2d.end());

    // Render debug lines
    if (!s_lines.empty()) {
        // Separate lines by depth test mode
        std::vector<const DebugLine*> depth_lines;
        std::vector<const DebugLine*> overlay_lines;

        for (const auto& line : s_lines) {
            if (line.depth_test) {
                depth_lines.push_back(&line);
            } else {
                overlay_lines.push_back(&line);
            }
        }

        // Submit depth-tested lines
        if (!depth_lines.empty()) {
            renderer->submit_debug_lines(RenderView::Debug, depth_lines.data(),
                                          static_cast<uint32_t>(depth_lines.size()), true);
        }

        // Submit overlay lines (no depth test)
        if (!overlay_lines.empty()) {
            renderer->submit_debug_lines(RenderView::DebugOverlay, overlay_lines.data(),
                                          static_cast<uint32_t>(overlay_lines.size()), false);
        }
    }

    // Clear single-frame items
    s_lines.erase(
        std::remove_if(s_lines.begin(), s_lines.end(),
            [](const DebugLine& l) { return l.expire_time == 0; }),
        s_lines.end());

    s_texts_3d.erase(
        std::remove_if(s_texts_3d.begin(), s_texts_3d.end(),
            [](const DebugText3D& t) { return t.expire_time == 0; }),
        s_texts_3d.end());

    s_texts_2d.erase(
        std::remove_if(s_texts_2d.begin(), s_texts_2d.end(),
            [](const DebugText2D& t) { return t.expire_time == 0; }),
        s_texts_2d.end());
}

void DebugDraw::shutdown(IRenderer* renderer) {
    clear();
    s_gpu_initialized = false;
}

} // namespace engine::render
