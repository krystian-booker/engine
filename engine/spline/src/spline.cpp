#include <engine/spline/spline.hpp>
#include <engine/spline/bezier_spline.hpp>
#include <engine/spline/catmull_rom.hpp>
#include <cmath>
#include <algorithm>

namespace engine::spline {

void Spline::get_segment(float t, int& segment_index, float& local_t) const {
    if (m_points.size() < 2) {
        segment_index = 0;
        local_t = 0.0f;
        return;
    }

    int num_segments = static_cast<int>(m_points.size()) - 1;
    if (end_mode == SplineEndMode::Loop) {
        num_segments = static_cast<int>(m_points.size());
    }

    // Scale t to segment range
    float scaled_t = t * num_segments;
    segment_index = static_cast<int>(std::floor(scaled_t));

    // Clamp segment index
    if (segment_index < 0) segment_index = 0;
    if (segment_index >= num_segments) segment_index = num_segments - 1;

    local_t = scaled_t - static_cast<float>(segment_index);
    local_t = std::clamp(local_t, 0.0f, 1.0f);
}

float Spline::normalize_t(float t) const {
    switch (end_mode) {
        case SplineEndMode::Clamp:
            return std::clamp(t, 0.0f, 1.0f);

        case SplineEndMode::Loop:
            t = std::fmod(t, 1.0f);
            if (t < 0.0f) t += 1.0f;
            return t;

        case SplineEndMode::PingPong: {
            // Map to [0, 2] range then fold
            t = std::fmod(std::abs(t), 2.0f);
            if (t > 1.0f) t = 2.0f - t;
            return t;
        }
    }
    return std::clamp(t, 0.0f, 1.0f);
}

std::unique_ptr<Spline> create_spline(SplineMode mode) {
    switch (mode) {
        case SplineMode::Bezier:
            return std::make_unique<BezierSpline>();
        case SplineMode::CatmullRom:
            return std::make_unique<CatmullRomSpline>();
        case SplineMode::Linear:
        case SplineMode::BSpline:
            // Default to Catmull-Rom for unimplemented types
            return std::make_unique<CatmullRomSpline>();
    }
    return std::make_unique<CatmullRomSpline>();
}

namespace SplineUtils {

std::vector<SplinePoint> make_circle(float radius, int num_points) {
    std::vector<SplinePoint> points;
    points.reserve(num_points);

    for (int i = 0; i < num_points; ++i) {
        float angle = (static_cast<float>(i) / num_points) * 2.0f * 3.14159265f;
        float x = std::cos(angle) * radius;
        float z = std::sin(angle) * radius;

        SplinePoint point;
        point.position = Vec3(x, 0.0f, z);
        points.push_back(point);
    }

    return points;
}

std::vector<SplinePoint> make_helix(float radius, float height, float turns, int points_per_turn) {
    std::vector<SplinePoint> points;
    int total_points = static_cast<int>(turns * points_per_turn);
    points.reserve(total_points);

    for (int i = 0; i <= total_points; ++i) {
        float t = static_cast<float>(i) / total_points;
        float angle = t * turns * 2.0f * 3.14159265f;
        float x = std::cos(angle) * radius;
        float z = std::sin(angle) * radius;
        float y = t * height;

        SplinePoint point;
        point.position = Vec3(x, y, z);
        points.push_back(point);
    }

    return points;
}

std::vector<SplinePoint> make_figure8(float size, int num_points) {
    std::vector<SplinePoint> points;
    points.reserve(num_points);

    for (int i = 0; i < num_points; ++i) {
        float t = static_cast<float>(i) / num_points * 2.0f * 3.14159265f;
        // Lemniscate of Bernoulli
        float denom = 1.0f + std::sin(t) * std::sin(t);
        float x = (size * std::cos(t)) / denom;
        float z = (size * std::sin(t) * std::cos(t)) / denom;

        SplinePoint point;
        point.position = Vec3(x, 0.0f, z);
        points.push_back(point);
    }

    return points;
}

void compute_smooth_tangents(std::vector<SplinePoint>& points, float tension, bool loop) {
    if (points.size() < 2) return;

    size_t n = points.size();
    float scale = (1.0f - tension) * 0.5f;

    for (size_t i = 0; i < n; ++i) {
        Vec3 prev, next;

        if (loop) {
            prev = points[(i + n - 1) % n].position;
            next = points[(i + 1) % n].position;
        } else {
            if (i == 0) {
                prev = points[0].position;
                next = points[1].position;
            } else if (i == n - 1) {
                prev = points[n - 2].position;
                next = points[n - 1].position;
            } else {
                prev = points[i - 1].position;
                next = points[i + 1].position;
            }
        }

        Vec3 tangent = (next - prev) * scale;
        points[i].tangent_in = -tangent;
        points[i].tangent_out = tangent;
    }
}

std::vector<SplineEvalResult> sample_uniform(const Spline& spline, int num_samples) {
    std::vector<SplineEvalResult> results;
    if (num_samples < 2) return results;

    results.reserve(num_samples);
    float total_length = spline.get_length();

    for (int i = 0; i < num_samples; ++i) {
        float distance = (static_cast<float>(i) / (num_samples - 1)) * total_length;
        results.push_back(spline.evaluate_at_distance(distance));
    }

    return results;
}

float calculate_total_twist(const Spline& spline, int samples) {
    if (samples < 2) return 0.0f;

    float total_twist = 0.0f;
    Vec3 prev_normal{0.0f, 1.0f, 0.0f};

    for (int i = 0; i < samples; ++i) {
        float t = static_cast<float>(i) / (samples - 1);
        SplineEvalResult eval = spline.evaluate(t);

        if (i > 0) {
            float dot = glm::dot(prev_normal, eval.normal);
            dot = std::clamp(dot, -1.0f, 1.0f);
            total_twist += std::acos(dot);
        }

        prev_normal = eval.normal;
    }

    return total_twist;
}

} // namespace SplineUtils

} // namespace engine::spline
