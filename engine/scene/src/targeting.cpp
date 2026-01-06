#include <engine/scene/targeting.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/core/event_dispatcher.hpp>
#include <engine/core/log.hpp>
#include <engine/reflect/type_registry.hpp>
#include <algorithm>
#include <cmath>

namespace engine::scene {

namespace {

Vec3 get_entity_position(World& world, Entity entity) {
    auto* world_transform = world.try_get<WorldTransform>(entity);
    if (world_transform) {
        return world_transform->get_position();
    }
    auto* local_transform = world.try_get<LocalTransform>(entity);
    if (local_transform) {
        return local_transform->position;
    }
    return Vec3(0.0f);
}

Vec3 get_entity_forward(World& world, Entity entity) {
    auto* world_transform = world.try_get<WorldTransform>(entity);
    if (world_transform) {
        return -Vec3(world_transform->matrix[2][0],
                     world_transform->matrix[2][1],
                     world_transform->matrix[2][2]);
    }
    auto* local_transform = world.try_get<LocalTransform>(entity);
    if (local_transform) {
        return local_transform->forward();
    }
    return Vec3(0.0f, 0.0f, -1.0f);
}

Vec3 transform_offset(const Vec3& offset, World& world, Entity entity) {
    auto* world_transform = world.try_get<WorldTransform>(entity);
    if (world_transform) {
        Vec4 local_pos(offset.x, offset.y, offset.z, 1.0f);
        Vec4 world_pos = world_transform->matrix * local_pos;
        return Vec3(world_pos.x, world_pos.y, world_pos.z);
    }
    auto* local_transform = world.try_get<LocalTransform>(entity);
    if (local_transform) {
        return local_transform->position + offset;
    }
    return offset;
}

float angle_between_vectors(const Vec3& a, const Vec3& b) {
    float dot = glm::dot(glm::normalize(a), glm::normalize(b));
    dot = glm::clamp(dot, -1.0f, 1.0f);
    return glm::degrees(std::acos(dot));
}

// Get horizontal angle for left/right switching
float get_horizontal_angle(const Vec3& forward, const Vec3& right, const Vec3& to_target) {
    Vec3 flat_forward = glm::normalize(Vec3(forward.x, 0.0f, forward.z));
    Vec3 flat_right = glm::normalize(Vec3(right.x, 0.0f, right.z));
    Vec3 flat_to_target = glm::normalize(Vec3(to_target.x, 0.0f, to_target.z));

    float forward_dot = glm::dot(flat_forward, flat_to_target);
    float right_dot = glm::dot(flat_right, flat_to_target);

    return std::atan2(right_dot, forward_dot);
}

} // anonymous namespace

TargetingSystem::TargetingSystem() {
    // Default line of sight check - always returns true
    // Real implementation would use physics raycast
    m_line_of_sight_check = [this](World& world, const Vec3& from, const Vec3& to, Entity exclude) {
        return default_line_of_sight_check(world, from, to, exclude);
    };
}

TargetingSystem& TargetingSystem::instance() {
    static TargetingSystem s_instance;
    return s_instance;
}

std::optional<TargetCandidate> TargetingSystem::find_best_target(
    World& world,
    Entity targeter,
    const Vec3& position,
    const Vec3& forward
) {
    auto targets = find_all_targets(world, targeter, position, forward);
    if (targets.empty()) {
        return std::nullopt;
    }
    return targets.front(); // Already sorted by score
}

std::vector<TargetCandidate> TargetingSystem::find_all_targets(
    World& world,
    Entity targeter,
    const Vec3& position,
    const Vec3& forward,
    float max_distance
) {
    std::vector<TargetCandidate> candidates;

    auto* targeter_comp = world.try_get<TargeterComponent>(targeter);
    if (!targeter_comp) return candidates;

    float max_dist = max_distance > 0.0f ? max_distance : targeter_comp->max_lock_distance;
    float max_angle = targeter_comp->lock_angle;

    auto view = world.view<TargetableComponent>();

    for (auto entity : view) {
        if (entity == targeter) continue;

        auto& targetable = view.get<TargetableComponent>(entity);

        if (!targetable.enabled) continue;
        if (!is_faction_targetable(*targeter_comp, targetable.faction)) continue;

        auto candidate = evaluate_target(world, targeter, entity, *targeter_comp, targetable, position, forward);

        // Filter by distance
        if (candidate.distance > max_dist) continue;
        if (candidate.distance < targetable.min_target_distance) continue;

        // Filter by angle
        if (candidate.angle > max_angle) continue;

        // Check line of sight if required
        if (targetable.requires_line_of_sight && !targetable.target_through_walls) {
            candidate.in_line_of_sight = m_line_of_sight_check(world, position, candidate.target_point, targeter);
            if (!candidate.in_line_of_sight) continue;
        }

        // Calculate final score
        candidate.is_current_target = (entity == targeter_comp->current_target);
        candidate.score = calculate_score(candidate, *targeter_comp, targetable, candidate.is_current_target);

        candidates.push_back(candidate);
    }

    // Sort by score (highest first)
    std::sort(candidates.begin(), candidates.end(),
              [](const TargetCandidate& a, const TargetCandidate& b) {
                  return a.score > b.score;
              });

    return candidates;
}

std::optional<TargetCandidate> TargetingSystem::can_target(
    World& world,
    Entity targeter,
    Entity target,
    const Vec3& position,
    const Vec3& forward
) {
    auto* targeter_comp = world.try_get<TargeterComponent>(targeter);
    if (!targeter_comp) return std::nullopt;

    auto* targetable = world.try_get<TargetableComponent>(target);
    if (!targetable || !targetable->enabled) return std::nullopt;

    if (!is_faction_targetable(*targeter_comp, targetable->faction)) return std::nullopt;

    auto candidate = evaluate_target(world, targeter, target, *targeter_comp, *targetable, position, forward);

    // Check distance
    if (candidate.distance > targeter_comp->max_lock_distance) return std::nullopt;
    if (candidate.distance < targetable->min_target_distance) return std::nullopt;

    // Check angle
    if (candidate.angle > targeter_comp->lock_angle) return std::nullopt;

    // Check LOS
    if (targetable->requires_line_of_sight && !targetable->target_through_walls) {
        candidate.in_line_of_sight = m_line_of_sight_check(world, position, candidate.target_point, targeter);
        if (!candidate.in_line_of_sight) return std::nullopt;
    }

    candidate.score = calculate_score(candidate, *targeter_comp, *targetable, candidate.is_current_target);

    return candidate;
}

void TargetingSystem::lock_on(World& world, Entity targeter, Entity target) {
    auto* targeter_comp = world.try_get<TargeterComponent>(targeter);
    if (!targeter_comp) return;

    auto* targetable = world.try_get<TargetableComponent>(target);
    if (!targetable || !targetable->enabled) return;

    Entity old_target = targeter_comp->current_target;

    // Update old target's state
    if (old_target != NullEntity && old_target != target) {
        auto* old_targetable = world.try_get<TargetableComponent>(old_target);
        if (old_targetable) {
            old_targetable->is_targeted = false;
            old_targetable->targeted_by = NullEntity;
        }
    }

    // Update new target's state
    targetable->is_targeted = true;
    targetable->targeted_by = targeter;

    // Update targeter state
    targeter_comp->current_target = target;
    targeter_comp->lock_on_active = true;
    targeter_comp->time_target_not_visible = 0.0f;
    targeter_comp->time_since_switch = 0.0f;

    // Notify
    notify_target_changed(targeter, old_target, target);

    // Emit event
    TargetAcquiredEvent event;
    event.targeter = targeter;
    event.target = target;
    event.is_hard_lock = true;
    core::EventDispatcher::instance().dispatch(event);

    core::log(core::LogLevel::Debug, "Locked on to target");
}

void TargetingSystem::unlock(World& world, Entity targeter) {
    auto* targeter_comp = world.try_get<TargeterComponent>(targeter);
    if (!targeter_comp) return;

    Entity old_target = targeter_comp->current_target;

    if (old_target != NullEntity) {
        auto* targetable = world.try_get<TargetableComponent>(old_target);
        if (targetable) {
            targetable->is_targeted = false;
            targetable->targeted_by = NullEntity;
        }
    }

    targeter_comp->current_target = NullEntity;
    targeter_comp->lock_on_active = false;
    targeter_comp->soft_lock_target = NullEntity;

    // Notify
    notify_target_changed(targeter, old_target, NullEntity);

    // Emit event
    if (old_target != NullEntity) {
        TargetLostEvent event;
        event.targeter = targeter;
        event.previous_target = old_target;
        event.reason = TargetLostReason::Manual;
        core::EventDispatcher::instance().dispatch(event);
    }

    core::log(core::LogLevel::Debug, "Lock-on released");
}

void TargetingSystem::toggle_lock_on(World& world, Entity targeter, const Vec3& position, const Vec3& forward) {
    auto* targeter_comp = world.try_get<TargeterComponent>(targeter);
    if (!targeter_comp) return;

    if (targeter_comp->lock_on_active && targeter_comp->current_target != NullEntity) {
        unlock(world, targeter);
    } else {
        auto best = find_best_target(world, targeter, position, forward);
        if (best) {
            lock_on(world, targeter, best->entity);
        }
    }
}

bool TargetingSystem::is_locked_on(World& world, Entity targeter) const {
    auto* targeter_comp = world.try_get<TargeterComponent>(targeter);
    return targeter_comp && targeter_comp->lock_on_active && targeter_comp->current_target != NullEntity;
}

Entity TargetingSystem::get_current_target(World& world, Entity targeter) const {
    auto* targeter_comp = world.try_get<TargeterComponent>(targeter);
    return targeter_comp ? targeter_comp->current_target : NullEntity;
}

Entity TargetingSystem::switch_target(
    World& world,
    Entity targeter,
    const Vec3& position,
    const Vec3& forward,
    SwitchDirection direction
) {
    auto* targeter_comp = world.try_get<TargeterComponent>(targeter);
    if (!targeter_comp) return NullEntity;

    // Check cooldown
    if (targeter_comp->time_since_switch < targeter_comp->switch_cooldown) {
        return targeter_comp->current_target;
    }

    auto candidates = find_all_targets(world, targeter, position, forward);
    if (candidates.empty()) return NullEntity;

    Entity old_target = targeter_comp->current_target;
    Entity new_target = NullEntity;

    // Handle different switch directions
    switch (direction) {
        case SwitchDirection::Nearest:
            // Already sorted, but re-sort by distance
            std::sort(candidates.begin(), candidates.end(),
                      [](const TargetCandidate& a, const TargetCandidate& b) {
                          return a.distance < b.distance;
                      });
            new_target = candidates.front().entity;
            break;

        case SwitchDirection::Farthest:
            std::sort(candidates.begin(), candidates.end(),
                      [](const TargetCandidate& a, const TargetCandidate& b) {
                          return a.distance > b.distance;
                      });
            new_target = candidates.front().entity;
            break;

        case SwitchDirection::Next:
        case SwitchDirection::Previous:
            return cycle_target(world, targeter, position, forward,
                               direction == SwitchDirection::Next);

        case SwitchDirection::Left:
        case SwitchDirection::Right: {
            // Sort by horizontal angle
            Vec3 right = glm::cross(forward, Vec3(0.0f, 1.0f, 0.0f));
            std::vector<std::pair<float, Entity>> angle_sorted;

            for (const auto& c : candidates) {
                if (c.entity == old_target) continue;
                Vec3 to_target = c.target_point - position;
                float h_angle = get_horizontal_angle(forward, right, to_target);
                angle_sorted.push_back({h_angle, c.entity});
            }

            if (angle_sorted.empty()) break;

            // Get current target angle as reference
            float current_angle = 0.0f;
            if (old_target != NullEntity) {
                Vec3 to_current = get_entity_position(world, old_target) - position;
                current_angle = get_horizontal_angle(forward, right, to_current);
            }

            // Find best target in direction
            float best_diff = 999.0f;
            for (const auto& [angle, entity] : angle_sorted) {
                float diff = angle - current_angle;
                // Normalize to -PI to PI
                while (diff > 3.14159f) diff -= 6.28318f;
                while (diff < -3.14159f) diff += 6.28318f;

                bool is_correct_direction =
                    (direction == SwitchDirection::Right && diff > 0.0f) ||
                    (direction == SwitchDirection::Left && diff < 0.0f);

                if (is_correct_direction && std::abs(diff) < best_diff) {
                    best_diff = std::abs(diff);
                    new_target = entity;
                }
            }
            break;
        }

        case SwitchDirection::Up:
        case SwitchDirection::Down: {
            // Sort by vertical position
            float current_y = old_target != NullEntity ?
                              get_entity_position(world, old_target).y : position.y;

            float best_diff = 999.0f;
            for (const auto& c : candidates) {
                if (c.entity == old_target) continue;
                float target_y = c.target_point.y;
                float diff = target_y - current_y;

                bool is_correct_direction =
                    (direction == SwitchDirection::Up && diff > 0.0f) ||
                    (direction == SwitchDirection::Down && diff < 0.0f);

                if (is_correct_direction && std::abs(diff) < best_diff) {
                    best_diff = std::abs(diff);
                    new_target = c.entity;
                }
            }
            break;
        }
    }

    // Apply switch
    if (new_target != NullEntity && new_target != old_target) {
        lock_on(world, targeter, new_target);

        TargetSwitchedEvent event;
        event.targeter = targeter;
        event.old_target = old_target;
        event.new_target = new_target;
        event.direction = direction;
        core::EventDispatcher::instance().dispatch(event);

        return new_target;
    }

    return old_target;
}

Entity TargetingSystem::cycle_target(World& world, Entity targeter, const Vec3& position,
                                      const Vec3& forward, bool next) {
    auto candidates = find_all_targets(world, targeter, position, forward);
    if (candidates.empty()) return NullEntity;

    auto* targeter_comp = world.try_get<TargeterComponent>(targeter);
    if (!targeter_comp) return NullEntity;

    Entity current = targeter_comp->current_target;

    // Find current index
    int current_idx = -1;
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (candidates[i].entity == current) {
            current_idx = static_cast<int>(i);
            break;
        }
    }

