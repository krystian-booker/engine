// Jolt Physics implementation
// This file contains all Jolt-specific code to isolate it from the public API

#include <engine/physics/physics_world.hpp>
#include <engine/core/log.hpp>

// Jolt includes
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseQuery.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>

#include <unordered_map>
#include <mutex>

namespace engine::physics {

using namespace engine::core;
using namespace JPH;

// Custom allocators for Jolt
static void* JoltAlloc(size_t size) { return malloc(size); }
static void JoltFree(void* ptr) { free(ptr); }
static void* JoltAlignedAlloc(size_t size, size_t alignment) {
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#else
    void* ptr = nullptr;
    posix_memalign(&ptr, alignment, size);
    return ptr;
#endif
}
static void JoltAlignedFree(void* ptr) {
#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

// Broad phase layer implementation
namespace BroadPhaseLayers {
    static constexpr BroadPhaseLayer NON_MOVING(0);
    static constexpr BroadPhaseLayer MOVING(1);
    static constexpr uint NUM_LAYERS(2);
}

class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
public:
    uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }

    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override {
        return layer == 0 ? BroadPhaseLayers::NON_MOVING : BroadPhaseLayers::MOVING;
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(BroadPhaseLayer layer) const override {
        switch ((BroadPhaseLayer::Type)layer) {
            case (BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
            case (BroadPhaseLayer::Type)BroadPhaseLayers::MOVING: return "MOVING";
            default: return "UNKNOWN";
        }
    }
#endif
};

class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(ObjectLayer layer1, BroadPhaseLayer layer2) const override {
        if (layer1 == 0) return layer2 == BroadPhaseLayers::NON_MOVING || layer2 == BroadPhaseLayers::MOVING;
        return layer2 == BroadPhaseLayers::MOVING;
    }
};

class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter {
public:
    CollisionFilter* filter = nullptr;

    bool ShouldCollide(ObjectLayer obj1, ObjectLayer obj2) const override {
        if (filter) {
            return filter->should_collide(static_cast<uint16_t>(obj1), static_cast<uint16_t>(obj2));
        }
        return true;  // Default: collide
    }
};

// Physics world implementation
struct PhysicsWorld::Impl {
    std::unique_ptr<TempAllocatorImpl> temp_allocator;
    std::unique_ptr<JobSystemThreadPool> job_system;
    std::unique_ptr<PhysicsSystem> physics_system;

    BPLayerInterfaceImpl broad_phase_layer_interface;
    ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_filter;
    ObjectLayerPairFilterImpl object_layer_pair_filter;

    CollisionFilter collision_filter;
    CollisionCallback collision_callback;
    Vec3 gravity{0.0f, -9.81f, 0.0f};

    mutable std::mutex body_map_mutex;
    std::unordered_map<uint32_t, BodyID> body_map;
    uint32_t next_body_id = 1;

    // Constraint storage
    mutable std::mutex constraint_map_mutex;
    std::unordered_map<uint32_t, Ref<Constraint>> constraint_map;
    uint32_t next_constraint_id = 1;

    // Shape info cache for debug rendering (shape type and dimensions per body)
    std::unordered_map<uint32_t, BodyShapeInfo> body_shape_info;

    bool initialized = false;

