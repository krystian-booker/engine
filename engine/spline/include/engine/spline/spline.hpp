#pragma once

#include <engine/core/math.hpp>
#include <vector>
#include <string>
#include <memory>

namespace engine::spline {

using namespace engine::core;

// Spline interpolation mode
enum class SplineMode : uint8_t {
    Linear,         // Linear interpolation between points
    Bezier,         // Cubic bezier with control points
    CatmullRom,     // Catmull-Rom (smooth through points)
    BSpline         // B-spline (approximating)
};

// How the spline handles its endpoints
enum class SplineEndMode : uint8_t {
    Clamp,          // Stop at endpoints
    Loop,           // Connect end to start
    PingPong        // Reverse direction at endpoints
};

// A single control point on a spline
struct SplinePoint {
    Vec3 position{0.0f};
    Vec3 tangent_in{0.0f};      // Incoming tangent (for bezier)
    Vec3 tangent_out{0.0f};     // Outgoing tangent (for bezier)
    float roll = 0.0f;          // Roll angle in radians

    // User data
    float custom_data = 0.0f;   // Custom per-point data (e.g., speed multiplier)

    SplinePoint() = default;
    explicit SplinePoint(const Vec3& pos) : position(pos) {}
    SplinePoint(const Vec3& pos, const Vec3& tan_in, const Vec3& tan_out)
        : position(pos), tangent_in(tan_in), tangent_out(tan_out) {}
};

// Result of evaluating a spline at a parameter t
struct SplineEvalResult {
    Vec3 position{0.0f};
    Vec3 tangent{0.0f, 0.0f, 1.0f};    // Normalized direction
    Vec3 normal{0.0f, 1.0f, 0.0f};      // Up vector
    Vec3 binormal{1.0f, 0.0f, 0.0f};    // Right vector
    float roll = 0.0f;
    float custom_data = 0.0f;
};

// Result of finding the nearest point on a spline
struct SplineNearestResult {
    float t = 0.0f;                     // Parameter on spline [0, 1]
    Vec3 position{0.0f};                // Closest point position
    float distance = 0.0f;              // Distance from query point
    int segment_index = 0;              // Which segment the point is on
};

// Base spline interface
class Spline {
public:
    virtual ~Spline() = default;

    // Spline type
    virtual SplineMode mode() const = 0;

    // Point management
    virtual size_t point_count() const = 0;
    virtual const SplinePoint& get_point(size_t index) const = 0;
    virtual void set_point(size_t index, const SplinePoint& point) = 0;
    virtual void add_point(const SplinePoint& point) = 0;
    virtual void insert_point(size_t index, const SplinePoint& point) = 0;
    virtual void remove_point(size_t index) = 0;
    virtual void clear_points() = 0;

    // Evaluation
    virtual SplineEvalResult evaluate(float t) const = 0;
    virtual Vec3 evaluate_position(float t) const = 0;
    virtual Vec3 evaluate_tangent(float t) const = 0;

    // Arc length
    virtual float get_length() const = 0;
    virtual float get_length_to(float t) const = 0;

    // Arc-length parameterization (uniform speed)
    virtual float get_t_at_distance(float distance) const = 0;
    virtual SplineEvalResult evaluate_at_distance(float distance) const = 0;

    // Nearest point queries
    virtual SplineNearestResult find_nearest_point(const Vec3& position) const = 0;
    virtual float find_nearest_t(const Vec3& position) const = 0;

    // Properties
    SplineEndMode end_mode = SplineEndMode::Clamp;
    bool auto_tangents = true;          // Automatically compute tangents
    float tension = 0.5f;               // Tension for auto-tangent calculation

    // Bounding box
    virtual AABB get_bounds() const = 0;

    // Subdivision for rendering/collision
    virtual std::vector<Vec3> tessellate(int subdivisions_per_segment = 10) const = 0;

    // Serialization helpers
    const std::vector<SplinePoint>& get_points() const { return m_points; }
    void set_points(const std::vector<SplinePoint>& points) {
        m_points = points;
        invalidate_cache();
    }

protected:
    std::vector<SplinePoint> m_points;

    // Cache invalidation
    mutable bool m_cache_valid = false;
    mutable float m_cached_length = 0.0f;
    mutable std::vector<float> m_cached_segment_lengths;
    mutable std::vector<float> m_cached_cumulative_lengths;

    virtual void invalidate_cache() const {
        m_cache_valid = false;
    }

    virtual void update_cache() const = 0;

    // Helper: get segment index and local t from global t
    void get_segment(float t, int& segment_index, float& local_t) const;

    // Helper: clamp or wrap t based on end mode
    float normalize_t(float t) const;
};

// Factory function to create splines
std::unique_ptr<Spline> create_spline(SplineMode mode);

// Utility functions
namespace SplineUtils {

// Generate a circle spline
std::vector<SplinePoint> make_circle(float radius, int num_points = 8);

// Generate a helix spline
std::vector<SplinePoint> make_helix(float radius, float height, float turns, int points_per_turn = 8);

// Generate a figure-8 spline
std::vector<SplinePoint> make_figure8(float size, int num_points = 16);

// Calculate smooth tangents for a set of points (Catmull-Rom style)
void compute_smooth_tangents(std::vector<SplinePoint>& points, float tension = 0.5f, bool loop = false);

// Sample spline at regular arc-length intervals
std::vector<SplineEvalResult> sample_uniform(const Spline& spline, int num_samples);

// Calculate total rotation around spline (for detecting twists)
float calculate_total_twist(const Spline& spline, int samples = 100);

} // namespace SplineUtils

} // namespace engine::spline