    // Calculate new index
    int new_idx;
    if (current_idx < 0) {
        new_idx = 0; // Not found, start at first
    } else if (next) {
        new_idx = (current_idx + 1) % static_cast<int>(candidates.size());
    } else {
        new_idx = (current_idx - 1 + static_cast<int>(candidates.size())) %
                  static_cast<int>(candidates.size());
    }

    Entity new_target = candidates[new_idx].entity;
    if (new_target != current) {
        lock_on(world, targeter, new_target);

        TargetSwitchedEvent event;
        event.targeter = targeter;
        event.old_target = current;
        event.new_target = new_target;
        event.direction = next ? SwitchDirection::Next : SwitchDirection::Previous;
        core::EventDispatcher::instance().dispatch(event);
    }

    return new_target;
}

Entity TargetingSystem::get_soft_lock_target(World& world, Entity targeter) const {
    auto* targeter_comp = world.try_get<TargeterComponent>(targeter);
    return targeter_comp ? targeter_comp->soft_lock_target : NullEntity;
}

Vec3 TargetingSystem::get_aim_assist_direction(
    World& world,
    Entity targeter,
    const Vec3& current_aim_direction,
    float assist_strength
) {
    auto* targeter_comp = world.try_get<TargeterComponent>(targeter);
    if (!targeter_comp || !targeter_comp->soft_lock_enabled) {
        return current_aim_direction;
    }

    // Use hard lock target if available
    Entity target = targeter_comp->current_target;
    if (target == NullEntity) {
        target = targeter_comp->soft_lock_target;
    }

    if (target == NullEntity) {
        return current_aim_direction;
    }

    float strength = assist_strength >= 0.0f ? assist_strength : targeter_comp->soft_lock_strength;
    if (strength <= 0.0f) {
        return current_aim_direction;
    }

    Vec3 targeter_pos = get_entity_position(world, targeter);
    Vec3 target_point = get_target_point(world, targeter);

    Vec3 to_target = glm::normalize(target_point - targeter_pos);

    // Blend between current aim and target direction
    Vec3 assisted = glm::normalize(glm::mix(current_aim_direction, to_target, strength));

    return assisted;
}