    // Helper to find PhysicsBodyId from Jolt BodyID (must hold mutex)
    PhysicsBodyId find_body_id(BodyID jolt_id) const {
        for (const auto& [id, jid] : body_map) {
            if (jid == jolt_id) {
                return PhysicsBodyId{id};
            }
        }
        return PhysicsBodyId{};
    }
};

// Forward declarations for impl functions
PhysicsWorld::Impl* create_physics_impl();
void shutdown_physics_impl(PhysicsWorld::Impl* impl);

// Constructor, destructor and move operations must be defined here where Impl is complete
PhysicsWorld::PhysicsWorld()
    : m_impl(create_physics_impl())
{
}

PhysicsWorld::~PhysicsWorld() {
    if (m_impl) {
        shutdown_physics_impl(m_impl.get());
    }
}

PhysicsWorld::PhysicsWorld(PhysicsWorld&&) noexcept = default;
PhysicsWorld& PhysicsWorld::operator=(PhysicsWorld&&) noexcept = default;

// Implementation functions
PhysicsWorld::Impl* create_physics_impl() {
    // Initialize Jolt allocators
    Allocate = JoltAlloc;
    Free = JoltFree;
    AlignedAllocate = JoltAlignedAlloc;
    AlignedFree = JoltAlignedFree;

    // Register all types with the factory
    Factory::sInstance = new Factory();
    RegisterTypes();

    return new PhysicsWorld::Impl();
}

void destroy_physics_impl(PhysicsWorld::Impl* impl) {
    if (impl) {
        shutdown_physics_impl(impl);

        // Destroy factory
        UnregisterTypes();
        delete Factory::sInstance;
        Factory::sInstance = nullptr;
    }
    delete impl;
}

void init_physics_impl(PhysicsWorld::Impl* impl, const PhysicsSettings& settings) {
    if (!impl || impl->initialized) return;

    // Create allocators
    impl->temp_allocator = std::make_unique<TempAllocatorImpl>(10 * 1024 * 1024);
    impl->job_system = std::make_unique<JobSystemThreadPool>(
        cMaxPhysicsJobs, cMaxPhysicsBarriers,
        static_cast<int>(std::thread::hardware_concurrency()) - 1
    );

    // Connect collision filter to layer pair filter
    impl->object_layer_pair_filter.filter = &impl->collision_filter;

    // Create physics system
    const uint max_bodies = 65536;
    const uint num_body_mutexes = 0;
    const uint max_body_pairs = 65536;
    const uint max_contact_constraints = 10240;

    impl->physics_system = std::make_unique<PhysicsSystem>();
    impl->physics_system->Init(
        max_bodies, num_body_mutexes, max_body_pairs, max_contact_constraints,
        impl->broad_phase_layer_interface,
        impl->object_vs_broadphase_filter,
        impl->object_layer_pair_filter
    );

    impl->gravity = settings.gravity;
    impl->physics_system->SetGravity(Vec3Arg(settings.gravity.x, settings.gravity.y, settings.gravity.z));

    impl->initialized = true;
}

void shutdown_physics_impl(PhysicsWorld::Impl* impl) {
    if (!impl || !impl->initialized) return;

    impl->body_map.clear();
    impl->physics_system.reset();
    impl->job_system.reset();
    impl->temp_allocator.reset();
    impl->initialized = false;
}

void step_physics_impl(PhysicsWorld::Impl* impl, double dt) {
    if (!impl || !impl->initialized) return;

    const int collision_steps = 1;
    impl->physics_system->Update(
        static_cast<float>(dt), collision_steps,
        impl->temp_allocator.get(), impl->job_system.get()
    );
}

PhysicsBodyId create_body_impl(PhysicsWorld::Impl* impl, const BodySettings& settings) {
    if (!impl || !impl->initialized) return PhysicsBodyId{};

    // Create shape based on settings
    RefConst<Shape> shape;

    if (settings.shape) {
        switch (settings.shape->type) {
            case ShapeType::Box: {
                auto* box = static_cast<BoxShapeSettings*>(settings.shape);
                shape = new BoxShape(Vec3Arg(box->half_extents.x, box->half_extents.y, box->half_extents.z));
                break;
            }
            case ShapeType::Sphere: {
                auto* sphere = static_cast<SphereShapeSettings*>(settings.shape);
                shape = new SphereShape(sphere->radius);
                break;
            }
            case ShapeType::Capsule: {
                auto* capsule = static_cast<CapsuleShapeSettings*>(settings.shape);
                shape = new CapsuleShape(capsule->half_height, capsule->radius);
                break;
            }
            case ShapeType::Cylinder: {
                auto* cyl = static_cast<CylinderShapeSettings*>(settings.shape);
                shape = new CylinderShape(cyl->half_height, cyl->radius);
                break;
            }
            case ShapeType::ConvexHull: {
                auto* hull = static_cast<ConvexHullShapeSettings*>(settings.shape);
                if (!hull->points.empty()) {
                    JPH::Array<JPH::Vec3> jolt_points;
                    jolt_points.reserve(hull->points.size());
                    for (const auto& p : hull->points) {
                        jolt_points.push_back(JPH::Vec3(p.x, p.y, p.z));
                    }
                    JPH::ConvexHullShapeSettings hull_settings(jolt_points.data(), static_cast<int>(jolt_points.size()));
                    auto result = hull_settings.Create();
                    if (result.IsValid()) {
                        shape = result.Get();
                    } else {
                        log(LogLevel::Error, "Failed to create convex hull shape");
                        shape = new BoxShape(Vec3Arg(0.5f, 0.5f, 0.5f));
                    }
                } else {
                    shape = new BoxShape(Vec3Arg(0.5f, 0.5f, 0.5f));
                }
                break;
            }
            case ShapeType::Mesh: {
                auto* mesh = static_cast<MeshShapeSettings*>(settings.shape);
                if (!mesh->vertices.empty() && !mesh->indices.empty()) {
                    JPH::VertexList vertices;
                    vertices.reserve(mesh->vertices.size());
                    for (const auto& v : mesh->vertices) {
                        vertices.push_back(Float3(v.x, v.y, v.z));
                    }

                    JPH::IndexedTriangleList triangles;
                    triangles.reserve(mesh->indices.size() / 3);
                    for (size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
                        triangles.push_back(IndexedTriangle(
                            mesh->indices[i], mesh->indices[i + 1], mesh->indices[i + 2]));
                    }

                    JPH::MeshShapeSettings mesh_settings(vertices, triangles);
                    auto result = mesh_settings.Create();
                    if (result.IsValid()) {
                        shape = result.Get();
                    } else {
                        log(LogLevel::Error, "Failed to create mesh shape");
                        shape = new BoxShape(Vec3Arg(0.5f, 0.5f, 0.5f));
                    }
                } else {
                    shape = new BoxShape(Vec3Arg(0.5f, 0.5f, 0.5f));
                }
                break;
            }
            case ShapeType::Compound: {
                auto* compound = static_cast<CompoundShapeSettings*>(settings.shape);
                if (!compound->children.empty()) {
                    StaticCompoundShapeSettings compound_settings;
                    for (const auto& child : compound->children) {
                        if (!child.shape) continue;

                        RefConst<Shape> child_shape;
                        // Recursively create child shapes (simplified: only basic shapes)
                        switch (child.shape->type) {
                            case ShapeType::Box: {
                                auto* box = static_cast<BoxShapeSettings*>(child.shape);
                                child_shape = new BoxShape(Vec3Arg(box->half_extents.x, box->half_extents.y, box->half_extents.z));
                                break;
                            }
                            case ShapeType::Sphere: {
                                auto* sphere = static_cast<SphereShapeSettings*>(child.shape);
                                child_shape = new SphereShape(sphere->radius);
                                break;
                            }
                            case ShapeType::Capsule: {
                                auto* capsule = static_cast<CapsuleShapeSettings*>(child.shape);
                                child_shape = new CapsuleShape(capsule->half_height, capsule->radius);
                                break;
                            }
                            default:
                                child_shape = new BoxShape(Vec3Arg(0.5f, 0.5f, 0.5f));
                                break;
                        }
                        compound_settings.AddShape(
                            Vec3Arg(child.position.x, child.position.y, child.position.z),
                            QuatArg(child.rotation.x, child.rotation.y, child.rotation.z, child.rotation.w),
                            child_shape
                        );
                    }
                    auto result = compound_settings.Create();
                    if (result.IsValid()) {
                        shape = result.Get();
                    } else {
                        log(LogLevel::Error, "Failed to create compound shape");
                        shape = new BoxShape(Vec3Arg(0.5f, 0.5f, 0.5f));
                    }
                } else {
                    shape = new BoxShape(Vec3Arg(0.5f, 0.5f, 0.5f));
                }
                break;
            }
            default:
                shape = new BoxShape(Vec3Arg(0.5f, 0.5f, 0.5f));
                break;
        }
    } else {
        shape = new BoxShape(Vec3Arg(0.5f, 0.5f, 0.5f));
    }

    // Determine motion type - use the layer from settings
    EMotionType motion_type;
    ObjectLayer object_layer = static_cast<ObjectLayer>(settings.layer);
    switch (settings.type) {
        case BodyType::Static:
            motion_type = EMotionType::Static;
            break;
        case BodyType::Kinematic:
            motion_type = EMotionType::Kinematic;
            break;
        case BodyType::Dynamic:
        default:
            motion_type = EMotionType::Dynamic;
            break;
    }

    // Create body
    BodyCreationSettings body_settings(
        shape,
        RVec3Arg(settings.position.x, settings.position.y, settings.position.z),
        QuatArg(settings.rotation.x, settings.rotation.y, settings.rotation.z, settings.rotation.w),
        motion_type,
        object_layer
    );

    body_settings.mFriction = settings.friction;
    body_settings.mRestitution = settings.restitution;
    body_settings.mLinearDamping = settings.linear_damping;
    body_settings.mAngularDamping = settings.angular_damping;
    body_settings.mAllowSleeping = settings.allow_sleep;
    body_settings.mIsSensor = settings.is_sensor;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    Body* body = body_interface.CreateBody(body_settings);

    if (!body) {
        log(LogLevel::Error, "Failed to create physics body");
        return PhysicsBodyId{};
    }

    body_interface.AddBody(body->GetID(), EActivation::Activate);

    std::lock_guard<std::mutex> lock(impl->body_map_mutex);
    PhysicsBodyId id{impl->next_body_id++};
    impl->body_map[id.id] = body->GetID();

    // Store shape info for debug rendering
    BodyShapeInfo shape_info;
    if (settings.shape) {
        shape_info.type = settings.shape->type;
        shape_info.center_offset = settings.shape->center_offset;
        switch (settings.shape->type) {
            case ShapeType::Box: {
                auto* box = static_cast<BoxShapeSettings*>(settings.shape);
                shape_info.dimensions = box->half_extents;
                break;
            }
            case ShapeType::Sphere: {
                auto* sphere = static_cast<SphereShapeSettings*>(settings.shape);
                shape_info.dimensions = Vec3{sphere->radius, sphere->radius, sphere->radius};
                break;
            }
            case ShapeType::Capsule: {
                auto* capsule = static_cast<CapsuleShapeSettings*>(settings.shape);
                shape_info.dimensions = Vec3{capsule->radius, capsule->half_height, capsule->radius};
                break;
            }
            case ShapeType::Cylinder: {
                auto* cyl = static_cast<CylinderShapeSettings*>(settings.shape);
                shape_info.dimensions = Vec3{cyl->radius, cyl->half_height, cyl->radius};
                break;
            }
            default:
                shape_info.dimensions = Vec3{0.5f};
                break;
        }
    }
    impl->body_shape_info[id.id] = shape_info;

    return id;
}

void destroy_body_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id) {
    if (!impl || !impl->initialized) return;

    BodyID jolt_id;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        auto it = impl->body_map.find(id.id);
        if (it == impl->body_map.end()) return;
        jolt_id = it->second;
        impl->body_map.erase(it);
        impl->body_shape_info.erase(id.id);
    }

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.RemoveBody(jolt_id);
    body_interface.DestroyBody(jolt_id);
}

