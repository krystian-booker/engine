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
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

#include <unordered_map>

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
    bool ShouldCollide(ObjectLayer obj1, ObjectLayer obj2) const override {
        // All layers collide with each other for now
        return true;
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
    Vec3 gravity{0.0f, -9.81f, 0.0f};

    std::unordered_map<uint32_t, BodyID> body_map;
    uint32_t next_body_id = 1;

    bool initialized = false;
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
            default:
                shape = new BoxShape(Vec3Arg(0.5f, 0.5f, 0.5f));
                break;
        }
    } else {
        shape = new BoxShape(Vec3Arg(0.5f, 0.5f, 0.5f));
    }

    // Determine motion type
    EMotionType motion_type;
    ObjectLayer object_layer;
    switch (settings.type) {
        case BodyType::Static:
            motion_type = EMotionType::Static;
            object_layer = 0;
            break;
        case BodyType::Kinematic:
            motion_type = EMotionType::Kinematic;
            object_layer = 1;
            break;
        case BodyType::Dynamic:
        default:
            motion_type = EMotionType::Dynamic;
            object_layer = 1;
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

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    Body* body = body_interface.CreateBody(body_settings);

    if (!body) {
        return PhysicsBodyId{};
    }

    body_interface.AddBody(body->GetID(), EActivation::Activate);

    PhysicsBodyId id{impl->next_body_id++};
    impl->body_map[id.id] = body->GetID();

    return id;
}

void destroy_body_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id) {
    if (!impl || !impl->initialized) return;

    auto it = impl->body_map.find(id.id);
    if (it == impl->body_map.end()) return;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.RemoveBody(it->second);
    body_interface.DestroyBody(it->second);

    impl->body_map.erase(it);
}

bool is_valid_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id) {
    if (!impl) return false;
    return impl->body_map.find(id.id) != impl->body_map.end();
}

void set_position_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Vec3& pos) {
    if (!impl || !impl->initialized) return;
    auto it = impl->body_map.find(id.id);
    if (it == impl->body_map.end()) return;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.SetPosition(it->second, RVec3Arg(pos.x, pos.y, pos.z), EActivation::Activate);
}

void set_rotation_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Quat& rot) {
    if (!impl || !impl->initialized) return;
    auto it = impl->body_map.find(id.id);
    if (it == impl->body_map.end()) return;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.SetRotation(it->second, QuatArg(rot.x, rot.y, rot.z, rot.w), EActivation::Activate);
}

Vec3 get_position_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id) {
    if (!impl || !impl->initialized) return Vec3{0.0f};
    auto it = impl->body_map.find(id.id);
    if (it == impl->body_map.end()) return Vec3{0.0f};

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    RVec3 pos = body_interface.GetPosition(it->second);
    return Vec3{static_cast<float>(pos.GetX()), static_cast<float>(pos.GetY()), static_cast<float>(pos.GetZ())};
}

Quat get_rotation_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id) {
    if (!impl || !impl->initialized) return Quat{1.0f, 0.0f, 0.0f, 0.0f};
    auto it = impl->body_map.find(id.id);
    if (it == impl->body_map.end()) return Quat{1.0f, 0.0f, 0.0f, 0.0f};

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    JPH::Quat rot = body_interface.GetRotation(it->second);
    return Quat{rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ()};
}

void set_linear_velocity_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Vec3& vel) {
    if (!impl || !impl->initialized) return;
    auto it = impl->body_map.find(id.id);
    if (it == impl->body_map.end()) return;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.SetLinearVelocity(it->second, Vec3Arg(vel.x, vel.y, vel.z));
}

void set_angular_velocity_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Vec3& vel) {
    if (!impl || !impl->initialized) return;
    auto it = impl->body_map.find(id.id);
    if (it == impl->body_map.end()) return;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.SetAngularVelocity(it->second, Vec3Arg(vel.x, vel.y, vel.z));
}

Vec3 get_linear_velocity_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id) {
    if (!impl || !impl->initialized) return Vec3{0.0f};
    auto it = impl->body_map.find(id.id);
    if (it == impl->body_map.end()) return Vec3{0.0f};

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    JPH::Vec3 vel = body_interface.GetLinearVelocity(it->second);
    return Vec3{vel.GetX(), vel.GetY(), vel.GetZ()};
}

Vec3 get_angular_velocity_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id) {
    if (!impl || !impl->initialized) return Vec3{0.0f};
    auto it = impl->body_map.find(id.id);
    if (it == impl->body_map.end()) return Vec3{0.0f};

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    JPH::Vec3 vel = body_interface.GetAngularVelocity(it->second);
    return Vec3{vel.GetX(), vel.GetY(), vel.GetZ()};
}

void add_force_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Vec3& force) {
    if (!impl || !impl->initialized) return;
    auto it = impl->body_map.find(id.id);
    if (it == impl->body_map.end()) return;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.AddForce(it->second, Vec3Arg(force.x, force.y, force.z));
}

void add_impulse_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Vec3& impulse) {
    if (!impl || !impl->initialized) return;
    auto it = impl->body_map.find(id.id);
    if (it == impl->body_map.end()) return;

    BodyInterface& body_interface = impl->physics_system->GetBodyInterface();
    body_interface.AddImpulse(it->second, Vec3Arg(impulse.x, impulse.y, impulse.z));
}

RaycastHit raycast_impl(PhysicsWorld::Impl* impl, const Vec3& origin, const Vec3& dir,
                        float max_dist, uint16_t /*mask*/) {
    RaycastHit result;
    if (!impl || !impl->initialized) return result;

    // TODO: Implement proper raycast with Jolt
    (void)origin;
    (void)dir;
    (void)max_dist;

    return result;
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
    return static_cast<uint32_t>(impl->body_map.size());
}

CollisionFilter& get_collision_filter_impl(PhysicsWorld::Impl* impl) {
    return impl->collision_filter;
}

} // namespace engine::physics