Vec3 TargetingSystem::get_target_point(World& world, Entity targeter) const {
    auto* targeter_comp = world.try_get<TargeterComponent>(targeter);
    if (!targeter_comp) return Vec3(0.0f);

    Entity target = targeter_comp->current_target;
    if (target == NullEntity) {
        target = targeter_comp->soft_lock_target;
    }

    if (target == NullEntity) return Vec3(0.0f);

    auto* targetable = world.try_get<TargetableComponent>(target);
    if (targetable) {
        return get_target_world_point(world, target, *targetable);
    }

    return get_entity_position(world, target);
}

bool TargetingSystem::validate_target(World& world, Entity targeter, const Vec3& position, const Vec3& forward) {
    auto* targeter_comp = world.try_get<TargeterComponent>(targeter);
    if (!targeter_comp || targeter_comp->current_target == NullEntity) {
        return false;
    }

    Entity target = targeter_comp->current_target;

    // Check if target still exists
    if (!world.valid(target)) {
        return false;
    }

    // Check if targetable component still enabled
    auto* targetable = world.try_get<TargetableComponent>(target);
    if (!targetable || !targetable->enabled) {
        return false;
    }

    // Check distance
    Vec3 target_pos = get_target_world_point(world, target, *targetable);
    float distance = glm::length(target_pos - position);
    if (distance > targeter_comp->lock_break_distance) {
        return false;
    }

    return true;
}