bool is_valid_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id) {
    if (!impl) return false;
    std::lock_guard<std::mutex> lock(impl->body_map_mutex);
    return impl->body_map.find(id.id) != impl->body_map.end();
}

// Helper to get Jolt BodyID from our PhysicsBodyId (caller must hold mutex)
static BodyID get_jolt_body_id(PhysicsWorld::Impl* impl, PhysicsBodyId id) {
    auto it = impl->body_map.find(id.id);
    if (it == impl->body_map.end()) return BodyID();
    return it->second;
}

void set_position_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Vec3& pos) {
    if (!impl || !impl->initialized) return;

    BodyID jolt_id;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id = get_jolt_body_id(impl, id);
    }
    if (jolt_id.IsInvalid()) return;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.SetPosition(jolt_id, RVec3Arg(pos.x, pos.y, pos.z), EActivation::Activate);
}

void set_rotation_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Quat& rot) {
    if (!impl || !impl->initialized) return;

    BodyID jolt_id;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id = get_jolt_body_id(impl, id);
    }
    if (jolt_id.IsInvalid()) return;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.SetRotation(jolt_id, QuatArg(rot.x, rot.y, rot.z, rot.w), EActivation::Activate);
}

Vec3 get_position_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id) {
    if (!impl || !impl->initialized) return Vec3{0.0f};

    BodyID jolt_id;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id = get_jolt_body_id(impl, id);
    }
    if (jolt_id.IsInvalid()) return Vec3{0.0f};

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    RVec3 pos = body_interface.GetPosition(jolt_id);
    return Vec3{static_cast<float>(pos.GetX()), static_cast<float>(pos.GetY()), static_cast<float>(pos.GetZ())};
}

Quat get_rotation_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id) {
    if (!impl || !impl->initialized) return Quat{1.0f, 0.0f, 0.0f, 0.0f};

    BodyID jolt_id;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id = get_jolt_body_id(impl, id);
    }
    if (jolt_id.IsInvalid()) return Quat{1.0f, 0.0f, 0.0f, 0.0f};

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    JPH::Quat rot = body_interface.GetRotation(jolt_id);
    return Quat{rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ()};
}

void set_linear_velocity_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Vec3& vel) {
    if (!impl || !impl->initialized) return;

    BodyID jolt_id;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id = get_jolt_body_id(impl, id);
    }
    if (jolt_id.IsInvalid()) return;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.SetLinearVelocity(jolt_id, Vec3Arg(vel.x, vel.y, vel.z));
}

void set_angular_velocity_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Vec3& vel) {
    if (!impl || !impl->initialized) return;

    BodyID jolt_id;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id = get_jolt_body_id(impl, id);
    }
    if (jolt_id.IsInvalid()) return;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.SetAngularVelocity(jolt_id, Vec3Arg(vel.x, vel.y, vel.z));
}

Vec3 get_linear_velocity_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id) {
    if (!impl || !impl->initialized) return Vec3{0.0f};

    BodyID jolt_id;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id = get_jolt_body_id(impl, id);
    }
    if (jolt_id.IsInvalid()) return Vec3{0.0f};

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    JPH::Vec3 vel = body_interface.GetLinearVelocity(jolt_id);
    return Vec3{vel.GetX(), vel.GetY(), vel.GetZ()};
}

Vec3 get_angular_velocity_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id) {
    if (!impl || !impl->initialized) return Vec3{0.0f};

    BodyID jolt_id;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id = get_jolt_body_id(impl, id);
    }
    if (jolt_id.IsInvalid()) return Vec3{0.0f};

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    JPH::Vec3 vel = body_interface.GetAngularVelocity(jolt_id);
    return Vec3{vel.GetX(), vel.GetY(), vel.GetZ()};
}

void add_force_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Vec3& force) {
    if (!impl || !impl->initialized) return;

    BodyID jolt_id;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id = get_jolt_body_id(impl, id);
    }
    if (jolt_id.IsInvalid()) return;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.AddForce(jolt_id, Vec3Arg(force.x, force.y, force.z));
}

void add_force_at_point_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Vec3& force, const Vec3& point) {
    if (!impl || !impl->initialized) return;

    BodyID jolt_id;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id = get_jolt_body_id(impl, id);
    }
    if (jolt_id.IsInvalid()) return;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.AddForce(jolt_id, Vec3Arg(force.x, force.y, force.z), RVec3Arg(point.x, point.y, point.z));
}

void add_torque_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Vec3& torque) {
    if (!impl || !impl->initialized) return;

    BodyID jolt_id;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id = get_jolt_body_id(impl, id);
    }
    if (jolt_id.IsInvalid()) return;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.AddTorque(jolt_id, Vec3Arg(torque.x, torque.y, torque.z));
}

