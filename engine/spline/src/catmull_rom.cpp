#include <engine/spline/catmull_rom.hpp>
#include <cmath>
#include <algorithm>

namespace engine::spline {

const SplinePoint& CatmullRomSpline::get_point(size_t index) const {
    return m_points.at(index);
}

void CatmullRomSpline::set_point(size_t index, const SplinePoint& point) {
    m_points.at(index) = point;
    invalidate_cache();
}

void CatmullRomSpline::add_point(const SplinePoint& point) {
    m_points.push_back(point);
    invalidate_cache();
}

void CatmullRomSpline::insert_point(size_t index, const SplinePoint& point) {
    if (index > m_points.size()) index = m_points.size();
    m_points.insert(m_points.begin() + static_cast<ptrdiff_t>(index), point);
    invalidate_cache();
}

void CatmullRomSpline::remove_point(size_t index) {
    if (index < m_points.size()) {
        m_points.erase(m_points.begin() + static_cast<ptrdiff_t>(index));
        invalidate_cache();
    }
}

void CatmullRomSpline::clear_points() {
    m_points.clear();
    invalidate_cache();
}

void CatmullRomSpline::get_segment_points(int segment, Vec3& p0, Vec3& p1, Vec3& p2, Vec3& p3) const {
    if (m_points.size() < 2) {
        p0 = p1 = p2 = p3 = Vec3(0.0f);
        return;
    }

    size_t n = m_points.size();
    int num_segments = static_cast<int>(n) - 1;
    if (end_mode == SplineEndMode::Loop) {
        num_segments = static_cast<int>(n);
    }

    segment = std::clamp(segment, 0, num_segments - 1);

    size_t i1 = static_cast<size_t>(segment);
    size_t i2 = (end_mode == SplineEndMode::Loop) ? ((i1 + 1) % n) : std::min(i1 + 1, n - 1);

    // Get surrounding points for Catmull-Rom
    size_t i0, i3;
    if (end_mode == SplineEndMode::Loop) {
        i0 = (i1 + n - 1) % n;
        i3 = (i2 + 1) % n;
    } else {
        i0 = (i1 > 0) ? i1 - 1 : i1;
        i3 = (i2 < n - 1) ? i2 + 1 : i2;
    }

    p0 = m_points[i0].position;
    p1 = m_points[i1].position;
    p2 = m_points[i2].position;
    p3 = m_points[i3].position;
}

float CatmullRomSpline::get_knot_interval(const Vec3& p0, const Vec3& p1) const {
    float d = glm::length(p1 - p0);
    return std::pow(d, alpha);
}

Vec3 CatmullRomSpline::catmull_rom_interpolate(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float t) const {
    if (std::abs(alpha) < 0.0001f) {
        // Uniform Catmull-Rom (faster, may have cusps)
        float t2 = t * t;
        float t3 = t2 * t;

        return 0.5f * ((2.0f * p1) +
                       (-p0 + p2) * t +
                       (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                       (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
    }

    // Centripetal/Chordal Catmull-Rom (no cusps)
    float t0 = 0.0f;
    float t1 = t0 + get_knot_interval(p0, p1);
    float t2 = t1 + get_knot_interval(p1, p2);
    float t3 = t2 + get_knot_interval(p2, p3);

    // Remap t from [0,1] to [t1, t2]
    float tt = glm::mix(t1, t2, t);

    // Ensure no division by zero
    auto safe_div = [](const Vec3& a, float b) -> Vec3 {
        return (std::abs(b) > 0.0001f) ? a / b : a;
    };

    if (std::abs(t1 - t0) < 0.0001f) return p0;
    if (std::abs(t2 - t1) < 0.0001f) return p1;
    if (std::abs(t3 - t2) < 0.0001f) return p2;

    Vec3 a1 = safe_div((t1 - tt) * p0 + (tt - t0) * p1, t1 - t0);
    Vec3 a2 = safe_div((t2 - tt) * p1 + (tt - t1) * p2, t2 - t1);
    Vec3 a3 = safe_div((t3 - tt) * p2 + (tt - t2) * p3, t3 - t2);

    Vec3 b1 = safe_div((t2 - tt) * a1 + (tt - t0) * a2, t2 - t0);
    Vec3 b2 = safe_div((t3 - tt) * a2 + (tt - t1) * a3, t3 - t1);

    return safe_div((t2 - tt) * b1 + (tt - t1) * b2, t2 - t1);
}

Vec3 CatmullRomSpline::catmull_rom_derivative(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float t) const {
    // Numerical derivative using finite differences
    float h = 0.001f;
    Vec3 prev = catmull_rom_interpolate(p0, p1, p2, p3, std::max(0.0f, t - h));
    Vec3 next = catmull_rom_interpolate(p0, p1, p2, p3, std::min(1.0f, t + h));
    return (next - prev) / (2.0f * h);
}

Vec3 CatmullRomSpline::get_tangent_at_point(size_t index) const {
    if (m_points.size() < 2) return Vec3(0.0f, 0.0f, 1.0f);
    if (index >= m_points.size()) index = m_points.size() - 1;

    size_t n = m_points.size();
    Vec3 prev, next;

    if (end_mode == SplineEndMode::Loop) {
        prev = m_points[(index + n - 1) % n].position;
        next = m_points[(index + 1) % n].position;
    } else {
        if (index == 0) {
            prev = m_points[0].position;
            next = m_points[1].position;
        } else if (index == n - 1) {
            prev = m_points[n - 2].position;
            next = m_points[n - 1].position;
        } else {
            prev = m_points[index - 1].position;
            next = m_points[index + 1].position;
        }
    }

    Vec3 tangent = next - prev;
    float len = glm::length(tangent);
    return (len > 0.0001f) ? tangent / len : Vec3(0.0f, 0.0f, 1.0f);
}

SplineEvalResult CatmullRomSpline::evaluate(float t) const {
    SplineEvalResult result;

    if (m_points.empty()) return result;

    if (m_points.size() == 1) {
        result.position = m_points[0].position;
        result.roll = m_points[0].roll;
        result.custom_data = m_points[0].custom_data;
        return result;
    }

    t = normalize_t(t);

    int segment;
    float local_t;
    get_segment(t, segment, local_t);

    Vec3 p0, p1, p2, p3;
    get_segment_points(segment, p0, p1, p2, p3);

    result.position = catmull_rom_interpolate(p0, p1, p2, p3, local_t);

    Vec3 deriv = catmull_rom_derivative(p0, p1, p2, p3, local_t);
    float deriv_len = glm::length(deriv);
    result.tangent = (deriv_len > 0.0001f) ? deriv / deriv_len : Vec3(0.0f, 0.0f, 1.0f);

    // Calculate normal and binormal
    Vec3 up(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(result.tangent, up)) > 0.99f) {
        up = Vec3(1.0f, 0.0f, 0.0f);
    }
    result.binormal = glm::normalize(glm::cross(result.tangent, up));
    result.normal = glm::cross(result.binormal, result.tangent);

    // Interpolate roll and custom data
    size_t n = m_points.size();
    size_t i1 = static_cast<size_t>(segment);
    size_t i2 = (end_mode == SplineEndMode::Loop) ? ((i1 + 1) % n) : std::min(i1 + 1, n - 1);
    result.roll = glm::mix(m_points[i1].roll, m_points[i2].roll, local_t);
    result.custom_data = glm::mix(m_points[i1].custom_data, m_points[i2].custom_data, local_t);

    // Apply roll
    if (std::abs(result.roll) > 0.0001f) {
        Mat4 rot = glm::rotate(Mat4(1.0f), result.roll, result.tangent);
        result.normal = Vec3(rot * Vec4(result.normal, 0.0f));
        result.binormal = Vec3(rot * Vec4(result.binormal, 0.0f));
    }

    return result;
}

Vec3 CatmullRomSpline::evaluate_position(float t) const {
    if (m_points.empty()) return Vec3(0.0f);
    if (m_points.size() == 1) return m_points[0].position;

    t = normalize_t(t);

    int segment;
    float local_t;
    get_segment(t, segment, local_t);

    Vec3 p0, p1, p2, p3;
    get_segment_points(segment, p0, p1, p2, p3);

    return catmull_rom_interpolate(p0, p1, p2, p3, local_t);
}

Vec3 CatmullRomSpline::evaluate_tangent(float t) const {
    if (m_points.size() < 2) return Vec3(0.0f, 0.0f, 1.0f);

    t = normalize_t(t);

    int segment;
    float local_t;
    get_segment(t, segment, local_t);

    Vec3 p0, p1, p2, p3;
    get_segment_points(segment, p0, p1, p2, p3);

    Vec3 deriv = catmull_rom_derivative(p0, p1, p2, p3, local_t);
    float len = glm::length(deriv);
    if (len > 0.0001f) {
        return deriv / len;
    }

    // Fallback to segment direction to avoid zero-length tangents that break smoothness
    Vec3 fallback = p2 - p1;
    float fallback_len = glm::length(fallback);
    if (fallback_len > 0.0001f) {
        return fallback / fallback_len;
    }

    return Vec3(0.0f, 0.0f, 1.0f);
}

void CatmullRomSpline::update_cache() const {
    if (m_cache_valid) return;

    m_cached_segment_lengths.clear();
    m_cached_cumulative_lengths.clear();
    m_cached_length = 0.0f;

    if (m_points.size() < 2) {
        m_cache_valid = true;
        return;
    }

    int num_segments = static_cast<int>(m_points.size()) - 1;
    if (end_mode == SplineEndMode::Loop) {
        num_segments = static_cast<int>(m_points.size());
    }

    m_cached_segment_lengths.reserve(num_segments);
    m_cached_cumulative_lengths.reserve(num_segments + 1);
    m_cached_cumulative_lengths.push_back(0.0f);

    for (int seg = 0; seg < num_segments; ++seg) {
        Vec3 p0, p1, p2, p3;
        get_segment_points(seg, p0, p1, p2, p3);

        // Approximate segment length
        float seg_len = 0.0f;
        Vec3 prev = catmull_rom_interpolate(p0, p1, p2, p3, 0.0f);
        for (int i = 1; i <= 20; ++i) {
            float t = static_cast<float>(i) / 20.0f;
            Vec3 curr = catmull_rom_interpolate(p0, p1, p2, p3, t);
            seg_len += glm::length(curr - prev);
            prev = curr;
        }

        m_cached_segment_lengths.push_back(seg_len);
        m_cached_length += seg_len;
        m_cached_cumulative_lengths.push_back(m_cached_length);
    }

    m_cache_valid = true;
}

float CatmullRomSpline::get_length() const {
    update_cache();
    return m_cached_length;
}

float CatmullRomSpline::get_length_to(float t) const {
    update_cache();

    if (m_points.size() < 2 || t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return m_cached_length;

    t = normalize_t(t);

    int segment;
    float local_t;
    get_segment(t, segment, local_t);

    float length = (segment > 0) ? m_cached_cumulative_lengths[segment] : 0.0f;

    // Add partial segment length
    if (local_t > 0.0f && segment < static_cast<int>(m_cached_segment_lengths.size())) {
        Vec3 p0, p1, p2, p3;
        get_segment_points(segment, p0, p1, p2, p3);

        Vec3 prev = catmull_rom_interpolate(p0, p1, p2, p3, 0.0f);
        for (int i = 1; i <= 20; ++i) {
            float sub_t = (static_cast<float>(i) / 20.0f) * local_t;
            Vec3 curr = catmull_rom_interpolate(p0, p1, p2, p3, sub_t);
            length += glm::length(curr - prev);
            prev = curr;
        }
    }

    return length;
}

float CatmullRomSpline::get_t_at_distance(float distance) const {
    update_cache();

    if (m_points.size() < 2 || m_cached_length < 0.0001f) return 0.0f;

    distance = std::clamp(distance, 0.0f, m_cached_length);

    // Find segment containing this distance
    int segment = 0;
    for (size_t i = 1; i < m_cached_cumulative_lengths.size(); ++i) {
        if (m_cached_cumulative_lengths[i] >= distance) {
            segment = static_cast<int>(i) - 1;
            break;
        }
    }

    float segment_start = (segment > 0) ? m_cached_cumulative_lengths[segment] : 0.0f;
    float local_distance = distance - segment_start;
    float segment_length = m_cached_segment_lengths[segment];

    // Binary search for local_t
    Vec3 p0, p1, p2, p3;
    get_segment_points(segment, p0, p1, p2, p3);

    float t_low = 0.0f, t_high = 1.0f;
    for (int iter = 0; iter < 20; ++iter) {
        float t_mid = (t_low + t_high) * 0.5f;

        float len = 0.0f;
        Vec3 prev = catmull_rom_interpolate(p0, p1, p2, p3, 0.0f);
        for (int i = 1; i <= 20; ++i) {
            float sub_t = (static_cast<float>(i) / 20.0f) * t_mid;
            Vec3 curr = catmull_rom_interpolate(p0, p1, p2, p3, sub_t);
            len += glm::length(curr - prev);
            prev = curr;
        }

        if (len < local_distance) {
            t_low = t_mid;
        } else {
            t_high = t_mid;
        }
    }

    float local_t = (t_low + t_high) * 0.5f;
    int num_segments = static_cast<int>(m_cached_segment_lengths.size());
    return (static_cast<float>(segment) + local_t) / num_segments;
}

SplineEvalResult CatmullRomSpline::evaluate_at_distance(float distance) const {
    float t = get_t_at_distance(distance);
    return evaluate(t);
}

SplineNearestResult CatmullRomSpline::find_nearest_point(const Vec3& position) const {
    SplineNearestResult result;

    if (m_points.empty()) return result;
    if (m_points.size() == 1) {
        result.position = m_points[0].position;
        result.distance = glm::length(position - result.position);
        return result;
    }

    float best_dist_sq = std::numeric_limits<float>::max();

    int num_segments = static_cast<int>(m_points.size()) - 1;
    if (end_mode == SplineEndMode::Loop) {
        num_segments = static_cast<int>(m_points.size());
    }

    for (int seg = 0; seg < num_segments; ++seg) {
        Vec3 p0, p1, p2, p3;
        get_segment_points(seg, p0, p1, p2, p3);

        for (int i = 0; i <= 20; ++i) {
            float local_t = static_cast<float>(i) / 20.0f;
            Vec3 pt = catmull_rom_interpolate(p0, p1, p2, p3, local_t);
            float dist_sq = glm::dot(position - pt, position - pt);

            if (dist_sq < best_dist_sq) {
                best_dist_sq = dist_sq;
                result.segment_index = seg;
                result.t = (static_cast<float>(seg) + local_t) / num_segments;
                result.position = pt;
            }
        }
    }

    result.distance = std::sqrt(best_dist_sq);
    return result;
}

float CatmullRomSpline::find_nearest_t(const Vec3& position) const {
    return find_nearest_point(position).t;
}

AABB CatmullRomSpline::get_bounds() const {
    AABB bounds;
    if (m_points.empty()) return bounds;

    bounds.min = bounds.max = m_points[0].position;

    // Sample the spline for bounds (control points don't define tight bounds for Catmull-Rom)
    if (m_points.size() >= 2) {
        for (int i = 0; i <= 100; ++i) {
            float t = static_cast<float>(i) / 100.0f;
            Vec3 pos = evaluate_position(t);
            bounds.expand(pos);
        }
    } else {
        for (const auto& pt : m_points) {
            bounds.expand(pt.position);
        }
    }

    return bounds;
}

std::vector<Vec3> CatmullRomSpline::tessellate(int subdivisions_per_segment) const {
    std::vector<Vec3> result;

    if (m_points.size() < 2) {
        if (!m_points.empty()) {
            result.push_back(m_points[0].position);
        }
        return result;
    }

    int num_segments = static_cast<int>(m_points.size()) - 1;
    if (end_mode == SplineEndMode::Loop) {
        num_segments = static_cast<int>(m_points.size());
    }

    result.reserve(num_segments * subdivisions_per_segment + 1);

    for (int seg = 0; seg < num_segments; ++seg) {
        Vec3 p0, p1, p2, p3;
        get_segment_points(seg, p0, p1, p2, p3);

        for (int i = 0; i < subdivisions_per_segment; ++i) {
            float t = static_cast<float>(i) / subdivisions_per_segment;
            result.push_back(catmull_rom_interpolate(p0, p1, p2, p3, t));
        }
    }

    // Add final point
    if (end_mode != SplineEndMode::Loop) {
        result.push_back(m_points.back().position);
    } else {
        result.push_back(m_points[0].position);
    }

    return result;
}

CatmullRomSpline create_smooth_path(const std::vector<Vec3>& points, bool loop) {
    CatmullRomSpline spline;
    spline.end_mode = loop ? SplineEndMode::Loop : SplineEndMode::Clamp;

    std::vector<SplinePoint> spline_points;
    spline_points.reserve(points.size());

    for (const auto& pos : points) {
        SplinePoint pt;
        pt.position = pos;
        spline_points.push_back(pt);
    }
    spline.set_points(spline_points);

    return spline;
}

CatmullRomSpline create_camera_path(const std::vector<Vec3>& positions, const std::vector<float>& rolls) {
    CatmullRomSpline spline;

    std::vector<SplinePoint> points;
    points.reserve(positions.size());

    for (size_t i = 0; i < positions.size(); ++i) {
        SplinePoint pt;
        pt.position = positions[i];
        if (i < rolls.size()) {
            pt.roll = rolls[i];
        }
        points.push_back(pt);
    }
    spline.set_points(points);

    return spline;
}

} // namespace engine::spline
