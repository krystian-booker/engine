#pragma once

#include <engine/core/math.hpp>
#include <cstdint>
#include <vector>

namespace engine::physics {

using namespace engine::core;

// Shape types
enum class ShapeType : uint8_t {
    Box,
    Sphere,
    Capsule,
    Cylinder,
    ConvexHull,
    Mesh,
    Compound
};

// Base shape settings
struct ShapeSettings {
    ShapeType type = ShapeType::Box;
    Vec3 center_offset{0.0f};  // Local offset from entity center
    Quat rotation_offset{1.0f, 0.0f, 0.0f, 0.0f};
};

// Box shape
struct BoxShapeSettings : ShapeSettings {
    Vec3 half_extents{0.5f};

    BoxShapeSettings() { type = ShapeType::Box; }
    BoxShapeSettings(const Vec3& extents) : half_extents(extents) { type = ShapeType::Box; }
};

// Sphere shape
struct SphereShapeSettings : ShapeSettings {
    float radius = 0.5f;

    SphereShapeSettings() { type = ShapeType::Sphere; }
    SphereShapeSettings(float r) : radius(r) { type = ShapeType::Sphere; }
};

// Capsule shape (cylinder with hemispherical caps)
struct CapsuleShapeSettings : ShapeSettings {
    float radius = 0.5f;
    float half_height = 0.5f;  // Half height of the cylindrical part

    CapsuleShapeSettings() { type = ShapeType::Capsule; }
    CapsuleShapeSettings(float r, float h) : radius(r), half_height(h) { type = ShapeType::Capsule; }
};

// Cylinder shape
struct CylinderShapeSettings : ShapeSettings {
    float radius = 0.5f;
    float half_height = 0.5f;

    CylinderShapeSettings() { type = ShapeType::Cylinder; }
    CylinderShapeSettings(float r, float h) : radius(r), half_height(h) { type = ShapeType::Cylinder; }
};

// Convex hull shape (from points)
struct ConvexHullShapeSettings : ShapeSettings {
    std::vector<Vec3> points;

    ConvexHullShapeSettings() { type = ShapeType::ConvexHull; }
};

// Triangle mesh shape (for static geometry)
struct MeshShapeSettings : ShapeSettings {
    std::vector<Vec3> vertices;
    std::vector<uint32_t> indices;

    MeshShapeSettings() { type = ShapeType::Mesh; }
};

// Compound shape (multiple shapes combined)
struct CompoundShapeSettings : ShapeSettings {
    struct Child {
        ShapeSettings* shape;
        Vec3 position{0.0f};
        Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    };
    std::vector<Child> children;

    CompoundShapeSettings() { type = ShapeType::Compound; }
};

} // namespace engine::physics
