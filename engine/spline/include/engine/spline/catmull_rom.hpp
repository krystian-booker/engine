#pragma once

#include <engine/spline/spline.hpp>

namespace engine::spline {

// Catmull-Rom spline implementation
// Automatically creates a smooth curve that passes through all control points
// No manual tangent editing required (tangents are derived from neighboring points)
class CatmullRomSpline : public Spline {
public:
    CatmullRomSpline() = default;
    ~CatmullRomSpline() override = default;

    // Spline interface
    SplineMode mode() const override { return SplineMode::CatmullRom; }

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

    // Catmull-Rom specific

    // Alpha parameter for centripetal/chordal variants:
    // 0.0 = uniform (can create loops/cusps)
    // 0.5 = centripetal (recommended, no cusps)
    // 1.0 = chordal (tighter curves)
    float alpha = 0.5f;

    // Get the automatically computed tangent at a point
    Vec3 get_tangent_at_point(size_t index) const;

protected:
    void update_cache() const override;

private:
    // Get the 4 control points for evaluating segment i (handles boundary conditions)
    void get_segment_points(int segment, Vec3& p0, Vec3& p1, Vec3& p2, Vec3& p3) const;

    // Catmull-Rom interpolation with parameterized alpha
    Vec3 catmull_rom_interpolate(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float t) const;
    Vec3 catmull_rom_derivative(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float t) const;

    // Calculate knot values for centripetal parameterization
    float get_knot_interval(const Vec3& p0, const Vec3& p1) const;
};

// Helper: Create a smooth path through points
CatmullRomSpline create_smooth_path(const std::vector<Vec3>& points, bool loop = false);

// Helper: Create a camera path through a series of look-at targets
CatmullRomSpline create_camera_path(const std::vector<Vec3>& positions,
                                     const std::vector<float>& rolls = {});

} // namespace engine::spline
