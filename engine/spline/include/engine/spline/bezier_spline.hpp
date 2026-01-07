#pragma once

#include <engine/spline/spline.hpp>

namespace engine::spline {

// Cubic Bezier spline implementation
// Each segment is defined by 4 control points: P0, P0_out, P1_in, P1
// P0 and P1 are the endpoints (from SplinePoint::position)
// P0_out is P0 + tangent_out, P1_in is P1 + tangent_in
class BezierSpline : public Spline {
public:
    BezierSpline() = default;
    ~BezierSpline() override = default;

    // Spline interface
    SplineMode mode() const override { return SplineMode::Bezier; }

    size_t point_count() const override { return m_points.size(); }
    const SplinePoint& get_point(size_t index) const override;
    void set_point(size_t index, const SplinePoint& point) override;
    void add_point(const SplinePoint& point) override;
    void insert_point(size_t index, const SplinePoint& point) override;
    void remove_point(size_t index) override;
    void clear_points() override;

    SplineEvalResult evaluate(float t) const override;
    Vec3 evaluate_position(float t) const override;
    Vec3 evaluate_tangent(float t) const override;

    float get_length() const override;
    float get_length_to(float t) const override;

    float get_t_at_distance(float distance) const override;
    SplineEvalResult evaluate_at_distance(float distance) const override;

    SplineNearestResult find_nearest_point(const Vec3& position) const override;
    float find_nearest_t(const Vec3& position) const override;

    AABB get_bounds() const override;
    std::vector<Vec3> tessellate(int subdivisions_per_segment = 10) const override;

    // Bezier-specific methods

    // Get the 4 control points for a segment
    void get_segment_control_points(int segment, Vec3& p0, Vec3& p1, Vec3& p2, Vec3& p3) const;

    // Split the spline at parameter t
    void split_at(float t);

    // Make tangents smooth (C1 continuity) at a point
    void make_smooth(size_t index);

    // Make tangents aligned but different lengths (G1 continuity) at a point
    void make_aligned(size_t index);

    // Break tangent continuity at a point
    void make_broken(size_t index);

    // Mirror tangents (make symmetric)
    void mirror_tangent(size_t index, bool mirror_out_to_in = true);

    // Auto-generate tangents for all points
    void auto_generate_tangents();

protected:
    void update_cache() const override;

private:
    // Evaluate cubic bezier curve
    static Vec3 cubic_bezier(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float t);
    static Vec3 cubic_bezier_derivative(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float t);

    // Approximate arc length of a bezier segment using subdivision
    static float approximate_segment_length(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, int subdivisions = 20);

    // Find parameter t for a given arc length using binary search
    float find_t_for_distance_in_segment(int segment, float target_distance, float segment_start_distance) const;
};

// Helper: Create a bezier spline from a simple path (auto-generates tangents)
BezierSpline create_bezier_from_path(const std::vector<Vec3>& points, float smoothness = 0.3f);

// Helper: Create a bezier circle
BezierSpline create_bezier_circle(const Vec3& center, float radius, const Vec3& normal = Vec3(0, 1, 0));

// Helper: Create a bezier arc
BezierSpline create_bezier_arc(const Vec3& center, float radius, float start_angle, float end_angle,
                                const Vec3& normal = Vec3(0, 1, 0));

} // namespace engine::spline
