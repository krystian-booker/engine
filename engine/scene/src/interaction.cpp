#include <engine/scene/interaction.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/core/log.hpp>
#include <algorithm>
#include <cmath>

namespace engine::scene {

InteractionSystem::InteractionSystem() = default;

InteractionSystem& InteractionSystem::instance() {
    static InteractionSystem s_instance;
    return s_instance;
}

std::optional<InteractionCandidate> InteractionSystem::find_best_interactable(
    World& world,
    const Vec3& position,
    const Vec3& forward,
    float max_distance
) {
    auto candidates = find_all_interactables(world, position, forward, max_distance);
    if (candidates.empty()) {
        return std::nullopt;
    }
    return candidates.front();  // Already sorted by score
}

std::vector<InteractionCandidate> InteractionSystem::find_all_interactables(
    World& world,
    const Vec3& position,
    const Vec3& forward,
    float max_distance
) {
    std::vector<InteractionCandidate> candidates;

    auto view = world.view<InteractableComponent, WorldTransform>();
    for (auto entity : view) {
        const auto& interactable = view.get<InteractableComponent>(entity);

        if (!interactable.enabled) {
            continue;
        }

        auto candidate = evaluate_interactable(world, entity, interactable, position, forward, max_distance);
        if (candidate.entity != NullEntity) {
            candidates.push_back(candidate);
        }
    }

    // Sort by score (descending)
    std::sort(candidates.begin(), candidates.end(),
        [](const InteractionCandidate& a, const InteractionCandidate& b) {
            return a.score > b.score;
        });

    return candidates;
}

std::optional<InteractionCandidate> InteractionSystem::can_interact_with(
    World& world,
    Entity target,
    const Vec3& position,
    const Vec3& forward
) {
    if (!world.valid(target)) {
        return std::nullopt;
    }

    auto* interactable = world.try_get<InteractableComponent>(target);
    if (!interactable || !interactable->enabled) {
        return std::nullopt;
    }

    auto candidate = evaluate_interactable(world, target, *interactable, position, forward,
                                           interactable->interaction_radius);
    if (candidate.entity != NullEntity) {
        return candidate;
    }
    return std::nullopt;
}

InteractionCandidate InteractionSystem::evaluate_interactable(
    World& world,
    Entity entity,
    const InteractableComponent& interactable,
    const Vec3& position,
    const Vec3& forward,
    float max_distance
) {
    InteractionCandidate result;
    result.entity = NullEntity;  // Invalid until proven otherwise

    auto* transform = world.try_get<WorldTransform>(entity);
    if (!transform) {
        return result;
    }

    // Calculate interaction point (entity position + offset)
    Vec3 target_pos = transform->position() + interactable.interaction_offset;

    // Distance check
    Vec3 to_target = target_pos - position;
    float distance = glm::length(to_target);

    float effective_max = std::min(max_distance, interactable.interaction_radius);
    if (distance > effective_max) {
        return result;
    }

    // Angle check (if not 360 degrees)
    float dot = 0.0f;
    float angle = 0.0f;
    if (interactable.interaction_angle < 360.0f && distance > 0.001f) {
        Vec3 direction = glm::normalize(to_target);
        dot = glm::dot(forward, direction);
        angle = glm::degrees(std::acos(std::clamp(dot, -1.0f, 1.0f)));

        float half_angle = interactable.interaction_angle * 0.5f;
        if (angle > half_angle) {
            return result;
        }
    } else {
        dot = 1.0f;  // Within range for all-around interaction
    }

    // Line of sight check
    bool in_los = true;
    if (interactable.requires_line_of_sight) {
        if (m_line_of_sight_check) {
            in_los = m_line_of_sight_check(world, position, target_pos);
        } else {
            in_los = default_line_of_sight_check(world, position, target_pos);
        }
        if (!in_los) {
            return result;
        }
    }

    // Build result
    result.entity = entity;
    result.distance = distance;
    result.angle = angle;
    result.dot_product = dot;
    result.in_line_of_sight = in_los;
    result.interaction_id = interactable.interaction_id;
    result.display_name = interactable.display_name;
    result.type = interactable.type;
    result.hold_to_interact = interactable.hold_to_interact;
    result.hold_duration = interactable.hold_duration;
    result.score = calculate_score(result, interactable);

    return result;
}

float InteractionSystem::calculate_score(const InteractionCandidate& candidate, const InteractableComponent& interactable) {
    // Scoring factors:
    // - Distance (closer = better)
    // - Angle alignment (more aligned = better)
    // - Priority

    float distance_score = 1.0f - (candidate.distance / interactable.interaction_radius);
    float angle_score = (candidate.dot_product + 1.0f) * 0.5f;  // Map -1..1 to 0..1
    float priority_score = static_cast<float>(interactable.priority) * 0.1f;

    // Weighted combination
    return (distance_score * 0.4f) + (angle_score * 0.5f) + priority_score;
}

bool InteractionSystem::default_line_of_sight_check(World& world, const Vec3& from, const Vec3& to) {
    // Default implementation: always visible
    // Games should provide a physics raycast callback for proper occlusion
    (void)world;
    (void)from;
    (void)to;
    return true;
}

void InteractionSystem::begin_hold(Entity interactor, Entity target) {
    m_hold_state.target = target;
    m_hold_state.hold_time = 0.0f;
    m_hold_state.holding = true;

    (void)interactor;  // Could be used for multiplayer context
}

bool InteractionSystem::update_hold(float dt) {
    if (!m_hold_state.holding || m_hold_state.target == NullEntity) {
        return false;
    }

    m_hold_state.hold_time += dt;
    return false;  // Caller should check get_hold_progress() >= 1.0
}

void InteractionSystem::cancel_hold() {
    m_hold_state.holding = false;
    m_hold_state.hold_time = 0.0f;
    m_hold_state.target = NullEntity;
}

float InteractionSystem::get_hold_progress() const {
    if (!m_hold_state.holding || m_hold_state.target == NullEntity) {
        return 0.0f;
    }

    // Need to look up hold duration - for now return time directly
    // In practice, caller would check against the candidate's hold_duration
    return m_hold_state.hold_time;
}

void InteractionSystem::interact(World& world, Entity interactor, Entity target) {
    if (!world.valid(target)) {
        return;
    }

    auto* interactable = world.try_get<InteractableComponent>(target);
    if (!interactable || !interactable->enabled) {
        return;
    }

    core::log(core::LogLevel::Debug, "Interaction: {} interacted with {} ({})",
              static_cast<uint32_t>(interactor),
              static_cast<uint32_t>(target),
              interactable->interaction_id);

    // Disable one-shot interactions
    if (interactable->one_shot) {
        interactable->enabled = false;
    }

    // Fire callback
    if (m_on_interaction) {
        m_on_interaction(interactor, target, interactable->interaction_id);
    }

    // Clear hold state if this was a hold interaction
    if (m_hold_state.target == target) {
        cancel_hold();
    }
}

void InteractionSystem::set_on_interaction(InteractionCallback callback) {
    m_on_interaction = std::move(callback);
}

void InteractionSystem::set_line_of_sight_check(std::function<bool(World&, const Vec3&, const Vec3&)> check) {
    m_line_of_sight_check = std::move(check);
}

// ECS highlight system
void interaction_highlight_system(World& world, double dt) {
    (void)dt;

    // This system would typically:
    // 1. Find the player entity
    // 2. Query for interactables in range
    // 3. Update highlight state on entities with InteractionHighlightComponent

    // For now, this is a placeholder that can be expanded based on game needs
    auto view = world.view<InteractableComponent, InteractionHighlightComponent>();
    for (auto entity : view) {
        auto& highlight = view.get<InteractionHighlightComponent>(entity);
        (void)highlight;
        // Highlight logic would go here - typically involves render state
    }
}

} // namespace engine::scene
