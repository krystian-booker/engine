#include <engine/spline/bezier_spline.hpp>
#include <cmath>
#include <algorithm>

namespace engine::spline {

const SplinePoint& BezierSpline::get_point(size_t index) const {
    return m_points.at(index);
}

void BezierSpline::set_point(size_t index, const SplinePoint& point) {
    m_points.at(index) = point;
    invalidate_cache();
}

void BezierSpline::add_point(const SplinePoint& point) {
    m_points.push_back(point);
    if (auto_tangents && m_points.size() > 1) {
        auto_generate_tangents();
    }
    invalidate_cache();
}

void BezierSpline::insert_point(size_t index, const SplinePoint& point) {
    if (index > m_points.size()) index = m_points.size();
    m_points.insert(m_points.begin() + static_cast<ptrdiff_t>(index), point);
    if (auto_tangents) {
        auto_generate_tangents();
    }
    invalidate_cache();
}

void BezierSpline::remove_point(size_t index) {
    if (index < m_points.size()) {
        m_points.erase(m_points.begin() + static_cast<ptrdiff_t>(index));
        if (auto_tangents && !m_points.empty()) {
            auto_generate_tangents();
        }
        invalidate_cache();
    }
}

void BezierSpline::clear_points() {
    m_points.clear();
    invalidate_cache();
}

void BezierSpline::get_segment_control_points(int segment, Vec3& p0, Vec3& p1, Vec3& p2, Vec3& p3) const {
    if (m_points.size() < 2) {
        p0 = p1 = p2 = p3 = Vec3(0.0f);
        return;
    }

    size_t n = m_points.size();
    size_t i0 = static_cast<size_t>(segment);
    size_t i1 = (end_mode == SplineEndMode::Loop) ? ((i0 + 1) % n) : std::min(i0 + 1, n - 1);

    const SplinePoint& pt0 = m_points[i0];
    const SplinePoint& pt1 = m_points[i1];

    p0 = pt0.position;
    p1 = pt0.position + pt0.tangent_out;
    p2 = pt1.position + pt1.tangent_in;
    p3 = pt1.position;
}

Vec3 BezierSpline::cubic_bezier(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float t) {
    float u = 1.0f - t;
    float t2 = t * t;
    float u2 = u * u;
    float t3 = t2 * t;
    float u3 = u2 * u;

    return u3 * p0 + 3.0f * u2 * t * p1 + 3.0f * u * t2 * p2 + t3 * p3;
}

Vec3 BezierSpline::cubic_bezier_derivative(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float t) {
    float u = 1.0f - t;
    float t2 = t * t;
    float u2 = u * u;

    return 3.0f * u2 * (p1 - p0) + 6.0f * u * t * (p2 - p1) + 3.0f * t2 * (p3 - p2);
}

SplineEvalResult BezierSpline::evaluate(float t) const {
    SplineEvalResult result;

    if (m_points.empty()) {
        return result;
    }

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
    get_segment_control_points(segment, p0, p1, p2, p3);

    result.position = cubic_bezier(p0, p1, p2, p3, local_t);
    result.tangent = glm::normalize(cubic_bezier_derivative(p0, p1, p2, p3, local_t));

    // Handle zero-length tangent
    if (glm::length(result.tangent) < 0.0001f) {
        result.tangent = Vec3(0.0f, 0.0f, 1.0f);
    }

    // Calculate normal and binormal using Frenet-Serret frame
    // Use a reference up vector to avoid twisting
    Vec3 up(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(result.tangent, up)) > 0.99f) {
        up = Vec3(1.0f, 0.0f, 0.0f);
    }
    result.binormal = glm::normalize(glm::cross(result.tangent, up));
    result.normal = glm::cross(result.binormal, result.tangent);

    // Interpolate roll
    size_t n = m_points.size();
    size_t i0 = static_cast<size_t>(segment);
    size_t i1 = (end_mode == SplineEndMode::Loop) ? ((i0 + 1) % n) : std::min(i0 + 1, n - 1);
    result.roll = glm::mix(m_points[i0].roll, m_points[i1].roll, local_t);
    result.custom_data = glm::mix(m_points[i0].custom_data, m_points[i1].custom_data, local_t);

