#include <engine/physics/character_controller.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/core/log.hpp>

// Jolt includes for CharacterVirtual
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Core/TempAllocator.h>

#include <algorithm>

namespace engine::physics {

using namespace JPH;

// Helper to convert engine Vec3 to Jolt Vec3
static JPH::Vec3 to_jolt(const Vec3& v) {
    return JPH::Vec3(v.x, v.y, v.z);
}

// Helper to convert Jolt Vec3 to engine Vec3
static Vec3 to_engine(const JPH::Vec3& v) {
    return Vec3{v.GetX(), v.GetY(), v.GetZ()};
}

#ifdef JPH_DOUBLE_PRECISION
static Vec3 to_engine(const JPH::RVec3& v) {
    return Vec3{
        static_cast<float>(v.GetX()),
        static_cast<float>(v.GetY()),
        static_cast<float>(v.GetZ())
    };
}
#endif

// Helper to convert engine Quat to Jolt Quat
static JPH::Quat to_jolt(const core::Quat& q) {
    return JPH::Quat(q.x, q.y, q.z, q.w);
}

// Helper to convert Jolt Quat to engine Quat
static core::Quat to_engine(const JPH::Quat& q) {
    return core::Quat{q.GetW(), q.GetX(), q.GetY(), q.GetZ()};
}

// Broad phase layer filter that allows all layers
class CharacterBroadPhaseLayerFilter : public BroadPhaseLayerFilter {
public:
    bool ShouldCollide(BroadPhaseLayer) const override { return true; }
};

// Object layer filter based on layer mask
class CharacterObjectLayerFilter : public ObjectLayerFilter {
public:
    explicit CharacterObjectLayerFilter(uint16_t mask) : m_mask(mask) {}

    bool ShouldCollide(ObjectLayer layer) const override {
        if (m_mask == 0) return false;
        const uint32_t layer_index = static_cast<uint32_t>(layer);
        if (layer_index >= 16) return false;
        return (m_mask & (1u << layer_index)) != 0;
    }

private:
    uint16_t m_mask;
};

// Body filter that ignores specific bodies
class CharacterBodyFilter : public BodyFilter {
public:
    bool ShouldCollide(const BodyID&) const override { return true; }
    bool ShouldCollideLocked(const Body&) const override { return true; }
};

// Shape filter
class CharacterShapeFilter : public ShapeFilter {
public:
    bool ShouldCollide(const Shape*, const SubShapeID&) const override { return true; }
};

// Character contact listener for handling contacts
class EngineCharacterContactListener : public JPH::CharacterContactListener {
public:
    void OnAdjustBodyVelocity(const CharacterVirtual*, const Body&, JPH::Vec3&, JPH::Vec3&) override {}

    bool OnContactValidate(const CharacterVirtual*, const BodyID&, const SubShapeID&) override {
        return true;
    }

    void OnContactAdded(const CharacterVirtual*, const BodyID&, const SubShapeID&,
                        RVec3Arg, JPH::Vec3Arg, CharacterContactSettings&) override {}

    void OnContactSolve(const CharacterVirtual*, const BodyID&, const SubShapeID&,
                        RVec3Arg, JPH::Vec3Arg, JPH::Vec3Arg, const PhysicsMaterial*,
                        JPH::Vec3Arg, JPH::Vec3&) override {}
};

struct CharacterController::Impl {
    Ref<CharacterVirtual> character;
    Ref<Shape> shape;
    EngineCharacterContactListener contact_listener;