void TargetingSystem::set_on_target_changed(TargetChangedCallback callback) {
    m_on_target_changed = std::move(callback);
}

void TargetingSystem::set_line_of_sight_check(LineOfSightCheck check) {
    m_line_of_sight_check = std::move(check);
}

TargetCandidate TargetingSystem::evaluate_target(
    World& world,
    Entity targeter,
    Entity target,
    const TargeterComponent& targeter_comp,
    const TargetableComponent& targetable,
    const Vec3& position,
    const Vec3& forward
) {
    TargetCandidate candidate;
    candidate.entity = target;
    candidate.target_point = get_target_world_point(world, target, targetable);

    Vec3 to_target = candidate.target_point - position;
    candidate.distance = glm::length(to_target);

    if (candidate.distance > 0.001f) {
        candidate.angle = angle_between_vectors(forward, to_target);
    } else {
        candidate.angle = 0.0f;
    }

    candidate.in_line_of_sight = true; // Checked later if needed

    return candidate;
}

float TargetingSystem::calculate_score(
    const TargetCandidate& candidate,
    const TargeterComponent& targeter,
    const TargetableComponent& targetable,
    bool is_current_target
) {
    float score = 0.0f;

    // Base priority from targetable
    score += targetable.target_priority * 100.0f;

    // Distance score (closer = better, normalized to 0-50)
    float max_dist = targeter.max_lock_distance;
    float dist_score = 1.0f - (candidate.distance / max_dist);
    score += dist_score * 50.0f;

    // Angle score (centered = better, normalized to 0-30)
    float angle_score = 1.0f - (candidate.angle / targeter.lock_angle);
    score += angle_score * 30.0f;

    // Bonus for current target (stickiness)
    if (is_current_target) {
        score += 20.0f;
    }

    // Target size bonus
    score += targetable.target_size * 10.0f;

    return score;
}