void add_impulse_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Vec3& impulse) {
    if (!impl || !impl->initialized) return;

    BodyID jolt_id;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id = get_jolt_body_id(impl, id);
    }
    if (jolt_id.IsInvalid()) return;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.AddImpulse(jolt_id, Vec3Arg(impulse.x, impulse.y, impulse.z));
}

void add_impulse_at_point_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Vec3& impulse, const Vec3& point) {
    if (!impl || !impl->initialized) return;

    BodyID jolt_id;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id = get_jolt_body_id(impl, id);
    }
    if (jolt_id.IsInvalid()) return;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.AddImpulse(jolt_id, Vec3Arg(impulse.x, impulse.y, impulse.z), RVec3Arg(point.x, point.y, point.z));
}

void set_friction_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, float friction) {
    if (!impl || !impl->initialized) return;

    BodyID jolt_id;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id = get_jolt_body_id(impl, id);
    }
    if (jolt_id.IsInvalid()) return;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.SetFriction(jolt_id, friction);
}

void set_restitution_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, float restitution) {
    if (!impl || !impl->initialized) return;

    BodyID jolt_id;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id = get_jolt_body_id(impl, id);
    }
    if (jolt_id.IsInvalid()) return;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.SetRestitution(jolt_id, restitution);
}

void set_gravity_factor_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, float factor) {
    if (!impl || !impl->initialized) return;

    BodyID jolt_id;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id = get_jolt_body_id(impl, id);
    }
    if (jolt_id.IsInvalid()) return;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.SetGravityFactor(jolt_id, factor);
}

void activate_body_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id) {
    if (!impl || !impl->initialized) return;

    BodyID jolt_id;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id = get_jolt_body_id(impl, id);
    }
    if (jolt_id.IsInvalid()) return;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.ActivateBody(jolt_id);
}

bool is_active_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id) {
    if (!impl || !impl->initialized) return false;

    BodyID jolt_id;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id = get_jolt_body_id(impl, id);
    }
    if (jolt_id.IsInvalid()) return false;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    return body_interface.IsActive(jolt_id);
}

RaycastHit raycast_impl(PhysicsWorld::Impl* impl, const Vec3& origin, const Vec3& dir,
                        float max_dist, uint16_t layer_mask) {
    RaycastHit result;
    if (!impl || !impl->initialized) return result;

    // Create ray from origin to origin + dir * max_dist
    RRayCast ray;
    ray.mOrigin = RVec3(origin.x, origin.y, origin.z);
    ray.mDirection = JPH::Vec3(dir.x * max_dist, dir.y * max_dist, dir.z * max_dist);

    RayCastResult hit;
    if (impl->physics_system->GetNarrowPhaseQuery().CastRay(ray, hit)) {
        result.hit = true;
        result.distance = hit.mFraction * max_dist;

        RVec3 hit_point = ray.GetPointOnRay(hit.mFraction);
        result.point = Vec3{
            static_cast<float>(hit_point.GetX()),
            static_cast<float>(hit_point.GetY()),
            static_cast<float>(hit_point.GetZ())
        };

        // Get normal from the body at hit point
        BodyLockRead lock(impl->physics_system->GetBodyLockInterface(), hit.mBodyID);
        if (lock.Succeeded()) {
            const Body& body = lock.GetBody();
            JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, hit_point);
            result.normal = Vec3{normal.GetX(), normal.GetY(), normal.GetZ()};
        }

        // Map Jolt BodyID back to our PhysicsBodyId
        std::lock_guard<std::mutex> map_lock(impl->body_map_mutex);
        result.body = impl->find_body_id(hit.mBodyID);
    }

    return result;
}

std::vector<RaycastHit> raycast_all_impl(PhysicsWorld::Impl* impl, const Vec3& origin, const Vec3& dir,
                                          float max_dist, uint16_t layer_mask) {
    std::vector<RaycastHit> results;
    if (!impl || !impl->initialized) return results;

    RRayCast ray;
    ray.mOrigin = RVec3(origin.x, origin.y, origin.z);
    ray.mDirection = JPH::Vec3(dir.x * max_dist, dir.y * max_dist, dir.z * max_dist);

    AllHitCollisionCollector<CastRayCollector> collector;
    impl->physics_system->GetNarrowPhaseQuery().CastRay(ray, RayCastSettings(), collector);

    collector.Sort();

    std::lock_guard<std::mutex> map_lock(impl->body_map_mutex);
    for (const auto& hit : collector.mHits) {
        RaycastHit result;
        result.hit = true;
        result.distance = hit.mFraction * max_dist;

        RVec3 hit_point = ray.GetPointOnRay(hit.mFraction);
        result.point = Vec3{
            static_cast<float>(hit_point.GetX()),
            static_cast<float>(hit_point.GetY()),
            static_cast<float>(hit_point.GetZ())
        };

        BodyLockRead lock(impl->physics_system->GetBodyLockInterface(), hit.mBodyID);
        if (lock.Succeeded()) {
            const Body& body = lock.GetBody();
            JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, hit_point);
            result.normal = Vec3{normal.GetX(), normal.GetY(), normal.GetZ()};
        }

        result.body = impl->find_body_id(hit.mBodyID);
        results.push_back(result);
    }

    return results;
}

std::vector<PhysicsBodyId> overlap_sphere_impl(PhysicsWorld::Impl* impl, const Vec3& center,
                                                float radius, uint16_t layer_mask) {
    std::vector<PhysicsBodyId> results;
    if (!impl || !impl->initialized) return results;

    SphereShape sphere(radius);
    AllHitCollisionCollector<CollideShapeCollector> collector;

    impl->physics_system->GetNarrowPhaseQuery().CollideShape(
        &sphere,
        JPH::Vec3::sReplicate(1.0f),
        RMat44::sTranslation(RVec3(center.x, center.y, center.z)),
        CollideShapeSettings(),
        RVec3::sZero(),
        collector
    );

    std::lock_guard<std::mutex> map_lock(impl->body_map_mutex);
    for (const auto& hit : collector.mHits) {
        PhysicsBodyId body_id = impl->find_body_id(hit.mBodyID2);
        if (body_id.valid()) {
            // Avoid duplicates
            bool found = false;
            for (const auto& existing : results) {
                if (existing.id == body_id.id) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                results.push_back(body_id);
            }
        }
    }

    return results;
}

