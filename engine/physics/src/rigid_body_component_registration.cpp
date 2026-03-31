#include <engine/physics/rigid_body_component.hpp>
#include <engine/reflect/reflect.hpp>

namespace {

using namespace engine::physics;
using namespace engine::reflect;
using engine::core::Quat;
using engine::core::Vec3;

ShapeType get_shape_type(const RigidBodyComponent& rb) {
    return std::visit([](const auto& shape) { return shape.type; }, rb.shape);
}

void set_shape_type(RigidBodyComponent& rb, ShapeType type) {
    switch (type) {
        case ShapeType::Box:
            rb.shape = BoxShapeSettings{};
            break;
        case ShapeType::Sphere:
            rb.shape = SphereShapeSettings{};
            break;
        case ShapeType::Capsule:
            rb.shape = CapsuleShapeSettings{};
            break;
        case ShapeType::Cylinder:
            rb.shape = CylinderShapeSettings{};
            break;
        case ShapeType::ConvexHull:
            rb.shape = ConvexHullShapeSettings{};
            break;
        case ShapeType::Mesh:
            rb.shape = MeshShapeSettings{};
            break;
        case ShapeType::HeightField:
            rb.shape = HeightFieldShapeSettings{};
            break;
        case ShapeType::Compound:
            rb.shape = CompoundShapeSettings{};
            break;
    }
}

template<typename ShapeT>
const ShapeT* get_shape_if(const RigidBodyComponent& rb) {
    return std::get_if<ShapeT>(&rb.shape);
}

template<typename ShapeT>
ShapeT& ensure_shape(RigidBodyComponent& rb, ShapeType type) {
    if (!std::holds_alternative<ShapeT>(rb.shape)) {
        set_shape_type(rb, type);
    }
    return std::get<ShapeT>(rb.shape);
}

Vec3 get_center_offset(const RigidBodyComponent& rb) {
    return std::visit([](const auto& shape) { return shape.center_offset; }, rb.shape);
}

void set_center_offset(RigidBodyComponent& rb, const Vec3& offset) {
    std::visit([&](auto& shape) { shape.center_offset = offset; }, rb.shape);
}

Quat get_rotation_offset(const RigidBodyComponent& rb) {
    return std::visit([](const auto& shape) { return shape.rotation_offset; }, rb.shape);
}

void set_rotation_offset(RigidBodyComponent& rb, const Quat& offset) {
    std::visit([&](auto& shape) { shape.rotation_offset = offset; }, rb.shape);
}

struct BodyTypeRegistrar {
    BodyTypeRegistrar() {
        TypeRegistry::instance().register_enum<BodyType>("BodyType", {
            {BodyType::Static, "Static"},
            {BodyType::Kinematic, "Kinematic"},
            {BodyType::Dynamic, "Dynamic"},
        });
    }
};
static BodyTypeRegistrar _body_type_registrar;

struct ShapeTypeRegistrar {
    ShapeTypeRegistrar() {
        TypeRegistry::instance().register_enum<ShapeType>("ShapeType", {
            {ShapeType::Box, "Box"},
            {ShapeType::Sphere, "Sphere"},
            {ShapeType::Capsule, "Capsule"},
            {ShapeType::Cylinder, "Cylinder"},
            {ShapeType::ConvexHull, "ConvexHull"},
            {ShapeType::Mesh, "Mesh"},
            {ShapeType::HeightField, "HeightField"},
            {ShapeType::Compound, "Compound"},
        });
    }
};
static ShapeTypeRegistrar _shape_type_registrar;

struct PhysicsVectorRegistrar {
    PhysicsVectorRegistrar() {
        auto& type_registry = TypeRegistry::instance();
        type_registry.register_type<Vec3>("Vec3");
        type_registry.register_type<float>("float");
        type_registry.register_type<uint32_t>("uint32_t");
        type_registry.register_vector_type<Vec3>();
        type_registry.register_vector_type<uint32_t>();
        type_registry.register_vector_type<float>();
    }
};
static PhysicsVectorRegistrar _physics_vector_registrar;

struct RigidBodyComponentRegistrar {
    RigidBodyComponentRegistrar() {
        auto& type_registry = TypeRegistry::instance();

        type_registry.register_component<RigidBodyComponent>(
            "RigidBodyComponent",
            TypeMeta()
                .set_display_name("Rigid Body")
                .set_description("Physics rigid body configuration"));

        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::type>("type",
            PropertyMeta().set_display_name("Body Type").set_category("General"));
        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::mass>("mass",
            PropertyMeta().set_display_name("Mass").set_category("General").set_range(0.0f, 10000.0f));
        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::friction>("friction",
            PropertyMeta().set_display_name("Friction").set_category("General").set_range(0.0f, 1.0f));
        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::restitution>("restitution",
            PropertyMeta().set_display_name("Restitution").set_category("General").set_range(0.0f, 1.0f));
        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::linear_damping>("linear_damping",
            PropertyMeta().set_display_name("Linear Damping").set_category("General").set_range(0.0f, 10.0f));
        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::angular_damping>("angular_damping",
            PropertyMeta().set_display_name("Angular Damping").set_category("General").set_range(0.0f, 10.0f));
        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::layer>("layer",
            PropertyMeta().set_display_name("Layer").set_category("Collision"));
        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::is_sensor>("is_sensor",
            PropertyMeta().set_display_name("Sensor").set_category("Collision"));
        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::sync_to_transform>("sync_to_transform",
            PropertyMeta().set_display_name("Sync To Transform").set_category("Behavior"));
        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::allow_sleep>("allow_sleep",
            PropertyMeta().set_display_name("Allow Sleep").set_category("Behavior"));
        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::lock_rotation_x>("lock_rotation_x",
            PropertyMeta().set_display_name("Lock Rotation X").set_category("Constraints"));
        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::lock_rotation_y>("lock_rotation_y",
            PropertyMeta().set_display_name("Lock Rotation Y").set_category("Constraints"));
        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::lock_rotation_z>("lock_rotation_z",
            PropertyMeta().set_display_name("Lock Rotation Z").set_category("Constraints"));

        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::type>(
            "shape_type",
            PropertyMeta().set_display_name("Shape Type").set_category("Shape"),
            [](const RigidBodyComponent& rb) { return get_shape_type(rb); },
            [](RigidBodyComponent& rb, ShapeType type) { set_shape_type(rb, type); });

        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::type>(
            "shape_center_offset",
            PropertyMeta().set_display_name("Center Offset").set_category("Shape"),
            [](const RigidBodyComponent& rb) { return get_center_offset(rb); },
            [](RigidBodyComponent& rb, const Vec3& value) { set_center_offset(rb, value); });

        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::type>(
            "shape_rotation_offset",
            PropertyMeta().set_display_name("Rotation Offset").set_category("Shape"),
            [](const RigidBodyComponent& rb) { return get_rotation_offset(rb); },
            [](RigidBodyComponent& rb, const Quat& value) { set_rotation_offset(rb, value); });

        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::type>(
            "box_half_extents",
            PropertyMeta().set_display_name("Box Half Extents").set_category("Shape"),
            [](const RigidBodyComponent& rb) { return get_shape_if<BoxShapeSettings>(rb) ? get_shape_if<BoxShapeSettings>(rb)->half_extents : Vec3{0.5f}; },
            [](RigidBodyComponent& rb, const Vec3& value) {
                if (auto* shape = std::get_if<BoxShapeSettings>(&rb.shape)) {
                    shape->half_extents = value;
                }
            });

        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::type>(
            "sphere_radius",
            PropertyMeta().set_display_name("Sphere Radius").set_category("Shape").set_range(0.0f, 1000.0f),
            [](const RigidBodyComponent& rb) { return get_shape_if<SphereShapeSettings>(rb) ? get_shape_if<SphereShapeSettings>(rb)->radius : 0.5f; },
            [](RigidBodyComponent& rb, float value) {
                if (auto* shape = std::get_if<SphereShapeSettings>(&rb.shape)) {
                    shape->radius = value;
                }
            });

        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::type>(
            "capsule_radius",
            PropertyMeta().set_display_name("Capsule Radius").set_category("Shape").set_range(0.0f, 1000.0f),
            [](const RigidBodyComponent& rb) { return get_shape_if<CapsuleShapeSettings>(rb) ? get_shape_if<CapsuleShapeSettings>(rb)->radius : 0.5f; },
            [](RigidBodyComponent& rb, float value) {
                if (auto* shape = std::get_if<CapsuleShapeSettings>(&rb.shape)) {
                    shape->radius = value;
                }
            });

        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::type>(
            "capsule_half_height",
            PropertyMeta().set_display_name("Capsule Half Height").set_category("Shape").set_range(0.0f, 1000.0f),
            [](const RigidBodyComponent& rb) { return get_shape_if<CapsuleShapeSettings>(rb) ? get_shape_if<CapsuleShapeSettings>(rb)->half_height : 0.5f; },
            [](RigidBodyComponent& rb, float value) {
                if (auto* shape = std::get_if<CapsuleShapeSettings>(&rb.shape)) {
                    shape->half_height = value;
                }
            });

        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::type>(
            "cylinder_radius",
            PropertyMeta().set_display_name("Cylinder Radius").set_category("Shape").set_range(0.0f, 1000.0f),
            [](const RigidBodyComponent& rb) { return get_shape_if<CylinderShapeSettings>(rb) ? get_shape_if<CylinderShapeSettings>(rb)->radius : 0.5f; },
            [](RigidBodyComponent& rb, float value) {
                if (auto* shape = std::get_if<CylinderShapeSettings>(&rb.shape)) {
                    shape->radius = value;
                }
            });

        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::type>(
            "cylinder_half_height",
            PropertyMeta().set_display_name("Cylinder Half Height").set_category("Shape").set_range(0.0f, 1000.0f),
            [](const RigidBodyComponent& rb) { return get_shape_if<CylinderShapeSettings>(rb) ? get_shape_if<CylinderShapeSettings>(rb)->half_height : 0.5f; },
            [](RigidBodyComponent& rb, float value) {
                if (auto* shape = std::get_if<CylinderShapeSettings>(&rb.shape)) {
                    shape->half_height = value;
                }
            });

        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::type>(
            "convex_points",
            PropertyMeta().set_display_name("Convex Points").set_category("Shape"),
            [](const RigidBodyComponent& rb) { return get_shape_if<ConvexHullShapeSettings>(rb) ? get_shape_if<ConvexHullShapeSettings>(rb)->points : std::vector<Vec3>{}; },
            [](RigidBodyComponent& rb, const std::vector<Vec3>& value) {
                if (auto* shape = std::get_if<ConvexHullShapeSettings>(&rb.shape)) {
                    shape->points = value;
                }
            });

        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::type>(
            "mesh_vertices",
            PropertyMeta().set_display_name("Mesh Vertices").set_category("Shape"),
            [](const RigidBodyComponent& rb) { return get_shape_if<MeshShapeSettings>(rb) ? get_shape_if<MeshShapeSettings>(rb)->vertices : std::vector<Vec3>{}; },
            [](RigidBodyComponent& rb, const std::vector<Vec3>& value) {
                if (auto* shape = std::get_if<MeshShapeSettings>(&rb.shape)) {
                    shape->vertices = value;
                }
            });

        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::type>(
            "mesh_indices",
            PropertyMeta().set_display_name("Mesh Indices").set_category("Shape"),
            [](const RigidBodyComponent& rb) { return get_shape_if<MeshShapeSettings>(rb) ? get_shape_if<MeshShapeSettings>(rb)->indices : std::vector<uint32_t>{}; },
            [](RigidBodyComponent& rb, const std::vector<uint32_t>& value) {
                if (auto* shape = std::get_if<MeshShapeSettings>(&rb.shape)) {
                    shape->indices = value;
                }
            });

        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::type>(
            "heightfield_heights",
            PropertyMeta().set_display_name("HeightField Heights").set_category("Shape"),
            [](const RigidBodyComponent& rb) { return get_shape_if<HeightFieldShapeSettings>(rb) ? get_shape_if<HeightFieldShapeSettings>(rb)->heights : std::vector<float>{}; },
            [](RigidBodyComponent& rb, const std::vector<float>& value) {
                if (auto* shape = std::get_if<HeightFieldShapeSettings>(&rb.shape)) {
                    shape->heights = value;
                }
            });

        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::type>(
            "heightfield_num_rows",
            PropertyMeta().set_display_name("HeightField Rows").set_category("Shape"),
            [](const RigidBodyComponent& rb) { return get_shape_if<HeightFieldShapeSettings>(rb) ? get_shape_if<HeightFieldShapeSettings>(rb)->num_rows : 0u; },
            [](RigidBodyComponent& rb, uint32_t value) {
                if (auto* shape = std::get_if<HeightFieldShapeSettings>(&rb.shape)) {
                    shape->num_rows = value;
                }
            });

        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::type>(
            "heightfield_num_cols",
            PropertyMeta().set_display_name("HeightField Columns").set_category("Shape"),
            [](const RigidBodyComponent& rb) { return get_shape_if<HeightFieldShapeSettings>(rb) ? get_shape_if<HeightFieldShapeSettings>(rb)->num_cols : 0u; },
            [](RigidBodyComponent& rb, uint32_t value) {
                if (auto* shape = std::get_if<HeightFieldShapeSettings>(&rb.shape)) {
                    shape->num_cols = value;
                }
            });

        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::type>(
            "heightfield_scale",
            PropertyMeta().set_display_name("HeightField Scale").set_category("Shape"),
            [](const RigidBodyComponent& rb) { return get_shape_if<HeightFieldShapeSettings>(rb) ? get_shape_if<HeightFieldShapeSettings>(rb)->scale : Vec3{1.0f}; },
            [](RigidBodyComponent& rb, const Vec3& value) {
                if (auto* shape = std::get_if<HeightFieldShapeSettings>(&rb.shape)) {
                    shape->scale = value;
                }
            });

        type_registry.register_property<RigidBodyComponent, &RigidBodyComponent::type>(
            "heightfield_offset",
            PropertyMeta().set_display_name("HeightField Offset").set_category("Shape"),
            [](const RigidBodyComponent& rb) { return get_shape_if<HeightFieldShapeSettings>(rb) ? get_shape_if<HeightFieldShapeSettings>(rb)->offset : Vec3{0.0f}; },
            [](RigidBodyComponent& rb, const Vec3& value) {
                if (auto* shape = std::get_if<HeightFieldShapeSettings>(&rb.shape)) {
                    shape->offset = value;
                }
            });
    }
};
static RigidBodyComponentRegistrar _rigid_body_component_registrar;

} // namespace

namespace engine::physics {

void ensure_rigid_body_component_registration() {}

} // namespace engine::physics