bool TargetingSystem::default_line_of_sight_check(World& world, const Vec3& from, const Vec3& to, Entity exclude) {
    // Default implementation - would typically use physics raycast
    // For now, always return true (no obstacles)
    return true;
}

bool TargetingSystem::is_faction_targetable(const TargeterComponent& targeter, const std::string& faction) const {
    for (const auto& target_faction : targeter.target_factions) {
        if (target_faction == faction) {
            return true;
        }
    }
    return false;
}

Vec3 TargetingSystem::get_target_world_point(World& world, Entity target, const TargetableComponent& targetable) const {
    return transform_offset(targetable.target_point_offset, world, target);
}

void TargetingSystem::notify_target_changed(Entity targeter, Entity old_target, Entity new_target) {
    if (m_on_target_changed) {
        m_on_target_changed(targeter, old_target, new_target);
    }
}

// ============================================================================
// ECS Systems
// ============================================================================

void targeting_system(World& world, double dt) {
    auto& system = TargetingSystem::instance();

    auto view = world.view<TargeterComponent>();

    for (auto entity : view) {
        auto& targeter = view.get<TargeterComponent>(entity);

        // Update switch cooldown
        if (targeter.time_since_switch < targeter.switch_cooldown) {
            targeter.time_since_switch += static_cast<float>(dt);
        }

        // Skip if not locked on
        if (!targeter.lock_on_active || targeter.current_target == NullEntity) {
            continue;
        }

        // Get targeter position and forward
        Vec3 position = get_entity_position(world, entity);
        Vec3 forward = get_entity_forward(world, entity);

        // Validate current target
        if (!system.validate_target(world, entity, position, forward)) {
            // Check reason
            TargetLostReason reason = TargetLostReason::TargetDisabled;

            if (!world.valid(targeter.current_target)) {
                reason = TargetLostReason::TargetDied;
            } else {
                Vec3 target_pos = get_entity_position(world, targeter.current_target);
                float dist = glm::length(target_pos - position);
                if (dist > targeter.lock_break_distance) {
                    reason = TargetLostReason::OutOfRange;
                }
            }

            Entity old_target = targeter.current_target;
            system.unlock(world, entity);

            TargetLostEvent event;
            event.targeter = entity;
            event.previous_target = old_target;
            event.reason = reason;
            core::EventDispatcher::instance().dispatch(event);
        }
    }
}