std::vector<PhysicsBodyId> overlap_box_impl(PhysicsWorld::Impl* impl, const Vec3& center,
                                             const Vec3& half_extents, const Quat& rotation,
                                             uint16_t layer_mask) {
    std::vector<PhysicsBodyId> results;
    if (!impl || !impl->initialized) return results;

    BoxShape box(JPH::Vec3(half_extents.x, half_extents.y, half_extents.z));
    AllHitCollisionCollector<CollideShapeCollector> collector;

    RMat44 transform = RMat44::sRotationTranslation(
        JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w),
        RVec3(center.x, center.y, center.z)
    );

    impl->physics_system->GetNarrowPhaseQuery().CollideShape(
        &box,
        JPH::Vec3::sReplicate(1.0f),
        transform,
        CollideShapeSettings(),
        RVec3::sZero(),
        collector
    );

    std::lock_guard<std::mutex> map_lock(impl->body_map_mutex);
    for (const auto& hit : collector.mHits) {
        PhysicsBodyId body_id = impl->find_body_id(hit.mBodyID2);
        if (body_id.valid()) {
            bool found = false;
            for (const auto& existing : results) {
                if (existing.id == body_id.id) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                results.push_back(body_id);
            }
        }
    }

    return results;
}

void set_collision_callback_impl(PhysicsWorld::Impl* impl, CollisionCallback callback) {
    if (!impl) return;
    impl->collision_callback = std::move(callback);
}

void set_gravity_impl(PhysicsWorld::Impl* impl, const Vec3& gravity) {
    if (!impl || !impl->initialized) return;
    impl->gravity = gravity;
    impl->physics_system->SetGravity(Vec3Arg(gravity.x, gravity.y, gravity.z));
}

Vec3 get_gravity_impl(PhysicsWorld::Impl* impl) {
    if (!impl) return Vec3{0.0f, -9.81f, 0.0f};
    return impl->gravity;
}

uint32_t get_body_count_impl(PhysicsWorld::Impl* impl) {
    if (!impl) return 0;
    std::lock_guard<std::mutex> lock(impl->body_map_mutex);
    return static_cast<uint32_t>(impl->body_map.size());
}

uint32_t get_active_body_count_impl(PhysicsWorld::Impl* impl) {
    if (!impl || !impl->initialized) return 0;
    return impl->physics_system->GetNumActiveBodies(EBodyType::RigidBody);
}

CollisionFilter& get_collision_filter_impl(PhysicsWorld::Impl* impl) {
    return impl->collision_filter;
}

std::vector<PhysicsBodyId> get_all_body_ids_impl(PhysicsWorld::Impl* impl) {
    std::vector<PhysicsBodyId> result;
    if (!impl) return result;

    std::lock_guard<std::mutex> lock(impl->body_map_mutex);
    result.reserve(impl->body_map.size());
    for (const auto& [id, jolt_id] : impl->body_map) {
        result.push_back(PhysicsBodyId{id});
    }
    return result;
}

// ============================================================================
// Motion Type API
// ============================================================================

void set_motion_type_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, BodyType type) {
    if (!impl || !impl->initialized) return;

    BodyID jolt_id;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id = get_jolt_body_id(impl, id);
    }
    if (jolt_id.IsInvalid()) return;

    EMotionType motion_type;
    switch (type) {
        case BodyType::Static:
            motion_type = EMotionType::Static;
            break;
        case BodyType::Kinematic:
            motion_type = EMotionType::Kinematic;
            break;
        case BodyType::Dynamic:
        default:
            motion_type = EMotionType::Dynamic;
            break;
    }

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.SetMotionType(jolt_id, motion_type, EActivation::Activate);
}

BodyType get_motion_type_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id) {
    if (!impl || !impl->initialized) return BodyType::Static;

    BodyID jolt_id;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id = get_jolt_body_id(impl, id);
    }
    if (jolt_id.IsInvalid()) return BodyType::Static;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    EMotionType motion_type = body_interface.GetMotionType(jolt_id);

    switch (motion_type) {
        case EMotionType::Static:
            return BodyType::Static;
        case EMotionType::Kinematic:
            return BodyType::Kinematic;
        case EMotionType::Dynamic:
        default:
            return BodyType::Dynamic;
    }
}

BodyShapeInfo get_body_shape_info_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id) {
    if (!impl) return BodyShapeInfo{};

    std::lock_guard<std::mutex> lock(impl->body_map_mutex);
    auto it = impl->body_shape_info.find(id.id);
    if (it != impl->body_shape_info.end()) {
        return it->second;
    }
    return BodyShapeInfo{};
}

// ============================================================================
// Constraint API
// ============================================================================

ConstraintId create_fixed_constraint_impl(PhysicsWorld::Impl* impl, const FixedConstraintSettings& settings) {
    if (!impl || !impl->initialized) return ConstraintId{};

    BodyID jolt_id_a, jolt_id_b;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id_a = get_jolt_body_id(impl, settings.body_a);
        jolt_id_b = get_jolt_body_id(impl, settings.body_b);
    }
    if (jolt_id_a.IsInvalid() || jolt_id_b.IsInvalid()) {
        log(LogLevel::Warn, "create_fixed_constraint: invalid body IDs");
        return ConstraintId{};
    }

    JPH::FixedConstraintSettings jolt_settings;
    jolt_settings.mAutoDetectPoint = false;
    jolt_settings.mPoint1 = RVec3(settings.local_anchor_a.x, settings.local_anchor_a.y, settings.local_anchor_a.z);
    jolt_settings.mPoint2 = RVec3(settings.local_anchor_b.x, settings.local_anchor_b.y, settings.local_anchor_b.z);

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    BodyLockWrite lock_a(impl->physics_system->GetBodyLockInterface(), jolt_id_a);
    BodyLockWrite lock_b(impl->physics_system->GetBodyLockInterface(), jolt_id_b);

    if (!lock_a.Succeeded() || !lock_b.Succeeded()) {
        log(LogLevel::Warn, "create_fixed_constraint: failed to lock bodies");
        return ConstraintId{};
    }

    Body& body_a = lock_a.GetBody();
    Body& body_b = lock_b.GetBody();

    Ref<Constraint> constraint = jolt_settings.Create(body_a, body_b);
    impl->physics_system->AddConstraint(constraint);

    std::lock_guard<std::mutex> clock(impl->constraint_map_mutex);
    ConstraintId id{impl->next_constraint_id++};
    impl->constraint_map[id.id] = constraint;

    return id;
}

