#include <engine/physics/buoyancy_component.hpp>
#include <engine/physics/water_volume.hpp>
#include <engine/physics/physics_world.hpp>
#include <cmath>
#include <algorithm>

namespace engine::physics {

// Constants
constexpr float GRAVITY = 9.81f;
constexpr float MIN_SUBMERGED_FRACTION = 0.001f;

// Calculate buoyancy force for a sphere at a given position
static BuoyancyResult calculate_sphere_buoyancy(
    const Vec3& center,
    float radius,
    float volume,
    const WaterVolume& water,
    float water_density)
{
    BuoyancyResult result;

    float surface_height = water.get_surface_height_at(center);
    float depth = surface_height - center.y;

    if (depth <= -radius) {
        // Completely above water
        result.submerged_fraction = 0.0f;
        return result;
    }

    if (depth >= radius) {
        // Completely submerged
        result.submerged_volume = volume;
        result.submerged_fraction = 1.0f;
    } else {
        // Partially submerged - use spherical cap formula
        float h = depth + radius;  // Height of submerged cap
        if (h < 0.0f) h = 0.0f;
        if (h > 2.0f * radius) h = 2.0f * radius;

        // Volume of spherical cap: V = (π * h² * (3r - h)) / 3
        float cap_volume = (3.14159265f * h * h * (3.0f * radius - h)) / 3.0f;
        float full_sphere_volume = (4.0f / 3.0f) * 3.14159265f * radius * radius * radius;

        result.submerged_fraction = cap_volume / full_sphere_volume;
        result.submerged_volume = volume * result.submerged_fraction;
    }

    // Buoyancy force: F = ρ * g * V_submerged
    float buoyancy_magnitude = water_density * GRAVITY * result.submerged_volume;
    result.force = Vec3{0.0f, buoyancy_magnitude, 0.0f};

    // Center of buoyancy (approximation for sphere)
    result.center_of_buoyancy = center;
    if (result.submerged_fraction < 1.0f && result.submerged_fraction > 0.0f) {
        // Move center of buoyancy down for partially submerged sphere
        float offset = radius * (1.0f - result.submerged_fraction) * 0.5f;
        result.center_of_buoyancy.y -= offset;
    }

    return result;
}

// Calculate buoyancy using automatic mode (estimate from shape bounds)
BuoyancyResult calculate_automatic_buoyancy(
    const Vec3& body_position,
    const Quat& body_rotation,
    const BodyShapeInfo& shape_info,
    const WaterVolume& water,
    float water_density,
    float buoyancy_multiplier)
{
    BuoyancyResult result;

    // Estimate volume and bounds from shape
    float estimated_volume = 0.0f;
    float estimated_radius = 0.0f;

    switch (shape_info.type) {
        case ShapeType::Box: {
            const Vec3& half = shape_info.dimensions;
            estimated_volume = 8.0f * half.x * half.y * half.z;
            estimated_radius = std::sqrt(half.x * half.x + half.y * half.y + half.z * half.z);
            break;
        }
        case ShapeType::Sphere: {
            float r = shape_info.dimensions.x;
            estimated_volume = (4.0f / 3.0f) * 3.14159265f * r * r * r;
            estimated_radius = r;
            break;
        }
        case ShapeType::Capsule: {
            float r = shape_info.dimensions.x;
            float h = shape_info.dimensions.y * 2.0f;
            estimated_volume = 3.14159265f * r * r * (h + (4.0f / 3.0f) * r);
            estimated_radius = std::max(r, h * 0.5f + r);
            break;
        }
        case ShapeType::Cylinder: {
            float r = shape_info.dimensions.x;
            float h = shape_info.dimensions.y * 2.0f;
            estimated_volume = 3.14159265f * r * r * h;
            estimated_radius = std::sqrt(r * r + (h * 0.5f) * (h * 0.5f));
            break;
        }
        default:
            // For complex shapes, use bounding box approximation
            const Vec3& half = shape_info.dimensions;
            estimated_volume = 8.0f * half.x * half.y * half.z * 0.6f;  // 60% fill factor
            estimated_radius = std::sqrt(half.x * half.x + half.y * half.y + half.z * half.z);
            break;
    }

    // Use sphere approximation for buoyancy calculation
    result = calculate_sphere_buoyancy(
        body_position,
        estimated_radius,
        estimated_volume,
        water,
        water_density
    );

    // Apply multiplier
    result.force.x *= buoyancy_multiplier;
    result.force.y *= buoyancy_multiplier;
    result.force.z *= buoyancy_multiplier;
    result.submerged_volume *= buoyancy_multiplier;

    return result;
}

// Calculate buoyancy using manual sample points
BuoyancyResult calculate_manual_buoyancy(
    const Vec3& body_position,
    const Quat& body_rotation,
    const std::vector<BuoyancyPoint>& points,
    const WaterVolume& water,
    float water_density,
    float buoyancy_multiplier)
{
    BuoyancyResult result;

    if (points.empty()) {
        return result;
    }

    Vec3 total_force{0.0f};
    Vec3 total_torque{0.0f};
    Vec3 weighted_center{0.0f};
    float total_volume = 0.0f;
    float total_submerged_volume = 0.0f;

    for (const auto& point : points) {
        // Transform point to world space
        // Simplified rotation (for full quaternion rotation would need proper math)
        Vec3 world_point = body_position + point.local_position;

        // Calculate buoyancy for this sample point
        BuoyancyResult point_result = calculate_sphere_buoyancy(
            world_point,
            point.radius,
            point.volume,
            water,
            water_density
        );

        total_force.x += point_result.force.x;
        total_force.y += point_result.force.y;
        total_force.z += point_result.force.z;

        total_submerged_volume += point_result.submerged_volume;
        total_volume += point.volume;

        // Calculate torque from this point
        Vec3 r = world_point - body_position;
        // Cross product: torque = r × F
        total_torque.x += r.y * point_result.force.z - r.z * point_result.force.y;
        total_torque.y += r.z * point_result.force.x - r.x * point_result.force.z;
        total_torque.z += r.x * point_result.force.y - r.y * point_result.force.x;

        // Weighted center for center of buoyancy
        if (point_result.submerged_volume > 0.0f) {
            weighted_center.x += world_point.x * point_result.submerged_volume;
            weighted_center.y += world_point.y * point_result.submerged_volume;
            weighted_center.z += world_point.z * point_result.submerged_volume;
        }
    }

    // Apply multiplier
    result.force.x = total_force.x * buoyancy_multiplier;
    result.force.y = total_force.y * buoyancy_multiplier;
    result.force.z = total_force.z * buoyancy_multiplier;

    result.torque.x = total_torque.x * buoyancy_multiplier;
    result.torque.y = total_torque.y * buoyancy_multiplier;
    result.torque.z = total_torque.z * buoyancy_multiplier;

    result.submerged_volume = total_submerged_volume;
    result.submerged_fraction = (total_volume > 0.0f) ?
        (total_submerged_volume / total_volume) : 0.0f;

    // Calculate center of buoyancy
    if (total_submerged_volume > 0.0f) {
        result.center_of_buoyancy.x = weighted_center.x / total_submerged_volume;
        result.center_of_buoyancy.y = weighted_center.y / total_submerged_volume;
        result.center_of_buoyancy.z = weighted_center.z / total_submerged_volume;
    } else {
        result.center_of_buoyancy = body_position;
    }

    return result;
}

// Calculate buoyancy using voxel grid
BuoyancyResult calculate_voxel_buoyancy(
    const Vec3& body_position,
    const Quat& body_rotation,
    const BodyShapeInfo& shape_info,
    const Vec3& voxel_resolution,
    uint32_t max_voxels,
    const WaterVolume& water,
    float water_density,
    float buoyancy_multiplier)
{
    BuoyancyResult result;

    // Get bounding box
    Vec3 half_extents = shape_info.dimensions;

    // Calculate voxel counts (limited by max_voxels)
    int nx = static_cast<int>(std::ceil(2.0f * half_extents.x / voxel_resolution.x));
    int ny = static_cast<int>(std::ceil(2.0f * half_extents.y / voxel_resolution.y));
    int nz = static_cast<int>(std::ceil(2.0f * half_extents.z / voxel_resolution.z));

    // Limit total voxels
    int total_voxels = nx * ny * nz;
    if (total_voxels > static_cast<int>(max_voxels)) {
        float scale = std::cbrt(static_cast<float>(max_voxels) / static_cast<float>(total_voxels));
        nx = std::max(1, static_cast<int>(nx * scale));
        ny = std::max(1, static_cast<int>(ny * scale));
        nz = std::max(1, static_cast<int>(nz * scale));
    }

    // Voxel dimensions
    float vx = 2.0f * half_extents.x / nx;
    float vy = 2.0f * half_extents.y / ny;
    float vz = 2.0f * half_extents.z / nz;
    float voxel_volume = vx * vy * vz;
    float voxel_radius = std::sqrt(vx * vx + vy * vy + vz * vz) * 0.5f;

    Vec3 total_force{0.0f};
    Vec3 total_torque{0.0f};
    Vec3 weighted_center{0.0f};
    float total_submerged_volume = 0.0f;
    int submerged_count = 0;

    // Iterate over voxel grid
    for (int ix = 0; ix < nx; ++ix) {
        for (int iy = 0; iy < ny; ++iy) {
            for (int iz = 0; iz < nz; ++iz) {
                // Voxel center in local space
                Vec3 local_pos{
                    -half_extents.x + vx * (ix + 0.5f),
                    -half_extents.y + vy * (iy + 0.5f),
                    -half_extents.z + vz * (iz + 0.5f)
                };

                // Transform to world space
                Vec3 world_pos = body_position + local_pos;

                // Check if voxel is submerged
                float surface_height = water.get_surface_height_at(world_pos);
                float depth = surface_height - world_pos.y;

                if (depth > -voxel_radius * 0.5f) {
                    // At least partially submerged
                    float submerged_fraction = std::clamp(
                        (depth + voxel_radius * 0.5f) / voxel_radius,
                        0.0f, 1.0f
                    );

                    float submerged_vol = voxel_volume * submerged_fraction;
                    float buoyancy_force = water_density * GRAVITY * submerged_vol;

                    Vec3 force{0.0f, buoyancy_force, 0.0f};

                    total_force.y += buoyancy_force;
                    total_submerged_volume += submerged_vol;

                    // Calculate torque
                    Vec3 r = world_pos - body_position;
                    total_torque.x += r.y * force.z - r.z * force.y;
                    total_torque.y += r.z * force.x - r.x * force.z;
                    total_torque.z += r.x * force.y - r.y * force.x;

                    // Weighted center
                    weighted_center.x += world_pos.x * submerged_vol;
                    weighted_center.y += world_pos.y * submerged_vol;
                    weighted_center.z += world_pos.z * submerged_vol;

                    ++submerged_count;
                }
            }
        }
    }

    // Apply multiplier
    result.force.x = total_force.x * buoyancy_multiplier;
    result.force.y = total_force.y * buoyancy_multiplier;
    result.force.z = total_force.z * buoyancy_multiplier;

    result.torque.x = total_torque.x * buoyancy_multiplier;
    result.torque.y = total_torque.y * buoyancy_multiplier;
    result.torque.z = total_torque.z * buoyancy_multiplier;

    result.submerged_volume = total_submerged_volume;

    float total_volume = 8.0f * half_extents.x * half_extents.y * half_extents.z;
    result.submerged_fraction = (total_volume > 0.0f) ?
        (total_submerged_volume / total_volume) : 0.0f;

    // Calculate center of buoyancy
    if (total_submerged_volume > 0.0f) {
        result.center_of_buoyancy.x = weighted_center.x / total_submerged_volume;
        result.center_of_buoyancy.y = weighted_center.y / total_submerged_volume;
        result.center_of_buoyancy.z = weighted_center.z / total_submerged_volume;
    } else {
        result.center_of_buoyancy = body_position;
    }

    return result;
}

// Calculate water drag forces
Vec3 calculate_water_drag(
    const Vec3& velocity,
    const Vec3& angular_velocity,
    float submerged_fraction,
    float linear_drag,
    float angular_drag,
    float water_density)
{
    if (submerged_fraction < MIN_SUBMERGED_FRACTION) {
        return Vec3{0.0f};
    }

    // Drag force: F_drag = -0.5 * ρ * Cd * A * v²
    // Simplified: F_drag = -drag_coefficient * v * |v|

    float speed_sq = velocity.x * velocity.x +
                    velocity.y * velocity.y +
                    velocity.z * velocity.z;

    if (speed_sq < 0.0001f) {
        return Vec3{0.0f};
    }

    float speed = std::sqrt(speed_sq);

    // Scale drag by submerged fraction
    float effective_drag = linear_drag * submerged_fraction * water_density * 0.001f;

    Vec3 drag_force{
        -velocity.x * speed * effective_drag,
        -velocity.y * speed * effective_drag,
        -velocity.z * speed * effective_drag
    };

    return drag_force;
}

Vec3 calculate_water_angular_drag(
    const Vec3& angular_velocity,
    float submerged_fraction,
    float angular_drag,
    float water_density)
{
    if (submerged_fraction < MIN_SUBMERGED_FRACTION) {
        return Vec3{0.0f};
    }

    float omega_sq = angular_velocity.x * angular_velocity.x +
                    angular_velocity.y * angular_velocity.y +
                    angular_velocity.z * angular_velocity.z;

    if (omega_sq < 0.0001f) {
        return Vec3{0.0f};
    }

    float omega = std::sqrt(omega_sq);

    // Scale drag by submerged fraction
    float effective_drag = angular_drag * submerged_fraction * water_density * 0.0001f;

    Vec3 drag_torque{
        -angular_velocity.x * omega * effective_drag,
        -angular_velocity.y * omega * effective_drag,
        -angular_velocity.z * omega * effective_drag
    };

    return drag_torque;
}

} // namespace engine::physics