void soft_lock_system(World& world, double dt) {
    auto view = world.view<TargeterComponent>();

    for (auto entity : view) {
        auto& targeter = view.get<TargeterComponent>(entity);

        // Skip if soft lock disabled or already hard locked
        if (!targeter.soft_lock_enabled) continue;
        if (targeter.lock_on_active && targeter.current_target != NullEntity) {
            targeter.soft_lock_target = NullEntity;
            continue;
        }

        Vec3 position = get_entity_position(world, entity);
        Vec3 forward = get_entity_forward(world, entity);

        // Find best soft lock target in narrower cone
        Entity best_target = NullEntity;
        float best_score = -1.0f;

        auto targetable_view = world.view<TargetableComponent>();
        for (auto target_entity : targetable_view) {
            if (target_entity == entity) continue;

            auto& targetable = targetable_view.get<TargetableComponent>(target_entity);
            if (!targetable.enabled) continue;

            Vec3 target_point = transform_offset(targetable.target_point_offset, world, target_entity);
            Vec3 to_target = target_point - position;
            float distance = glm::length(to_target);

            if (distance > targeter.soft_lock_range) continue;

            float angle = angle_between_vectors(forward, to_target);
            if (angle > targeter.soft_lock_angle) continue;

            // Score based on distance and angle
            float score = (1.0f - distance / targeter.soft_lock_range) * 50.0f +
                          (1.0f - angle / targeter.soft_lock_angle) * 50.0f;

            if (score > best_score) {
                best_score = score;
                best_target = target_entity;
            }
        }

        targeter.soft_lock_target = best_target;
    }
}