ConstraintId create_hinge_constraint_impl(PhysicsWorld::Impl* impl, const HingeConstraintSettings& settings) {
    if (!impl || !impl->initialized) return ConstraintId{};

    BodyID jolt_id_a, jolt_id_b;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id_a = get_jolt_body_id(impl, settings.body_a);
        jolt_id_b = get_jolt_body_id(impl, settings.body_b);
    }
    if (jolt_id_a.IsInvalid() || jolt_id_b.IsInvalid()) {
        log(LogLevel::Warn, "create_hinge_constraint: invalid body IDs");
        return ConstraintId{};
    }

    JPH::HingeConstraintSettings jolt_settings;
    jolt_settings.mPoint1 = RVec3(settings.local_anchor_a.x, settings.local_anchor_a.y, settings.local_anchor_a.z);
    jolt_settings.mPoint2 = RVec3(settings.local_anchor_b.x, settings.local_anchor_b.y, settings.local_anchor_b.z);
    jolt_settings.mHingeAxis1 = JPH::Vec3(settings.hinge_axis.x, settings.hinge_axis.y, settings.hinge_axis.z);
    jolt_settings.mHingeAxis2 = jolt_settings.mHingeAxis1;
    jolt_settings.mNormalAxis1 = JPH::Vec3(1.0f, 0.0f, 0.0f);
    if (std::abs(settings.hinge_axis.x) > 0.9f) {
        jolt_settings.mNormalAxis1 = JPH::Vec3(0.0f, 1.0f, 0.0f);
    }
    jolt_settings.mNormalAxis2 = jolt_settings.mNormalAxis1;

    if (settings.enable_limits) {
        jolt_settings.mLimitsMin = settings.limit_min;
        jolt_settings.mLimitsMax = settings.limit_max;
    }

    BodyLockWrite lock_a(impl->physics_system->GetBodyLockInterface(), jolt_id_a);
    BodyLockWrite lock_b(impl->physics_system->GetBodyLockInterface(), jolt_id_b);

    if (!lock_a.Succeeded() || !lock_b.Succeeded()) {
        log(LogLevel::Warn, "create_hinge_constraint: failed to lock bodies");
        return ConstraintId{};
    }

    Body& body_a = lock_a.GetBody();
    Body& body_b = lock_b.GetBody();

    Ref<Constraint> constraint = jolt_settings.Create(body_a, body_b);
    impl->physics_system->AddConstraint(constraint);

    std::lock_guard<std::mutex> clock(impl->constraint_map_mutex);
    ConstraintId id{impl->next_constraint_id++};
    impl->constraint_map[id.id] = constraint;

    return id;
}

ConstraintId create_swing_twist_constraint_impl(PhysicsWorld::Impl* impl, const SwingTwistConstraintSettings& settings) {
    if (!impl || !impl->initialized) return ConstraintId{};

    BodyID jolt_id_a, jolt_id_b;
    {
        std::lock_guard<std::mutex> lock(impl->body_map_mutex);
        jolt_id_a = get_jolt_body_id(impl, settings.body_a);
        jolt_id_b = get_jolt_body_id(impl, settings.body_b);
    }
    if (jolt_id_a.IsInvalid() || jolt_id_b.IsInvalid()) {
        log(LogLevel::Warn, "create_swing_twist_constraint: invalid body IDs");
        return ConstraintId{};
    }

    JPH::SwingTwistConstraintSettings jolt_settings;
    jolt_settings.mPosition1 = RVec3(settings.local_anchor_a.x, settings.local_anchor_a.y, settings.local_anchor_a.z);
    jolt_settings.mPosition2 = RVec3(settings.local_anchor_b.x, settings.local_anchor_b.y, settings.local_anchor_b.z);
    jolt_settings.mTwistAxis1 = JPH::Vec3(settings.twist_axis.x, settings.twist_axis.y, settings.twist_axis.z);
    jolt_settings.mTwistAxis2 = jolt_settings.mTwistAxis1;
    jolt_settings.mPlaneAxis1 = JPH::Vec3(settings.plane_axis.x, settings.plane_axis.y, settings.plane_axis.z);
    jolt_settings.mPlaneAxis2 = jolt_settings.mPlaneAxis1;
    jolt_settings.mNormalHalfConeAngle = settings.swing_limit_y;
    jolt_settings.mPlaneHalfConeAngle = settings.swing_limit_z;
    jolt_settings.mTwistMinAngle = settings.twist_min;
    jolt_settings.mTwistMaxAngle = settings.twist_max;

    BodyLockWrite lock_a(impl->physics_system->GetBodyLockInterface(), jolt_id_a);
    BodyLockWrite lock_b(impl->physics_system->GetBodyLockInterface(), jolt_id_b);

    if (!lock_a.Succeeded() || !lock_b.Succeeded()) {
        log(LogLevel::Warn, "create_swing_twist_constraint: failed to lock bodies");
        return ConstraintId{};
    }

    Body& body_a = lock_a.GetBody();
    Body& body_b = lock_b.GetBody();

    Ref<Constraint> constraint = jolt_settings.Create(body_a, body_b);
    impl->physics_system->AddConstraint(constraint);

    std::lock_guard<std::mutex> clock(impl->constraint_map_mutex);
    ConstraintId id{impl->next_constraint_id++};
    impl->constraint_map[id.id] = constraint;

    return id;
}

void destroy_constraint_impl(PhysicsWorld::Impl* impl, ConstraintId id) {
    if (!impl || !impl->initialized) return;

    Ref<Constraint> constraint;
    {
        std::lock_guard<std::mutex> lock(impl->constraint_map_mutex);
        auto it = impl->constraint_map.find(id.id);
        if (it == impl->constraint_map.end()) return;
        constraint = it->second;
        impl->constraint_map.erase(it);
    }

    impl->physics_system->RemoveConstraint(constraint);
}

void set_constraint_motor_state_impl(PhysicsWorld::Impl* impl, ConstraintId id, bool enabled) {
    if (!impl || !impl->initialized) return;

    Ref<Constraint> constraint;
    {
        std::lock_guard<std::mutex> lock(impl->constraint_map_mutex);
        auto it = impl->constraint_map.find(id.id);
        if (it == impl->constraint_map.end()) return;
        constraint = it->second;
    }

    // Try casting to different constraint types and set motor
    // Try casting to different constraint types and set motor
    EConstraintSubType type = constraint->GetSubType();
    if (type == EConstraintSubType::SwingTwist) {
        auto* swing_twist = static_cast<SwingTwistConstraint*>(constraint.GetPtr());
        swing_twist->SetSwingMotorState(enabled ? EMotorState::Position : EMotorState::Off);
        swing_twist->SetTwistMotorState(enabled ? EMotorState::Position : EMotorState::Off);
    } else if (type == EConstraintSubType::Hinge) {
        auto* hinge = static_cast<HingeConstraint*>(constraint.GetPtr());
        hinge->SetMotorState(enabled ? EMotorState::Position : EMotorState::Off);
    }
}