    // Apply roll to normal/binormal
    if (std::abs(result.roll) > 0.0001f) {
        Mat4 rot = glm::rotate(Mat4(1.0f), result.roll, result.tangent);
        result.normal = Vec3(rot * Vec4(result.normal, 0.0f));
        result.binormal = Vec3(rot * Vec4(result.binormal, 0.0f));
    }

    return result;
}

Vec3 BezierSpline::evaluate_position(float t) const {
    if (m_points.empty()) return Vec3(0.0f);
    if (m_points.size() == 1) return m_points[0].position;

    t = normalize_t(t);

    int segment;
    float local_t;
    get_segment(t, segment, local_t);

    Vec3 p0, p1, p2, p3;
    get_segment_control_points(segment, p0, p1, p2, p3);

    return cubic_bezier(p0, p1, p2, p3, local_t);
}

Vec3 BezierSpline::evaluate_tangent(float t) const {
    if (m_points.size() < 2) return Vec3(0.0f, 0.0f, 1.0f);

    t = normalize_t(t);

    int segment;
    float local_t;
    get_segment(t, segment, local_t);

    Vec3 p0, p1, p2, p3;
    get_segment_control_points(segment, p0, p1, p2, p3);

    Vec3 deriv = cubic_bezier_derivative(p0, p1, p2, p3, local_t);
    float len = glm::length(deriv);
    if (len < 0.0001f) return Vec3(0.0f, 0.0f, 1.0f);
    return deriv / len;
}

float BezierSpline::approximate_segment_length(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, int subdivisions) {
    float length = 0.0f;
    Vec3 prev = p0;

    for (int i = 1; i <= subdivisions; ++i) {
        float t = static_cast<float>(i) / subdivisions;
        Vec3 curr = cubic_bezier(p0, p1, p2, p3, t);
        length += glm::length(curr - prev);
        prev = curr;
    }

    return length;
}

void BezierSpline::update_cache() const {
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

    for (int i = 0; i < num_segments; ++i) {
        Vec3 p0, p1, p2, p3;
        const_cast<BezierSpline*>(this)->get_segment_control_points(i, p0, p1, p2, p3);
        float seg_len = approximate_segment_length(p0, p1, p2, p3);
        m_cached_segment_lengths.push_back(seg_len);
        m_cached_length += seg_len;
        m_cached_cumulative_lengths.push_back(m_cached_length);
    }

    m_cache_valid = true;
}

float BezierSpline::get_length() const {
    update_cache();
    return m_cached_length;
}

float BezierSpline::get_length_to(float t) const {
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
        const_cast<BezierSpline*>(this)->get_segment_control_points(segment, p0, p1, p2, p3);

        // Approximate length to local_t
        Vec3 prev = p0;
        for (int i = 1; i <= 20; ++i) {
            float sub_t = (static_cast<float>(i) / 20.0f) * local_t;
            Vec3 curr = cubic_bezier(p0, p1, p2, p3, sub_t);
            length += glm::length(curr - prev);
            prev = curr;
        }
    }

    return length;
}

float BezierSpline::find_t_for_distance_in_segment(int segment, float target_distance, float segment_start_distance) const {
    if (segment >= static_cast<int>(m_cached_segment_lengths.size())) return 1.0f;

    float segment_length = m_cached_segment_lengths[segment];
    if (segment_length < 0.0001f) return 0.0f;

    float local_distance = target_distance - segment_start_distance;
    if (local_distance <= 0.0f) return 0.0f;
    if (local_distance >= segment_length) return 1.0f;

    Vec3 p0, p1, p2, p3;
    const_cast<BezierSpline*>(this)->get_segment_control_points(segment, p0, p1, p2, p3);

    // Binary search for t
    float t_low = 0.0f, t_high = 1.0f;
    for (int iter = 0; iter < 20; ++iter) {
        float t_mid = (t_low + t_high) * 0.5f;

        // Calculate length to t_mid
        float len = 0.0f;
        Vec3 prev = p0;
        int steps = 20;
        for (int i = 1; i <= steps; ++i) {
            float sub_t = (static_cast<float>(i) / steps) * t_mid;
            Vec3 curr = cubic_bezier(p0, p1, p2, p3, sub_t);
            len += glm::length(curr - prev);
            prev = curr;
        }

        if (len < local_distance) {
            t_low = t_mid;
        } else {
            t_high = t_mid;
        }
    }

    return (t_low + t_high) * 0.5f;
}