void target_indicator_system(World& world, double dt) {
    // Update target indicators based on targeting state
    // This would typically project 3D positions to screen space

    auto view = world.view<TargetableComponent, TargetIndicatorComponent>();

    for (auto entity : view) {
        auto& targetable = view.get<TargetableComponent>(entity);
        auto& indicator = view.get<TargetIndicatorComponent>(entity);

        // Update indicator visibility based on targeting state
        indicator.show_indicator = targetable.is_targeted ||
                                   targetable.show_indicator_when_available;

        // Animation scale (pulse when targeted)
        if (targetable.is_targeted) {
            float pulse = 1.0f + 0.1f * std::sin(static_cast<float>(dt) * 5.0f);
            indicator.indicator_scale = pulse;
        } else {
            indicator.indicator_scale = 1.0f;
        }
    }
}

// ============================================================================
// Component Registration
// ============================================================================

void register_targeting_components() {
    using namespace reflect;

    // TargetableComponent
    TypeRegistry::instance().register_component<TargetableComponent>("TargetableComponent")
        .display_name("Targetable")
        .category("Combat");

    TypeRegistry::instance().register_property<TargetableComponent>("enabled",
        [](const TargetableComponent& c) { return c.enabled; },
        [](TargetableComponent& c, bool v) { c.enabled = v; })
        .display_name("Enabled");

    TypeRegistry::instance().register_property<TargetableComponent>("target_priority",
        [](const TargetableComponent& c) { return c.target_priority; },
        [](TargetableComponent& c, float v) { c.target_priority = v; })
        .display_name("Priority").min(0.0f);

    TypeRegistry::instance().register_property<TargetableComponent>("faction",
        [](const TargetableComponent& c) { return c.faction; },
        [](TargetableComponent& c, const std::string& v) { c.faction = v; })
        .display_name("Faction");

    TypeRegistry::instance().register_property<TargetableComponent>("max_target_distance",
        [](const TargetableComponent& c) { return c.max_target_distance; },
        [](TargetableComponent& c, float v) { c.max_target_distance = v; })
        .display_name("Max Distance").min(1.0f);

    // TargeterComponent
    TypeRegistry::instance().register_component<TargeterComponent>("TargeterComponent")
        .display_name("Targeter")
        .category("Combat");

    TypeRegistry::instance().register_property<TargeterComponent>("max_lock_distance",
        [](const TargeterComponent& c) { return c.max_lock_distance; },
        [](TargeterComponent& c, float v) { c.max_lock_distance = v; })
        .display_name("Max Lock Distance").min(1.0f);

    TypeRegistry::instance().register_property<TargeterComponent>("lock_angle",
        [](const TargeterComponent& c) { return c.lock_angle; },
        [](TargeterComponent& c, float v) { c.lock_angle = v; })
        .display_name("Lock Angle").min(10.0f).max(180.0f);

    TypeRegistry::instance().register_property<TargeterComponent>("soft_lock_enabled",
        [](const TargeterComponent& c) { return c.soft_lock_enabled; },
        [](TargeterComponent& c, bool v) { c.soft_lock_enabled = v; })
        .display_name("Soft Lock Enabled");

    TypeRegistry::instance().register_property<TargeterComponent>("soft_lock_strength",
        [](const TargeterComponent& c) { return c.soft_lock_strength; },
        [](TargeterComponent& c, float v) { c.soft_lock_strength = v; })
        .display_name("Aim Assist Strength").min(0.0f).max(1.0f);

    // TargetIndicatorComponent
    TypeRegistry::instance().register_component<TargetIndicatorComponent>("TargetIndicatorComponent")
        .display_name("Target Indicator")
        .category("Combat/UI");

    TypeRegistry::instance().register_property<TargetIndicatorComponent>("show_indicator",
        [](const TargetIndicatorComponent& c) { return c.show_indicator; },
        [](TargetIndicatorComponent& c, bool v) { c.show_indicator = v; })
        .display_name("Show Indicator");

    TypeRegistry::instance().register_property<TargetIndicatorComponent>("indicator_size",
        [](const TargetIndicatorComponent& c) { return c.indicator_size; },
        [](TargetIndicatorComponent& c, float v) { c.indicator_size = v; })
        .display_name("Indicator Size").min(8.0f);

    core::log(core::LogLevel::Info, "Targeting components registered");
}

} // namespace engine::scene