void set_constraint_motor_target_impl(PhysicsWorld::Impl* impl, ConstraintId id, const Quat& target) {
    if (!impl || !impl->initialized) return;

    Ref<Constraint> constraint;
    {
        std::lock_guard<std::mutex> lock(impl->constraint_map_mutex);
        auto it = impl->constraint_map.find(id.id);
        if (it == impl->constraint_map.end()) return;
        constraint = it->second;
    }

    EConstraintSubType type = constraint->GetSubType();
    if (type == EConstraintSubType::SwingTwist) {
        auto* swing_twist = static_cast<SwingTwistConstraint*>(constraint.GetPtr());
        swing_twist->SetTargetOrientationCS(JPH::Quat(target.x, target.y, target.z, target.w));
    } else if (type == EConstraintSubType::Hinge) {
        auto* hinge = static_cast<HingeConstraint*>(constraint.GetPtr());
        // For hinge, extract rotation around hinge axis
        // Simplified: just use the angle component
        float angle = 2.0f * std::acos(std::clamp(target.w, -1.0f, 1.0f));
        hinge->SetTargetAngle(angle);
    }
}

void set_constraint_motor_velocity_impl(PhysicsWorld::Impl* impl, ConstraintId id, const Vec3& angular_velocity) {
    if (!impl || !impl->initialized) return;

    Ref<Constraint> constraint;
    {
        std::lock_guard<std::mutex> lock(impl->constraint_map_mutex);
        auto it = impl->constraint_map.find(id.id);
        if (it == impl->constraint_map.end()) return;
        constraint = it->second;
    }

    EConstraintSubType type = constraint->GetSubType();
    if (type == EConstraintSubType::SwingTwist) {
        auto* swing_twist = static_cast<SwingTwistConstraint*>(constraint.GetPtr());
        swing_twist->SetSwingMotorState(EMotorState::Velocity);
        swing_twist->SetTwistMotorState(EMotorState::Velocity);
        swing_twist->SetTargetAngularVelocityCS(JPH::Vec3(angular_velocity.x, angular_velocity.y, angular_velocity.z));
    } else if (type == EConstraintSubType::Hinge) {
        auto* hinge = static_cast<HingeConstraint*>(constraint.GetPtr());
        hinge->SetMotorState(EMotorState::Velocity);
        hinge->SetTargetAngularVelocity(glm::length(angular_velocity));
    }
}

void set_constraint_motor_strength_impl(PhysicsWorld::Impl* impl, ConstraintId id, float max_force) {
    if (!impl || !impl->initialized) return;

    Ref<Constraint> constraint;
    {
        std::lock_guard<std::mutex> lock(impl->constraint_map_mutex);
        auto it = impl->constraint_map.find(id.id);
        if (it == impl->constraint_map.end()) return;
        constraint = it->second;
    }

    MotorSettings motor_settings(10.0f, 1.0f);
    motor_settings.SetForceLimit(max_force);

    EConstraintSubType type = constraint->GetSubType();
    if (type == EConstraintSubType::SwingTwist) {
        auto* swing_twist = static_cast<SwingTwistConstraint*>(constraint.GetPtr());
        swing_twist->GetSwingMotorSettings() = motor_settings;
        swing_twist->GetTwistMotorSettings() = motor_settings;
    } else if (type == EConstraintSubType::Hinge) {
        auto* hinge = static_cast<HingeConstraint*>(constraint.GetPtr());
        hinge->GetMotorSettings() = motor_settings;
    }
}

// ============================================================================
// Debug/Contact Query API
// ============================================================================

std::vector<ContactPointInfo> get_contact_points_impl(PhysicsWorld::Impl* impl) {
    std::vector<ContactPointInfo> result;
    if (!impl || !impl->initialized) return result;

    // Jolt doesn't provide a simple contact list - we'd need to set up a ContactListener
    // For now, return empty. Full implementation would require adding a ContactListener
    // during init that collects contacts each frame.
    return result;
}

std::vector<ConstraintInfo> get_all_constraints_impl(PhysicsWorld::Impl* impl) {
    std::vector<ConstraintInfo> result;
    if (!impl || !impl->initialized) return result;

    std::lock_guard<std::mutex> clock(impl->constraint_map_mutex);
    std::lock_guard<std::mutex> block(impl->body_map_mutex);

    for (const auto& [cid, constraint] : impl->constraint_map) {
        ConstraintInfo info;
        info.id = ConstraintId{cid};

        // Get body IDs from constraint (if TwoBodyConstraint)
        if (constraint->GetType() == EConstraintType::TwoBodyConstraint) {
            auto* two_body = static_cast<TwoBodyConstraint*>(constraint.GetPtr());
            const Body* body1 = two_body->GetBody1();
            const Body* body2 = two_body->GetBody2();

            if (body1) {
                info.body_a = impl->find_body_id(body1->GetID());
                RVec3 pos = body1->GetPosition();
                info.world_anchor_a = Vec3{static_cast<float>(pos.GetX()),
                                            static_cast<float>(pos.GetY()),
                                            static_cast<float>(pos.GetZ())};
            }
            if (body2) {
                info.body_b = impl->find_body_id(body2->GetID());
                RVec3 pos = body2->GetPosition();
                info.world_anchor_b = Vec3{static_cast<float>(pos.GetX()),
                                            static_cast<float>(pos.GetY()),
                                            static_cast<float>(pos.GetZ())};
            }
        }

        result.push_back(info);
    }

    return result;
}

// ============================================================================
// PhysicsWorld method implementations (forwarding to impl functions)
// ============================================================================

void PhysicsWorld::init(const engine::core::PhysicsSettings& settings) {
    init_physics_impl(m_impl.get(), settings);
}

void PhysicsWorld::shutdown() {
    shutdown_physics_impl(m_impl.get());
}

void PhysicsWorld::step(double dt) {
    step_physics_impl(m_impl.get(), dt);
}

PhysicsBodyId PhysicsWorld::create_body(const BodySettings& settings) {
    return create_body_impl(m_impl.get(), settings);
}

void PhysicsWorld::destroy_body(PhysicsBodyId id) {
    destroy_body_impl(m_impl.get(), id);
}

bool PhysicsWorld::is_valid(PhysicsBodyId id) const {
    return is_valid_impl(m_impl.get(), id);
}

void PhysicsWorld::set_position(PhysicsBodyId id, const Vec3& pos) {
    set_position_impl(m_impl.get(), id, pos);
}

void PhysicsWorld::set_rotation(PhysicsBodyId id, const Quat& rot) {
    set_rotation_impl(m_impl.get(), id, rot);
}

void PhysicsWorld::set_transform(PhysicsBodyId id, const Vec3& pos, const Quat& rot) {
    set_position_impl(m_impl.get(), id, pos);
    set_rotation_impl(m_impl.get(), id, rot);
}

Vec3 PhysicsWorld::get_position(PhysicsBodyId id) const {
    return get_position_impl(m_impl.get(), id);
}

Quat PhysicsWorld::get_rotation(PhysicsBodyId id) const {
    return get_rotation_impl(m_impl.get(), id);
}