    // Cached physics system pointer
    PhysicsSystem* physics_system = nullptr;
    TempAllocator* temp_allocator = nullptr;
};

CharacterController::CharacterController()
    : m_impl(std::make_unique<Impl>())
{
}

CharacterController::~CharacterController() {
    shutdown();
}

CharacterController::CharacterController(CharacterController&&) noexcept = default;
CharacterController& CharacterController::operator=(CharacterController&&) noexcept = default;

void CharacterController::init(PhysicsWorld& world, const CharacterSettings& settings) {
    if (m_initialized) {
        shutdown();
    }

    m_world = &world;
    m_settings = settings;
    m_position = settings.position;
    m_rotation = settings.rotation;

    // Get Jolt physics system
    m_impl->physics_system = static_cast<PhysicsSystem*>(world.get_jolt_system());
    m_impl->temp_allocator = static_cast<TempAllocator*>(world.get_temp_allocator());

    if (!m_impl->physics_system || !m_impl->temp_allocator) {
        core::log(core::LogLevel::Error, "CharacterController::init failed: PhysicsWorld not initialized");
        return;
    }

    // Create capsule shape for character
    // Jolt capsule is oriented along Y axis, total height = 2*half_height + 2*radius
    float half_height = (settings.height - 2.0f * settings.radius) * 0.5f;
    if (half_height < 0.01f) {
        half_height = 0.01f;
    }

    // Create a capsule and offset it so the bottom is at y=0
    Ref<CapsuleShape> capsule = new CapsuleShape(half_height, settings.radius);

    // Offset the shape so character origin is at feet
    float center_offset_y = half_height + settings.radius;
    RotatedTranslatedShapeSettings offset_settings(
        JPH::Vec3(0.0f, center_offset_y, 0.0f),
        JPH::Quat::sIdentity(),
        capsule
    );
    auto result = offset_settings.Create();
    if (!result.IsValid()) {
        core::log(core::LogLevel::Error, "CharacterController::init failed: Could not create character shape");
        return;
    }
    m_impl->shape = result.Get();

    // Create CharacterVirtual settings
    CharacterVirtualSettings char_settings;
    char_settings.mMaxSlopeAngle = glm::radians(settings.max_slope_angle);
    char_settings.mMaxStrength = settings.push_force;
    char_settings.mShape = m_impl->shape;
    char_settings.mMass = settings.mass;
    char_settings.mPredictiveContactDistance = settings.skin_width;
    char_settings.mPenetrationRecoverySpeed = 1.0f;
    char_settings.mCharacterPadding = 0.02f;
    char_settings.mUp = JPH::Vec3(0.0f, 1.0f, 0.0f);
    char_settings.mSupportingVolume = Plane(JPH::Vec3(0.0f, 1.0f, 0.0f), -settings.radius);

    // Create the character
    m_impl->character = new CharacterVirtual(
        &char_settings,
        RVec3(settings.position.x, settings.position.y, settings.position.z),
        to_jolt(settings.rotation),
        0,  // User data
        m_impl->physics_system
    );

    m_impl->character->SetListener(&m_impl->contact_listener);

    m_initialized = true;

    core::log(core::LogLevel::Debug, "CharacterController initialized at ({}, {}, {})",
              m_position.x, m_position.y, m_position.z);
}

void CharacterController::shutdown() {
    if (!m_initialized) return;

    m_impl->character = nullptr;
    m_impl->shape = nullptr;
    m_impl->physics_system = nullptr;
    m_impl->temp_allocator = nullptr;
    m_world = nullptr;
    m_initialized = false;
}

bool CharacterController::is_initialized() const {
    return m_initialized;
}

void CharacterController::set_position(const Vec3& pos) {
    m_position = pos;
    if (m_impl->character) {
        m_impl->character->SetPosition(RVec3(pos.x, pos.y, pos.z));
    }
}

Vec3 CharacterController::get_position() const {
    if (m_impl->character) {
        return to_engine(m_impl->character->GetPosition());
    }
    return m_position;
}

void CharacterController::set_rotation(const Quat& rot) {
    m_rotation = rot;
    if (m_impl->character) {
        m_impl->character->SetRotation(to_jolt(rot));
    }
}

Quat CharacterController::get_rotation() const {
    if (m_impl->character) {
        return to_engine(m_impl->character->GetRotation());
    }
    return m_rotation;
}

void CharacterController::set_movement_input(const Vec3& direction) {
    m_movement_input = direction;
    // Clamp magnitude to 1
    float len = glm::length(m_movement_input);
    if (len > 1.0f) {
        m_movement_input /= len;
    }
}

void CharacterController::set_movement_input(float x, float z) {
    set_movement_input(Vec3{x, 0.0f, z});
}

void CharacterController::jump(float impulse) {
    m_jump_requested = true;
    m_jump_impulse = impulse;
    m_time_since_jump_pressed = 0.0f;
}

bool CharacterController::can_jump() const {
    if (!m_enabled) return false;

    // Can jump if grounded or within coyote time
    if (m_ground_state.on_ground) return true;
    if (m_ground_state.time_since_grounded < m_coyote_time && !m_has_jumped) return true;

    return false;
}

void CharacterController::update(float dt) {
    if (!m_initialized || !m_enabled || !m_world || !m_impl->character) return;

    // Update ground state from CharacterVirtual
    update_ground_state(dt);

    // Handle jump buffering
    m_time_since_jump_pressed += dt;

    // Check for buffered jump
    if (m_time_since_jump_pressed < m_jump_buffer_time && can_jump()) {
        m_jump_requested = true;
    }

    // Execute jump if requested and allowed
    if (m_jump_requested && can_jump()) {
        m_velocity.y = m_jump_impulse;
        m_has_jumped = true;
        m_jump_requested = false;
        m_ground_state.on_ground = false;
    }
    m_jump_requested = false;

    // Apply movement
    apply_movement(dt);

    // Apply gravity
    apply_gravity(dt);

    // Calculate desired velocity for the character
    JPH::Vec3 desired_velocity = to_jolt(m_velocity);

    // Handle moving platforms - add ground velocity
    if (m_ground_state.on_ground && m_ground_state.ground_body.valid()) {
        desired_velocity += to_jolt(m_ground_state.ground_velocity);
    }

    // Set up filters for collision
    CharacterBroadPhaseLayerFilter broad_phase_filter;
    CharacterObjectLayerFilter object_filter(m_settings.collide_with);
    CharacterBodyFilter body_filter;
    CharacterShapeFilter shape_filter;

    // Use ExtendedUpdate for proper stair stepping and collision
    CharacterVirtual::ExtendedUpdateSettings update_settings;
    update_settings.mStickToFloorStepDown = JPH::Vec3(0.0f, -m_settings.step_height, 0.0f);
    update_settings.mWalkStairsStepUp = JPH::Vec3(0.0f, m_settings.step_height, 0.0f);
    update_settings.mWalkStairsMinStepForward = 0.02f;
    update_settings.mWalkStairsStepForwardTest = 0.15f;
    update_settings.mWalkStairsCosAngleForwardContact = glm::cos(glm::radians(75.0f));
    update_settings.mWalkStairsStepDownExtra = JPH::Vec3::sZero();

    m_impl->character->ExtendedUpdate(
        dt,
        to_jolt(m_world->get_gravity() * m_gravity_scale),
        update_settings,
        broad_phase_filter,
        object_filter,
        body_filter,
        shape_filter,
        *m_impl->temp_allocator
    );

    // Update velocity based on character's actual movement
    m_impl->character->SetLinearVelocity(desired_velocity);

    // Get updated position from character
    m_position = to_engine(m_impl->character->GetPosition());

    // Snap vertical velocity if grounded and moving down
    if (m_ground_state.on_ground && m_velocity.y < 0.0f) {
        m_velocity.y = 0.0f;
    }

    // Reset jump state when landing
    if (m_ground_state.on_ground && !m_ground_state.was_on_ground) {
        m_has_jumped = false;
    }
}

const GroundState& CharacterController::get_ground_state() const {
    return m_ground_state;
}

bool CharacterController::is_grounded() const {
    return m_ground_state.on_ground;
}

Vec3 CharacterController::get_velocity() const {
    return m_velocity;
}

Vec3 CharacterController::get_linear_velocity() const {
    return m_velocity;
}

void CharacterController::set_velocity(const Vec3& vel) {
    m_velocity = vel;
}

void CharacterController::add_velocity(const Vec3& vel) {
    m_velocity += vel;
}

void CharacterController::set_movement_speed(float speed) {
    m_movement_speed = speed;
}

float CharacterController::get_movement_speed() const {
    return m_movement_speed;
}

void CharacterController::set_jump_impulse(float impulse) {
    m_jump_impulse = impulse;
}

float CharacterController::get_jump_impulse() const {
    return m_jump_impulse;
}

void CharacterController::set_gravity_scale(float scale) {
    m_gravity_scale = scale;
}

float CharacterController::get_gravity_scale() const {
    return m_gravity_scale;
}

void CharacterController::set_air_control(float control) {
    m_air_control = glm::clamp(control, 0.0f, 1.0f);
}

float CharacterController::get_air_control() const {
    return m_air_control;
}

void CharacterController::set_friction(float friction) {
    m_friction = friction;
}

float CharacterController::get_friction() const {
    return m_friction;
}

void CharacterController::set_air_friction(float friction) {
    m_air_friction = friction;
}

float CharacterController::get_air_friction() const {
    return m_air_friction;
}

void CharacterController::set_acceleration(float accel) {
    m_acceleration = accel;
}

float CharacterController::get_acceleration() const {
    return m_acceleration;
}

void CharacterController::set_deceleration(float decel) {
    m_deceleration = decel;
}

float CharacterController::get_deceleration() const {
    return m_deceleration;
}

void CharacterController::set_enabled(bool enabled) {
    m_enabled = enabled;
}

bool CharacterController::is_enabled() const {
    return m_enabled;
}

void CharacterController::teleport(const Vec3& position, const Quat& rotation) {
    m_position = position;
    m_rotation = rotation;
    m_velocity = Vec3{0.0f};

    if (m_impl->character) {
        m_impl->character->SetPosition(RVec3(position.x, position.y, position.z));
        m_impl->character->SetRotation(to_jolt(rotation));
        m_impl->character->SetLinearVelocity(JPH::Vec3::sZero());
    }

    refresh_ground_state();
}

void CharacterController::refresh_ground_state() {
    update_ground_state(0.0f);
}

void CharacterController::update_ground_state(float dt) {
    if (!m_impl->character) return;

    m_ground_state.was_on_ground = m_ground_state.on_ground;

    // Get ground state from CharacterVirtual
    CharacterVirtual::EGroundState jolt_ground_state = m_impl->character->GetGroundState();

    switch (jolt_ground_state) {
        case CharacterVirtual::EGroundState::OnGround:
            m_ground_state.on_ground = true;
            m_ground_state.sliding = false;
            m_ground_state.time_since_grounded = 0.0f;
            break;

        case CharacterVirtual::EGroundState::OnSteepGround:
            m_ground_state.on_ground = false;
            m_ground_state.on_slope = true;
            m_ground_state.sliding = true;
            break;

        case CharacterVirtual::EGroundState::NotSupported:
        case CharacterVirtual::EGroundState::InAir:
        default:
            m_ground_state.on_ground = false;
            m_ground_state.on_slope = false;
            m_ground_state.sliding = false;
            break;
    }

    // Get ground normal and velocity
    if (jolt_ground_state != CharacterVirtual::EGroundState::InAir) {
        m_ground_state.ground_normal = to_engine(m_impl->character->GetGroundNormal());
        m_ground_state.ground_velocity = to_engine(m_impl->character->GetGroundVelocity());
        m_ground_state.ground_point = to_engine(m_impl->character->GetGroundPosition());

        // Calculate slope angle
        float dot = glm::dot(m_ground_state.ground_normal, Vec3{0.0f, 1.0f, 0.0f});
        m_ground_state.slope_angle = std::acos(glm::clamp(dot, -1.0f, 1.0f));
        m_ground_state.on_slope = m_ground_state.slope_angle > 0.01f;

        // Map ground body ID - we'd need to look it up from Jolt's BodyID
        // For now, just use an invalid ID unless we implement the mapping
        m_ground_state.ground_body = PhysicsBodyId{};
    } else {
        m_ground_state.ground_normal = Vec3{0.0f, 1.0f, 0.0f};
        m_ground_state.ground_velocity = Vec3{0.0f};
        m_ground_state.slope_angle = 0.0f;
    }

    // Update time since grounded
    if (!m_ground_state.on_ground) {
        m_ground_state.time_since_grounded += dt;
    }
}

void CharacterController::apply_movement(float dt) {
    // Transform input to world space based on rotation
    Vec3 forward = m_rotation * Vec3{0.0f, 0.0f, -1.0f};
    Vec3 right = m_rotation * Vec3{1.0f, 0.0f, 0.0f};

    // Project to horizontal plane
    forward.y = 0.0f;
    right.y = 0.0f;
    if (glm::length(forward) > 0.001f) forward = glm::normalize(forward);
    if (glm::length(right) > 0.001f) right = glm::normalize(right);

    // Calculate desired velocity
    Vec3 desired_velocity = (right * m_movement_input.x + forward * m_movement_input.z) * m_movement_speed;

    // Get current horizontal velocity
    Vec3 current_horizontal{m_velocity.x, 0.0f, m_velocity.z};

    float control = m_ground_state.on_ground ? 1.0f : m_air_control;

    // Apply friction/deceleration when no input
    if (glm::length(m_movement_input) < 0.01f) {
        float decel = m_deceleration * dt * control;
        float speed = glm::length(current_horizontal);
        if (speed > 0.0f) {
            float new_speed = std::max(0.0f, speed - decel);
            if (speed > 0.001f) {
                current_horizontal = glm::normalize(current_horizontal) * new_speed;
            } else {
                current_horizontal = Vec3{0.0f};
            }
        }
    } else {
        // Accelerate towards desired velocity
        Vec3 velocity_diff = desired_velocity - current_horizontal;
        float accel = m_acceleration * dt * control;

        if (glm::length(velocity_diff) > accel) {
            velocity_diff = glm::normalize(velocity_diff) * accel;
        }

        current_horizontal += velocity_diff;
    }

    // Apply friction when grounded and no input
    if (m_ground_state.on_ground && glm::length(m_movement_input) < 0.01f) {
        float friction_force = m_friction * dt;
        float horizontal_speed = glm::length(current_horizontal);
        if (horizontal_speed > friction_force) {
            current_horizontal -= glm::normalize(current_horizontal) * friction_force;
        } else {
            current_horizontal = Vec3{0.0f};
        }
    }

    // Update velocity
    m_velocity.x = current_horizontal.x;
    m_velocity.z = current_horizontal.z;

    // Handle sliding on steep slopes
    if (m_ground_state.sliding && m_world) {
        Vec3 slide_dir = m_ground_state.ground_normal;
        slide_dir.y = 0.0f;
        if (glm::length(slide_dir) > 0.01f) {
            slide_dir = glm::normalize(slide_dir);
            Vec3 gravity = m_world->get_gravity();
            float gravity_magnitude = std::abs(gravity.y);
            m_velocity += slide_dir * gravity_magnitude * dt;
        }
    }
}

void CharacterController::apply_gravity(float dt) {
    if (!m_world) return;

    if (!m_ground_state.on_ground) {
        Vec3 gravity = m_world->get_gravity() * m_gravity_scale;
        m_velocity += gravity * dt;
    } else if (m_velocity.y < 0.0f) {
        // Snap to ground
        m_velocity.y = 0.0f;
    }
}

void CharacterController::handle_step_up() {
    // Step-up is handled by CharacterVirtual::ExtendedUpdate
    // This function is kept for API compatibility but no longer needed
}

// System function
void character_controller_system(scene::World& world, PhysicsWorld& physics, float dt) {
    auto view = world.view<CharacterControllerComponent, scene::LocalTransform>();

    for (auto entity : view) {
        auto& cc = view.get<CharacterControllerComponent>(entity);
        auto& transform = view.get<scene::LocalTransform>(entity);

        if (!cc.controller) continue;
        if (!cc.controller->is_initialized()) continue;
        if (!cc.controller->is_enabled()) continue;

        // Update the controller
        cc.controller->update(dt);

        // Sync position and rotation back to transform component
        transform.position = cc.controller->get_position();
        transform.rotation = cc.controller->get_rotation();
    }
}

} // namespace engine::physics
