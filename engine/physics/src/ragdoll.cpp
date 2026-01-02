#include <engine/physics/ragdoll.hpp>
#include <engine/core/log.hpp>
#include <engine/core/filesystem.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <functional>

namespace engine::physics {

using json = nlohmann::json;

// ============================================================================
// RagdollDefinition
// ============================================================================

RagdollDefinition RagdollDefinition::load(const std::string& path) {
    RagdollDefinition def;

    auto content = core::FileSystem::read_text(path);
    if (content.empty()) {
        core::log(core::LogLevel::Error, "Failed to load ragdoll definition: {}", path);
        return def;
    }

    try {
        json j = json::parse(content);

        def.name = j.value("name", "");
        def.root_body = j.value("root_body", "");

        // Parse bodies
        if (j.contains("bodies") && j["bodies"].is_array()) {
            for (const auto& body_json : j["bodies"]) {
                RagdollBodyDef body;
                body.bone_name = body_json.value("bone", "");
                body.mass = body_json.value("mass", 5.0f);
                body.friction = body_json.value("friction", 0.5f);
                body.restitution = body_json.value("restitution", 0.0f);

                std::string shape = body_json.value("shape", "capsule");
                if (shape == "capsule") body.shape = RagdollShapeType::Capsule;
                else if (shape == "box") body.shape = RagdollShapeType::Box;
                else if (shape == "sphere") body.shape = RagdollShapeType::Sphere;

                if (body_json.contains("dimensions")) {
                    auto& d = body_json["dimensions"];
                    body.dimensions = Vec3{d[0], d[1], d[2]};
                }

                if (body_json.contains("offset")) {
                    auto& o = body_json["offset"];
                    body.offset = Vec3{o[0], o[1], o[2]};
                }

                def.bodies.push_back(body);
            }
        }

        // Parse joints
        if (j.contains("joints") && j["joints"].is_array()) {
            for (const auto& joint_json : j["joints"]) {
                RagdollJointDef joint;
                joint.body_a = joint_json.value("body_a", "");
                joint.body_b = joint_json.value("body_b", "");
                joint.twist_min = joint_json.value("twist_min", -0.5f);
                joint.twist_max = joint_json.value("twist_max", 0.5f);
                joint.swing_limit_1 = joint_json.value("swing_limit_1", 0.5f);
                joint.swing_limit_2 = joint_json.value("swing_limit_2", 0.5f);

                std::string type = joint_json.value("type", "cone");
                if (type == "fixed") joint.type = RagdollJointType::Fixed;
                else if (type == "hinge") joint.type = RagdollJointType::Hinge;
                else if (type == "cone") joint.type = RagdollJointType::Cone;
                else if (type == "twist") joint.type = RagdollJointType::Twist;

                def.joints.push_back(joint);
            }
        }

    } catch (const std::exception& e) {
        core::log(core::LogLevel::Error, "Failed to parse ragdoll definition: {}", e.what());
    }

    return def;
}

bool RagdollDefinition::save(const std::string& path) const {
    json j;
    j["name"] = name;
    j["root_body"] = root_body;

    j["bodies"] = json::array();
    for (const auto& body : bodies) {
        json body_json;
        body_json["bone"] = body.bone_name;
        body_json["mass"] = body.mass;
        body_json["friction"] = body.friction;
        body_json["restitution"] = body.restitution;

        switch (body.shape) {
            case RagdollShapeType::Capsule: body_json["shape"] = "capsule"; break;
            case RagdollShapeType::Box: body_json["shape"] = "box"; break;
            case RagdollShapeType::Sphere: body_json["shape"] = "sphere"; break;
        }

        body_json["dimensions"] = {body.dimensions.x, body.dimensions.y, body.dimensions.z};
        body_json["offset"] = {body.offset.x, body.offset.y, body.offset.z};

        j["bodies"].push_back(body_json);
    }

    j["joints"] = json::array();
    for (const auto& joint : joints) {
        json joint_json;
        joint_json["body_a"] = joint.body_a;
        joint_json["body_b"] = joint.body_b;
        joint_json["twist_min"] = joint.twist_min;
        joint_json["twist_max"] = joint.twist_max;
        joint_json["swing_limit_1"] = joint.swing_limit_1;
        joint_json["swing_limit_2"] = joint.swing_limit_2;

        switch (joint.type) {
            case RagdollJointType::Fixed: joint_json["type"] = "fixed"; break;
            case RagdollJointType::Hinge: joint_json["type"] = "hinge"; break;
            case RagdollJointType::Cone: joint_json["type"] = "cone"; break;
            case RagdollJointType::Twist: joint_json["type"] = "twist"; break;
        }

        j["joints"].push_back(joint_json);
    }

    return core::FileSystem::write_text(path, j.dump(2));
}

RagdollDefinition RagdollDefinition::generate_from_skeleton(const render::Skeleton& skeleton) {
    RagdollDefinition def;
    def.name = "auto_generated";

    // This would analyze the skeleton and create appropriate bodies and joints
    // For now, return a basic humanoid template

    // Common humanoid bones
    const std::vector<std::pair<std::string, Vec3>> humanoid_bones = {
        {"pelvis", {0.15f, 0.1f, 0.1f}},
        {"spine", {0.12f, 0.15f, 0.08f}},
        {"spine1", {0.12f, 0.15f, 0.08f}},
        {"spine2", {0.12f, 0.15f, 0.08f}},
        {"neck", {0.05f, 0.08f, 0.05f}},
        {"head", {0.1f, 0.12f, 0.1f}},
        {"left_upper_arm", {0.04f, 0.15f, 0.04f}},
        {"left_lower_arm", {0.035f, 0.13f, 0.035f}},
        {"right_upper_arm", {0.04f, 0.15f, 0.04f}},
        {"right_lower_arm", {0.035f, 0.13f, 0.035f}},
        {"left_upper_leg", {0.06f, 0.22f, 0.06f}},
        {"left_lower_leg", {0.05f, 0.2f, 0.05f}},
        {"right_upper_leg", {0.06f, 0.22f, 0.06f}},
        {"right_lower_leg", {0.05f, 0.2f, 0.05f}},
    };

    for (const auto& [bone_name, dimensions] : humanoid_bones) {
        RagdollBodyDef body;
        body.bone_name = bone_name;
        body.shape = RagdollShapeType::Capsule;
        body.dimensions = dimensions;
        body.mass = dimensions.x * dimensions.y * dimensions.z * 1000.0f;  // Approximate
        def.bodies.push_back(body);
    }

    def.root_body = "pelvis";

    return def;
}

bool RagdollDefinition::is_valid() const {
    if (bodies.empty()) return false;
    if (root_body.empty()) return false;

    // Check that root body exists
    bool found_root = false;
    for (const auto& body : bodies) {
        if (body.bone_name == root_body) {
            found_root = true;
            break;
        }
    }

    return found_root;
}

// ============================================================================
// Ragdoll
// ============================================================================

Ragdoll::Ragdoll() = default;
Ragdoll::~Ragdoll() {
    shutdown();
}

Ragdoll::Ragdoll(Ragdoll&&) noexcept = default;
Ragdoll& Ragdoll::operator=(Ragdoll&&) noexcept = default;

void Ragdoll::init(PhysicsWorld& world, const RagdollDefinition& def,
                   const render::Skeleton& skeleton) {
    if (m_initialized) {
        shutdown();
    }

    // Validate inputs
    if (!def.is_valid()) {
        core::log(core::LogLevel::Error,
            "Ragdoll::init failed: invalid definition (empty bodies or missing root)");
        return;
    }

    if (skeleton.get_bone_count() == 0) {
        core::log(core::LogLevel::Error,
            "Ragdoll::init failed: skeleton has no bones");
        return;
    }

    m_world = &world;
    m_definition = def;
    m_skeleton = &skeleton;

    create_bodies(world, skeleton);
    create_joints(world);

    m_initialized = true;
    m_state = RagdollState::Disabled;

    core::log(core::LogLevel::Debug, "Ragdoll initialized with {} bodies, {} joints",
              m_definition.bodies.size(), m_definition.joints.size());
}

void Ragdoll::shutdown() {
    if (!m_initialized) return;

    destroy_joints();
    destroy_bodies();

    m_bone_to_body.clear();
    m_bone_to_index.clear();
    m_world = nullptr;
    m_skeleton = nullptr;
    m_initialized = false;
}

bool Ragdoll::is_initialized() const {
    return m_initialized;
}

void Ragdoll::set_state(RagdollState state) {
    if (m_state == state) return;
    if (!m_initialized || !m_world) {
        core::log(core::LogLevel::Warn, "Ragdoll::set_state called on uninitialized ragdoll");
        return;
    }

    RagdollState old_state = m_state;
    m_state = state;

    switch (state) {
        case RagdollState::Disabled:
            // Set all bodies to kinematic - animation controls them
            for (auto& [name, body_id] : m_bone_to_body) {
                m_world->set_motion_type(body_id, BodyType::Kinematic);
            }
            // Disable motors on joints
            for (uint32_t joint_id : m_joint_ids) {
                m_world->set_constraint_motor_state(ConstraintId{joint_id}, false);
            }
            break;

        case RagdollState::Active:
            // Set all bodies to dynamic - physics controls them
            for (auto& [name, body_id] : m_bone_to_body) {
                m_world->set_motion_type(body_id, BodyType::Dynamic);
                m_world->activate_body(body_id);
            }
            // Disable motors for pure ragdoll
            for (uint32_t joint_id : m_joint_ids) {
                m_world->set_constraint_motor_state(ConstraintId{joint_id}, false);
            }
            break;

        case RagdollState::Blending:
            m_blend_time = 0.0f;
            // Keep bodies dynamic during blend
            for (auto& [name, body_id] : m_bone_to_body) {
                m_world->set_motion_type(body_id, BodyType::Dynamic);
            }
            break;

        case RagdollState::Powered:
            // Set bodies to dynamic with motors enabled
            for (auto& [name, body_id] : m_bone_to_body) {
                m_world->set_motion_type(body_id, BodyType::Dynamic);
                m_world->activate_body(body_id);
            }
            // Enable motors on all joints
            for (uint32_t joint_id : m_joint_ids) {
                m_world->set_constraint_motor_state(ConstraintId{joint_id}, true);
                m_world->set_constraint_motor_strength(ConstraintId{joint_id}, m_motor_strength * 1000.0f);
            }
            break;
    }

    core::log(core::LogLevel::Debug, "Ragdoll state changed: {} -> {}",
              static_cast<int>(old_state), static_cast<int>(state));
}

RagdollState Ragdoll::get_state() const {
    return m_state;
}

void Ragdoll::activate(const std::vector<render::BoneTransform>& current_pose,
                       const Vec3& initial_velocity) {
    if (!m_initialized) return;

    // Set body positions from current pose
    // This would map bone transforms to ragdoll body positions

    // Apply initial velocity to all bodies
    for (auto& [name, body_id] : m_bone_to_body) {
        m_world->set_linear_velocity(body_id, initial_velocity);
    }

    set_state(RagdollState::Active);
}

void Ragdoll::deactivate() {
    set_state(RagdollState::Disabled);
}

void Ragdoll::blend_to_animation(float duration) {
    m_blend_duration = duration;
    m_blend_time = 0.0f;

    // Store current ragdoll pose as blend start
    get_pose(m_blend_start_pose);

    set_state(RagdollState::Blending);
}

void Ragdoll::apply_impulse(const std::string& bone_name, const Vec3& impulse, const Vec3& point) {
    if (!m_initialized || !m_world) {
        core::log(core::LogLevel::Warn,
            "Ragdoll::apply_impulse called on uninitialized ragdoll");
        return;
    }

    // Validate impulse magnitude (prevent physics explosion)
    float impulse_mag = glm::length(impulse);
    if (impulse_mag > 10000.0f) {
        core::log(core::LogLevel::Warn,
            "Ragdoll::apply_impulse: impulse magnitude {} exceeds safe limit, clamping",
            impulse_mag);
    }

    auto it = m_bone_to_body.find(bone_name);
    if (it == m_bone_to_body.end()) {
        core::log(core::LogLevel::Warn,
            "Ragdoll::apply_impulse: bone '{}' not found", bone_name);
        return;
    }

    // Clamp impulse to safe range
    Vec3 safe_impulse = impulse;
    if (impulse_mag > 10000.0f) {
        safe_impulse = glm::normalize(impulse) * 10000.0f;
    }

    m_world->add_impulse_at_point(it->second, safe_impulse, point);
}

void Ragdoll::apply_force(const Vec3& force) {
    if (!m_initialized || !m_world) {
        core::log(core::LogLevel::Warn,
            "Ragdoll::apply_force called on uninitialized ragdoll");
        return;
    }

    for (auto& [name, body_id] : m_bone_to_body) {
        m_world->add_force(body_id, force);
    }
}

void Ragdoll::get_pose(std::vector<render::BoneTransform>& out_pose) const {
    if (!m_skeleton || !m_world) {
        out_pose.clear();
        return;
    }

    const int bone_count = m_skeleton->get_bone_count();
    out_pose.resize(bone_count);

    // Start with bind pose as default
    std::vector<render::BoneTransform> bind_pose = m_skeleton->get_bind_pose();
    out_pose = bind_pose;

    // Override bones that have physics bodies
    for (const auto& [bone_name, body_id] : m_bone_to_body) {
        int32_t bone_index = m_skeleton->find_bone(bone_name);
        if (bone_index < 0 || bone_index >= bone_count) continue;

        Vec3 world_pos = m_world->get_position(body_id);
        Quat world_rot = m_world->get_rotation(body_id);

        // Get the body def to apply offset correction
        auto it = m_bone_to_index.find(bone_name);
        if (it != m_bone_to_index.end() && it->second < static_cast<int>(m_definition.bodies.size())) {
            const auto& body_def = m_definition.bodies[it->second];
            // Remove offset to get bone position
            Vec3 offset_world = world_rot * body_def.offset;
            world_pos -= offset_world;
            // Apply rotation offset inverse
            world_rot = world_rot * glm::inverse(body_def.rotation_offset);
        }

        // Convert world transform to local (relative to parent bone)
        const render::Bone& bone = m_skeleton->get_bone(bone_index);
        if (bone.parent_index >= 0 && bone.parent_index < bone_count) {
            // Get parent world transform - for simplicity, use physics body if available
            auto parent_it = m_bone_to_body.find(m_skeleton->get_bone(bone.parent_index).name);
            if (parent_it != m_bone_to_body.end()) {
                Vec3 parent_pos = m_world->get_position(parent_it->second);
                Quat parent_rot = m_world->get_rotation(parent_it->second);

                // Convert to local space
                Quat inv_parent_rot = glm::inverse(parent_rot);
                Vec3 local_pos = inv_parent_rot * (world_pos - parent_pos);
                Quat local_rot = inv_parent_rot * world_rot;

                out_pose[bone_index].position = local_pos;
                out_pose[bone_index].rotation = local_rot;
            } else {
                // No parent body, use world transform directly
                out_pose[bone_index].position = world_pos;
                out_pose[bone_index].rotation = world_rot;
            }
        } else {
            // Root bone - use world transform
            out_pose[bone_index].position = world_pos;
            out_pose[bone_index].rotation = world_rot;
        }
        out_pose[bone_index].scale = Vec3{1.0f};
    }
}

void Ragdoll::update(float dt, const std::vector<render::BoneTransform>* anim_pose) {
    if (!m_initialized) return;

    switch (m_state) {
        case RagdollState::Disabled:
            // Nothing to do - animation controls the pose
            break;

        case RagdollState::Active:
            // Physics simulation handles this
            break;

        case RagdollState::Blending:
            if (anim_pose) {
                update_blend(dt, *anim_pose);
            }
            break;

        case RagdollState::Powered:
            if (anim_pose) {
                // Apply motor targets to follow animation
                set_motor_targets(*anim_pose);
            }
            break;
    }
}

void Ragdoll::set_bone_kinematic(const std::string& bone_name, bool kinematic) {
    if (!m_initialized || !m_world) {
        core::log(core::LogLevel::Warn,
            "Ragdoll::set_bone_kinematic called on uninitialized ragdoll");
        return;
    }

    auto it = m_bone_to_body.find(bone_name);
    if (it == m_bone_to_body.end()) {
        core::log(core::LogLevel::Warn,
            "Ragdoll::set_bone_kinematic: bone '{}' not found", bone_name);
        return;
    }

    BodyType type = kinematic ? BodyType::Kinematic : BodyType::Dynamic;
    m_world->set_motion_type(it->second, type);

    if (!kinematic) {
        m_world->activate_body(it->second);
    }
}

void Ragdoll::set_bones_kinematic_below(const std::string& bone_name, bool kinematic) {
    if (!m_initialized || !m_world || !m_skeleton) {
        core::log(core::LogLevel::Warn,
            "Ragdoll::set_bones_kinematic_below called on uninitialized ragdoll");
        return;
    }

    int32_t bone_index = m_skeleton->find_bone(bone_name);
    if (bone_index < 0) {
        core::log(core::LogLevel::Warn,
            "Ragdoll::set_bones_kinematic_below: bone '{}' not found", bone_name);
        return;
    }

    // Recursive helper to process bone and children
    std::function<void(int32_t)> process_bone = [&](int32_t idx) {
        if (idx < 0 || idx >= m_skeleton->get_bone_count()) return;

        const render::Bone& bone = m_skeleton->get_bone(idx);
        set_bone_kinematic(bone.name, kinematic);

        for (int32_t child_idx : bone.children) {
            process_bone(child_idx);
        }
    };

    process_bone(bone_index);
}

void Ragdoll::set_motor_targets(const std::vector<render::BoneTransform>& target_pose) {
    if (!m_initialized || !m_world || !m_skeleton || target_pose.empty()) {
        return;
    }

    // For each joint, calculate target rotation from pose
    for (size_t i = 0; i < m_definition.joints.size() && i < m_joint_ids.size(); ++i) {
        const auto& joint_def = m_definition.joints[i];

        int32_t bone_a_idx = m_skeleton->find_bone(joint_def.body_a);
        int32_t bone_b_idx = m_skeleton->find_bone(joint_def.body_b);

        if (bone_a_idx < 0 || bone_b_idx < 0) continue;
        if (bone_b_idx >= static_cast<int32_t>(target_pose.size())) continue;

        // Get relative rotation from parent to child in target pose
        Quat parent_rot = (bone_a_idx < static_cast<int32_t>(target_pose.size()))
            ? target_pose[bone_a_idx].rotation
            : Quat{1, 0, 0, 0};
        Quat child_rot = target_pose[bone_b_idx].rotation;

        Quat relative_rot = glm::inverse(parent_rot) * child_rot;

        // Set motor target with strength scaling
        float effective_torque = m_motor_strength * 1000.0f;
        m_world->set_constraint_motor_target(ConstraintId{m_joint_ids[i]}, relative_rot);
        m_world->set_constraint_motor_strength(ConstraintId{m_joint_ids[i]}, effective_torque);
    }
}

void Ragdoll::set_motor_strength(float strength) {
    m_motor_strength = glm::clamp(strength, 0.0f, 1.0f);
}

float Ragdoll::get_motor_strength() const {
    return m_motor_strength;
}

void Ragdoll::set_position(const Vec3& pos) {
    // Get root body position offset
    auto it = m_bone_to_body.find(m_definition.root_body);
    if (it == m_bone_to_body.end()) return;

    Vec3 current_root_pos = m_world->get_position(it->second);
    Vec3 offset = pos - current_root_pos;

    // Move all bodies by offset
    for (auto& [name, body_id] : m_bone_to_body) {
        Vec3 body_pos = m_world->get_position(body_id);
        m_world->set_position(body_id, body_pos + offset);
    }
}

Vec3 Ragdoll::get_position() const {
    auto it = m_bone_to_body.find(m_definition.root_body);
    if (it != m_bone_to_body.end()) {
        return m_world->get_position(it->second);
    }
    return Vec3{0.0f};
}

void Ragdoll::set_rotation(const Quat& rot) {
    // Would rotate all bodies around the root
}

Quat Ragdoll::get_rotation() const {
    auto it = m_bone_to_body.find(m_definition.root_body);
    if (it != m_bone_to_body.end()) {
        return m_world->get_rotation(it->second);
    }
    return Quat{1, 0, 0, 0};
}

PhysicsBodyId Ragdoll::get_body(const std::string& bone_name) const {
    auto it = m_bone_to_body.find(bone_name);
    if (it != m_bone_to_body.end()) {
        return it->second;
    }
    return PhysicsBodyId{};
}

std::vector<PhysicsBodyId> Ragdoll::get_all_bodies() const {
    std::vector<PhysicsBodyId> bodies;
    bodies.reserve(m_bone_to_body.size());
    for (const auto& [name, id] : m_bone_to_body) {
        bodies.push_back(id);
    }
    return bodies;
}

void Ragdoll::set_collision_layer(uint16_t layer) {
    m_collision_layer = layer;
    // Would update all body layers
}

uint16_t Ragdoll::get_collision_layer() const {
    return m_collision_layer;
}

void Ragdoll::create_bodies(PhysicsWorld& world, const render::Skeleton& skeleton) {
    for (size_t i = 0; i < m_definition.bodies.size(); ++i) {
        const auto& body_def = m_definition.bodies[i];

        BodySettings settings;
        settings.type = BodyType::Dynamic;
        settings.mass = body_def.mass;
        settings.friction = body_def.friction;
        settings.restitution = body_def.restitution;
        settings.layer = m_collision_layer;

        // Set shape based on type
        switch (body_def.shape) {
            case RagdollShapeType::Capsule:
            {
                CapsuleShapeSettings shape(body_def.dimensions.x, body_def.dimensions.y);
                shape.center_offset = body_def.offset;
                shape.rotation_offset = body_def.rotation_offset;
                settings.shape = &shape;
                PhysicsBodyId body_id = world.create_body(settings);
                m_bone_to_body[body_def.bone_name] = body_id;
                m_bone_to_index[body_def.bone_name] = static_cast<int>(i);
                break;
            }
            case RagdollShapeType::Box:
            {
                BoxShapeSettings shape(body_def.dimensions);
                shape.center_offset = body_def.offset;
                shape.rotation_offset = body_def.rotation_offset;
                settings.shape = &shape;
                PhysicsBodyId body_id = world.create_body(settings);
                m_bone_to_body[body_def.bone_name] = body_id;
                m_bone_to_index[body_def.bone_name] = static_cast<int>(i);
                break;
            }
            case RagdollShapeType::Sphere:
            {
                SphereShapeSettings shape(body_def.dimensions.x);
                shape.center_offset = body_def.offset;
                shape.rotation_offset = body_def.rotation_offset;
                settings.shape = &shape;
                PhysicsBodyId body_id = world.create_body(settings);
                m_bone_to_body[body_def.bone_name] = body_id;
                m_bone_to_index[body_def.bone_name] = static_cast<int>(i);
                break;
            }
        }
    }
}

void Ragdoll::create_joints(PhysicsWorld& world) {
    m_joint_ids.clear();
    m_joint_ids.reserve(m_definition.joints.size());

    for (const auto& joint_def : m_definition.joints) {
        auto it_a = m_bone_to_body.find(joint_def.body_a);
        auto it_b = m_bone_to_body.find(joint_def.body_b);

        if (it_a == m_bone_to_body.end() || it_b == m_bone_to_body.end()) {
            core::log(core::LogLevel::Warn,
                "Ragdoll joint creation failed: bodies '{}' or '{}' not found",
                joint_def.body_a, joint_def.body_b);
            continue;
        }

        ConstraintId constraint_id;

        switch (joint_def.type) {
            case RagdollJointType::Fixed: {
                FixedConstraintSettings settings;
                settings.body_a = it_a->second;
                settings.body_b = it_b->second;
                settings.local_anchor_a = joint_def.local_anchor_a;
                settings.local_anchor_b = joint_def.local_anchor_b;
                constraint_id = world.create_fixed_constraint(settings);
                break;
            }
            case RagdollJointType::Hinge: {
                HingeConstraintSettings settings;
                settings.body_a = it_a->second;
                settings.body_b = it_b->second;
                settings.local_anchor_a = joint_def.local_anchor_a;
                settings.local_anchor_b = joint_def.local_anchor_b;
                settings.hinge_axis = joint_def.hinge_axis;
                settings.limit_min = joint_def.twist_min;
                settings.limit_max = joint_def.twist_max;
                settings.enable_limits = true;
                constraint_id = world.create_hinge_constraint(settings);
                break;
            }
            case RagdollJointType::Cone:
            case RagdollJointType::Twist: {
                SwingTwistConstraintSettings settings;
                settings.body_a = it_a->second;
                settings.body_b = it_b->second;
                settings.local_anchor_a = joint_def.local_anchor_a;
                settings.local_anchor_b = joint_def.local_anchor_b;
                settings.twist_axis = Vec3{0.0f, 1.0f, 0.0f};
                settings.plane_axis = Vec3{1.0f, 0.0f, 0.0f};
                settings.swing_limit_y = joint_def.swing_limit_1;
                settings.swing_limit_z = joint_def.swing_limit_2;
                settings.twist_min = joint_def.twist_min;
                settings.twist_max = joint_def.twist_max;
                constraint_id = world.create_swing_twist_constraint(settings);
                break;
            }
        }

        if (constraint_id.valid()) {
            m_joint_ids.push_back(constraint_id.id);
        }
    }

    core::log(core::LogLevel::Debug, "Created {} ragdoll joints", m_joint_ids.size());
}

void Ragdoll::destroy_bodies() {
    if (!m_world) return;

    for (auto& [name, body_id] : m_bone_to_body) {
        m_world->destroy_body(body_id);
    }
    m_bone_to_body.clear();
}

void Ragdoll::destroy_joints() {
    if (!m_world) return;

    for (uint32_t joint_id : m_joint_ids) {
        m_world->destroy_constraint(ConstraintId{joint_id});
    }
    m_joint_ids.clear();
}

void Ragdoll::update_blend(float dt, const std::vector<render::BoneTransform>& anim_pose) {
    if (!m_initialized || !m_world || !m_skeleton) return;

    m_blend_time += dt;
    float t = m_blend_time / m_blend_duration;

    if (t >= 1.0f) {
        // Blend complete - switch to disabled (animation controlled)
        set_state(RagdollState::Disabled);
        return;
    }

    // Smooth blend using ease-out curve
    float blend_factor = 1.0f - (1.0f - t) * (1.0f - t);

    // Get current ragdoll pose
    std::vector<render::BoneTransform> ragdoll_pose;
    get_pose(ragdoll_pose);

    // For each body, interpolate position/rotation toward animation target
    for (const auto& [bone_name, body_id] : m_bone_to_body) {
        int32_t bone_index = m_skeleton->find_bone(bone_name);
        if (bone_index < 0 || bone_index >= static_cast<int32_t>(anim_pose.size())) continue;
        if (bone_index >= static_cast<int32_t>(ragdoll_pose.size())) continue;

        const auto& anim_xform = anim_pose[bone_index];
        const auto& phys_xform = ragdoll_pose[bone_index];

        // Interpolate transform
        Vec3 blended_pos = glm::mix(phys_xform.position, anim_xform.position, blend_factor);
        Quat blended_rot = glm::slerp(phys_xform.rotation, anim_xform.rotation, blend_factor);

        // Get body offset from definition
        auto idx_it = m_bone_to_index.find(bone_name);
        Vec3 offset{0.0f};
        Quat rot_offset{1, 0, 0, 0};
        if (idx_it != m_bone_to_index.end() && idx_it->second < static_cast<int>(m_definition.bodies.size())) {
            const auto& body_def = m_definition.bodies[idx_it->second];
            offset = body_def.offset;
            rot_offset = body_def.rotation_offset;
        }

        // Apply offset to get body position
        Vec3 body_pos = blended_pos + blended_rot * offset;
        Quat body_rot = blended_rot * rot_offset;

        // Apply blended transform
        m_world->set_position(body_id, body_pos);
        m_world->set_rotation(body_id, body_rot);
    }
}

} // namespace engine::physics