void PhysicsWorld::set_linear_velocity(PhysicsBodyId id, const Vec3& vel) {
    set_linear_velocity_impl(m_impl.get(), id, vel);
}

void PhysicsWorld::set_angular_velocity(PhysicsBodyId id, const Vec3& vel) {
    set_angular_velocity_impl(m_impl.get(), id, vel);
}

Vec3 PhysicsWorld::get_linear_velocity(PhysicsBodyId id) const {
    return get_linear_velocity_impl(m_impl.get(), id);
}

Vec3 PhysicsWorld::get_angular_velocity(PhysicsBodyId id) const {
    return get_angular_velocity_impl(m_impl.get(), id);
}

void PhysicsWorld::add_force(PhysicsBodyId id, const Vec3& force) {
    add_force_impl(m_impl.get(), id, force);
}

void PhysicsWorld::add_force_at_point(PhysicsBodyId id, const Vec3& force, const Vec3& point) {
    add_force_at_point_impl(m_impl.get(), id, force, point);
}

void PhysicsWorld::add_torque(PhysicsBodyId id, const Vec3& torque) {
    add_torque_impl(m_impl.get(), id, torque);
}

void PhysicsWorld::add_impulse(PhysicsBodyId id, const Vec3& impulse) {
    add_impulse_impl(m_impl.get(), id, impulse);
}

void PhysicsWorld::add_impulse_at_point(PhysicsBodyId id, const Vec3& impulse, const Vec3& point) {
    add_impulse_at_point_impl(m_impl.get(), id, impulse, point);
}

void PhysicsWorld::set_gravity_factor(PhysicsBodyId id, float factor) {
    set_gravity_factor_impl(m_impl.get(), id, factor);
}

void PhysicsWorld::set_friction(PhysicsBodyId id, float friction) {
    set_friction_impl(m_impl.get(), id, friction);
}

void PhysicsWorld::set_restitution(PhysicsBodyId id, float restitution) {
    set_restitution_impl(m_impl.get(), id, restitution);
}

void PhysicsWorld::activate_body(PhysicsBodyId id) {
    activate_body_impl(m_impl.get(), id);
}

bool PhysicsWorld::is_active(PhysicsBodyId id) const {
    return is_active_impl(m_impl.get(), id);
}

void PhysicsWorld::set_motion_type(PhysicsBodyId id, BodyType type) {
    set_motion_type_impl(m_impl.get(), id, type);
}

BodyType PhysicsWorld::get_motion_type(PhysicsBodyId id) const {
    return get_motion_type_impl(m_impl.get(), id);
}

BodyShapeInfo PhysicsWorld::get_body_shape_info(PhysicsBodyId id) const {
    return get_body_shape_info_impl(m_impl.get(), id);
}

BodyType PhysicsWorld::get_body_type(PhysicsBodyId id) const {
    return get_motion_type_impl(m_impl.get(), id);
}

RaycastHit PhysicsWorld::raycast(const Vec3& origin, const Vec3& direction, float max_distance,
                                  uint16_t layer_mask) const {
    return raycast_impl(m_impl.get(), origin, direction, max_distance, layer_mask);
}

std::vector<RaycastHit> PhysicsWorld::raycast_all(const Vec3& origin, const Vec3& direction,
                                                   float max_distance, uint16_t layer_mask) const {
    return raycast_all_impl(m_impl.get(), origin, direction, max_distance, layer_mask);
}

std::vector<PhysicsBodyId> PhysicsWorld::overlap_sphere(const Vec3& center, float radius,
                                                         uint16_t layer_mask) const {
    return overlap_sphere_impl(m_impl.get(), center, radius, layer_mask);
}

std::vector<PhysicsBodyId> PhysicsWorld::overlap_box(const Vec3& center, const Vec3& half_extents,
                                                      const Quat& rotation, uint16_t layer_mask) const {
    return overlap_box_impl(m_impl.get(), center, half_extents, rotation, layer_mask);
}

void PhysicsWorld::set_collision_callback(CollisionCallback callback) {
    set_collision_callback_impl(m_impl.get(), std::move(callback));
}

CollisionFilter& PhysicsWorld::get_collision_filter() {
    return get_collision_filter_impl(m_impl.get());
}

const CollisionFilter& PhysicsWorld::get_collision_filter() const {
    return const_cast<PhysicsWorld*>(this)->get_collision_filter();
}

ConstraintId PhysicsWorld::create_fixed_constraint(const FixedConstraintSettings& settings) {
    return create_fixed_constraint_impl(m_impl.get(), settings);
}

ConstraintId PhysicsWorld::create_hinge_constraint(const HingeConstraintSettings& settings) {
    return create_hinge_constraint_impl(m_impl.get(), settings);
}

ConstraintId PhysicsWorld::create_swing_twist_constraint(const SwingTwistConstraintSettings& settings) {
    return create_swing_twist_constraint_impl(m_impl.get(), settings);
}

void PhysicsWorld::destroy_constraint(ConstraintId id) {
    destroy_constraint_impl(m_impl.get(), id);
}

void PhysicsWorld::set_constraint_motor_state(ConstraintId id, bool enabled) {
    set_constraint_motor_state_impl(m_impl.get(), id, enabled);
}

void PhysicsWorld::set_constraint_motor_target(ConstraintId id, const Quat& target_rotation) {
    set_constraint_motor_target_impl(m_impl.get(), id, target_rotation);
}

void PhysicsWorld::set_constraint_motor_velocity(ConstraintId id, const Vec3& angular_velocity) {
    set_constraint_motor_velocity_impl(m_impl.get(), id, angular_velocity);
}

void PhysicsWorld::set_constraint_motor_strength(ConstraintId id, float max_force_limit) {
    set_constraint_motor_strength_impl(m_impl.get(), id, max_force_limit);
}

std::vector<ContactPointInfo> PhysicsWorld::get_contact_points() const {
    return get_contact_points_impl(m_impl.get());
}

std::vector<ConstraintInfo> PhysicsWorld::get_all_constraints() const {
    return get_all_constraints_impl(m_impl.get());
}

void PhysicsWorld::set_gravity(const Vec3& gravity) {
    set_gravity_impl(m_impl.get(), gravity);
}

Vec3 PhysicsWorld::get_gravity() const {
    return get_gravity_impl(m_impl.get());
}

uint32_t PhysicsWorld::get_body_count() const {
    return get_body_count_impl(m_impl.get());
}

uint32_t PhysicsWorld::get_active_body_count() const {
    return get_active_body_count_impl(m_impl.get());
}

std::vector<PhysicsBodyId> PhysicsWorld::get_all_body_ids() const {
    return get_all_body_ids_impl(m_impl.get());
}

} // namespace engine::physics