float BezierSpline::get_t_at_distance(float distance) const {
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

    float local_t = find_t_for_distance_in_segment(segment, distance,
        (segment > 0) ? m_cached_cumulative_lengths[segment] : 0.0f);

    int num_segments = static_cast<int>(m_cached_segment_lengths.size());
    return (static_cast<float>(segment) + local_t) / num_segments;
}

SplineEvalResult BezierSpline::evaluate_at_distance(float distance) const {
    float t = get_t_at_distance(distance);
    return evaluate(t);
}

SplineNearestResult BezierSpline::find_nearest_point(const Vec3& position) const {
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
        const_cast<BezierSpline*>(this)->get_segment_control_points(seg, p0, p1, p2, p3);

        // Sample segment
        for (int i = 0; i <= 20; ++i) {
            float local_t = static_cast<float>(i) / 20.0f;
            Vec3 pt = cubic_bezier(p0, p1, p2, p3, local_t);
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

float BezierSpline::find_nearest_t(const Vec3& position) const {
    return find_nearest_point(position).t;
}

AABB BezierSpline::get_bounds() const {
    AABB bounds;
    if (m_points.empty()) return bounds;

    bounds.min = bounds.max = m_points[0].position;

    for (const auto& pt : m_points) {
        bounds.expand(pt.position);
        bounds.expand(pt.position + pt.tangent_in);
        bounds.expand(pt.position + pt.tangent_out);
    }

    return bounds;
}

std::vector<Vec3> BezierSpline::tessellate(int subdivisions_per_segment) const {
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
        const_cast<BezierSpline*>(this)->get_segment_control_points(seg, p0, p1, p2, p3);

        for (int i = 0; i < subdivisions_per_segment; ++i) {
            float t = static_cast<float>(i) / subdivisions_per_segment;
            result.push_back(cubic_bezier(p0, p1, p2, p3, t));
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

void BezierSpline::split_at(float t) {
    if (m_points.size() < 2) return;

    t = normalize_t(t);

    int segment;
    float local_t;
    get_segment(t, segment, local_t);

    Vec3 p0, p1, p2, p3;
    get_segment_control_points(segment, p0, p1, p2, p3);

    // De Casteljau's algorithm
    Vec3 q0 = glm::mix(p0, p1, local_t);
    Vec3 q1 = glm::mix(p1, p2, local_t);
    Vec3 q2 = glm::mix(p2, p3, local_t);

    Vec3 r0 = glm::mix(q0, q1, local_t);
    Vec3 r1 = glm::mix(q1, q2, local_t);

    Vec3 split_point = glm::mix(r0, r1, local_t);

    // Create new point
    SplinePoint new_pt;
    new_pt.position = split_point;
    new_pt.tangent_in = r0 - split_point;
    new_pt.tangent_out = r1 - split_point;

    // Update existing points
    size_t i0 = static_cast<size_t>(segment);
    size_t i1 = (end_mode == SplineEndMode::Loop) ? ((i0 + 1) % m_points.size()) : std::min(i0 + 1, m_points.size() - 1);

    m_points[i0].tangent_out = q0 - p0;
    m_points[i1].tangent_in = q2 - p3;

    // Insert new point
    m_points.insert(m_points.begin() + static_cast<ptrdiff_t>(i0 + 1), new_pt);
    invalidate_cache();
}

void BezierSpline::make_smooth(size_t index) {
    if (index >= m_points.size()) return;

    Vec3 avg_dir = glm::normalize(m_points[index].tangent_out - m_points[index].tangent_in);
    float len_out = glm::length(m_points[index].tangent_out);
    float len_in = glm::length(m_points[index].tangent_in);

    m_points[index].tangent_out = avg_dir * len_out;
    m_points[index].tangent_in = -avg_dir * len_in;

    invalidate_cache();
}

void BezierSpline::make_aligned(size_t index) {
    if (index >= m_points.size()) return;

    Vec3 dir = glm::normalize(m_points[index].tangent_out);
    float len_in = glm::length(m_points[index].tangent_in);
    m_points[index].tangent_in = -dir * len_in;

    invalidate_cache();
}

void BezierSpline::make_broken(size_t /*index*/) {
    // Tangents are already independent, nothing to do
    invalidate_cache();
}

void BezierSpline::mirror_tangent(size_t index, bool mirror_out_to_in) {
    if (index >= m_points.size()) return;

    if (mirror_out_to_in) {
        m_points[index].tangent_in = -m_points[index].tangent_out;
    } else {
        m_points[index].tangent_out = -m_points[index].tangent_in;
    }

    invalidate_cache();
}

void BezierSpline::auto_generate_tangents() {
    SplineUtils::compute_smooth_tangents(m_points, tension, end_mode == SplineEndMode::Loop);
    invalidate_cache();
}

BezierSpline create_bezier_from_path(const std::vector<Vec3>& points, float smoothness) {
    BezierSpline spline;

    std::vector<SplinePoint> spline_points;
    spline_points.reserve(points.size());

    for (const auto& pos : points) {
        SplinePoint pt;
        pt.position = pos;
        spline_points.push_back(pt);
    }
    spline.set_points(spline_points);

    spline.tension = 1.0f - smoothness;
    spline.auto_generate_tangents();

    return spline;
}

BezierSpline create_bezier_circle(const Vec3& center, float radius, const Vec3& normal) {
    BezierSpline spline;
    spline.end_mode = SplineEndMode::Loop;

    // Create rotation to align with normal
    Vec3 up(0.0f, 1.0f, 0.0f);
    Vec3 axis = glm::cross(up, normal);
    float angle = std::acos(std::clamp(glm::dot(up, normal), -1.0f, 1.0f));
    Quat rot = (glm::length(axis) > 0.0001f) ? glm::angleAxis(angle, glm::normalize(axis)) : Quat(1, 0, 0, 0);

    // Magic number for bezier circle approximation
    float k = 0.5522847498f * radius;

    std::vector<SplinePoint> points;
    points.reserve(4);

    // 4 points for a circle
    for (int i = 0; i < 4; ++i) {
        float a = static_cast<float>(i) * 3.14159265f * 0.5f;
        float x = std::cos(a) * radius;
        float z = std::sin(a) * radius;

        SplinePoint pt;
        pt.position = center + Vec3(rot * Vec4(x, 0.0f, z, 0.0f));

        // Tangents perpendicular to radius
        float tx = -std::sin(a) * k;
        float tz = std::cos(a) * k;
        pt.tangent_out = Vec3(rot * Vec4(tx, 0.0f, tz, 0.0f));
        pt.tangent_in = -pt.tangent_out;

        points.push_back(pt);
    }
    spline.set_points(points);

    return spline;
}

BezierSpline create_bezier_arc(const Vec3& center, float radius, float start_angle, float end_angle, const Vec3& normal) {
    BezierSpline spline;

    // Create rotation to align with normal
    Vec3 up(0.0f, 1.0f, 0.0f);
    Vec3 axis = glm::cross(up, normal);
    float rot_angle = std::acos(std::clamp(glm::dot(up, normal), -1.0f, 1.0f));
    Quat rot = (glm::length(axis) > 0.0001f) ? glm::angleAxis(rot_angle, glm::normalize(axis)) : Quat(1, 0, 0, 0);

    float arc_angle = end_angle - start_angle;
    int num_segments = std::max(1, static_cast<int>(std::ceil(std::abs(arc_angle) / (3.14159265f * 0.5f))));

    std::vector<SplinePoint> points;
    points.reserve(num_segments + 1);

    for (int i = 0; i <= num_segments; ++i) {
        float t = static_cast<float>(i) / num_segments;
        float a = start_angle + t * arc_angle;
        float x = std::cos(a) * radius;
        float z = std::sin(a) * radius;

        SplinePoint pt;
        pt.position = center + Vec3(rot * Vec4(x, 0.0f, z, 0.0f));

        // Tangent magnitude based on arc segment
        float k = radius * std::tan(std::abs(arc_angle) / (num_segments * 2)) * 4.0f / 3.0f;
        float tx = -std::sin(a) * k;
        float tz = std::cos(a) * k;
        pt.tangent_out = Vec3(rot * Vec4(tx, 0.0f, tz, 0.0f));
        pt.tangent_in = -pt.tangent_out;

        points.push_back(pt);
    }
    spline.set_points(points);

    return spline;
}

} // namespace engine::spline
