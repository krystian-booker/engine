#include <engine/ai/bt_nodes.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/core/log.hpp>
#include <cmath>

namespace engine::ai {

namespace {

Vec3 get_entity_position(scene::World& world, scene::Entity entity) {
    auto* world_transform = world.try_get<scene::WorldTransform>(entity);
    if (world_transform) {
        return world_transform->get_position();
    }
    auto* local_transform = world.try_get<scene::LocalTransform>(entity);
    if (local_transform) {
        return local_transform->position;
    }
    return Vec3(0.0f);
}

} // anonymous namespace

// ============================================================================
// BTIsInRange Implementation
// ============================================================================

BTStatus BTIsInRange::tick(BTContext& ctx) {
    if (!ctx.is_valid()) {
        m_last_status = BTStatus::Failure;
        return BTStatus::Failure;
    }

    // Get self position
    Vec3 self_pos = get_entity_position(*ctx.world, ctx.entity);

    // Get target position from blackboard
    Vec3 target_pos = ctx.blackboard->get_position(m_target_key);

    // If target is an entity, get its position
    if (ctx.blackboard->has(bb::TARGET_ENTITY)) {
        scene::Entity target = ctx.blackboard->get_entity(bb::TARGET_ENTITY);
        if (target != scene::NullEntity && ctx.world->valid(target)) {
            target_pos = get_entity_position(*ctx.world, target);
            // Update target position in blackboard
            ctx.blackboard->set_position(bb::TARGET_POSITION, target_pos);
        }
    }

    float distance = glm::length(target_pos - self_pos);
    ctx.blackboard->set_float(bb::TARGET_DISTANCE, distance);

    m_last_status = (distance <= m_range) ? BTStatus::Success : BTStatus::Failure;
    return m_last_status;
}

// ============================================================================
// BTMoveTo Implementation
// ============================================================================

BTStatus BTMoveTo::tick(BTContext& ctx) {
    if (!ctx.is_valid()) {
        m_last_status = BTStatus::Failure;
        return BTStatus::Failure;
    }

    // Get target position
    Vec3 target_pos = ctx.blackboard->get_position(m_target_key);

    // Get current position
    Vec3 current_pos = get_entity_position(*ctx.world, ctx.entity);

    // Check if arrived
    float distance = glm::length(target_pos - current_pos);
    if (distance <= m_arrival_distance) {
        m_path_requested = false;
        m_last_status = BTStatus::Success;
        return BTStatus::Success;
    }

    // Calculate direction
    Vec3 direction = glm::normalize(target_pos - current_pos);

    // Move towards target (simplified - real implementation uses NavAgent)
    Vec3 movement = direction * m_movement_speed * ctx.delta_time;
    Vec3 new_pos = current_pos + movement;

    // Update position
    auto* local_transform = ctx.world->try_get<scene::LocalTransform>(ctx.entity);
    if (local_transform) {
        local_transform->position = new_pos;
    }

    ctx.blackboard->set_bool(bb::PATH_FOUND, true);

    m_last_status = BTStatus::Running;
    return BTStatus::Running;
}

// ============================================================================
// BTLookAt Implementation
// ============================================================================

BTStatus BTLookAt::tick(BTContext& ctx) {
    if (!ctx.is_valid()) {
        m_last_status = BTStatus::Failure;
        return BTStatus::Failure;
    }

    // Get target position
    Vec3 target_pos = ctx.blackboard->get_position(m_target_key);

    // Get current position
    Vec3 current_pos = get_entity_position(*ctx.world, ctx.entity);

    // Calculate direction to target
    Vec3 to_target = target_pos - current_pos;
    to_target.y = 0; // Keep on horizontal plane

    if (glm::length(to_target) < 0.01f) {
        m_last_status = BTStatus::Success;
        return BTStatus::Success;
    }

    to_target = glm::normalize(to_target);

    // Calculate target rotation
    float target_yaw = std::atan2(to_target.x, to_target.z);

    // Get current rotation
    auto* local_transform = ctx.world->try_get<scene::LocalTransform>(ctx.entity);
    if (!local_transform) {
        m_last_status = BTStatus::Failure;
        return BTStatus::Failure;
    }

    // Convert current rotation to yaw
    Vec3 euler = glm::eulerAngles(local_transform->rotation);
    float current_yaw = euler.y;

    // Calculate angle difference
    float diff = target_yaw - current_yaw;

    // Normalize to -PI to PI
    while (diff > 3.14159f) diff -= 6.28318f;
    while (diff < -3.14159f) diff += 6.28318f;

    // Rotate towards target
    float max_rotation = glm::radians(m_rotation_speed) * ctx.delta_time;
    float rotation_amount = glm::clamp(diff, -max_rotation, max_rotation);

    // Apply rotation
    euler.y += rotation_amount;
    local_transform->rotation = Quat(euler);

    // Check if facing target
    if (std::abs(diff) < 0.1f) {
        m_last_status = BTStatus::Success;
        return BTStatus::Success;
    }

    m_last_status = BTStatus::Running;
    return BTStatus::Running;
}

// ============================================================================
// BTPlayAnimation Implementation
// ============================================================================

BTStatus BTPlayAnimation::tick(BTContext& ctx) {
    if (!ctx.is_valid()) {
        m_last_status = BTStatus::Failure;
        return BTStatus::Failure;
    }

    // Start animation if not started
    if (!m_animation_started) {
        // In real implementation, this would trigger the animation system
        // For now, just mark as started
        m_animation_started = true;

        if (!m_wait_for_completion) {
            m_last_status = BTStatus::Success;
            return BTStatus::Success;
        }
    }

    // Check if animation is complete
    // In real implementation, this would query the animation system
    // For now, immediately succeed
    m_animation_started = false;
    m_last_status = BTStatus::Success;
    return BTStatus::Success;
}

// ============================================================================
// BTPlaySound Implementation
// ============================================================================

BTStatus BTPlaySound::tick(BTContext& ctx) {
    if (!ctx.is_valid()) {
        m_last_status = BTStatus::Failure;
        return BTStatus::Failure;
    }

    // In real implementation, this would play a sound via the audio system
    // For now, just succeed
    m_last_status = BTStatus::Success;
    return BTStatus::Success;
}

} // namespace engine::ai
