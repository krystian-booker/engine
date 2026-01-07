#include <engine/ai/ai.hpp>
#include <engine/ai/ai_components.hpp>
#include <engine/ai/perception.hpp>
#include <engine/ai/blackboard.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/core/event_dispatcher.hpp>
#include <engine/core/log.hpp>
#include <engine/reflect/type_registry.hpp>
#include <random>

namespace engine::ai {

namespace {

Vec3 get_entity_position(scene::World& world, scene::Entity entity) {
    auto* world_transform = world.try_get<scene::WorldTransform>(entity);
    if (world_transform) {
        return world_transform->position();
    }
    auto* local_transform = world.try_get<scene::LocalTransform>(entity);
    if (local_transform) {
        return local_transform->position;
    }
    return Vec3(0.0f);
}

float random_range(float min, float max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(min, max);
    return dist(gen);
}

} // anonymous namespace

// ============================================================================
// AI Behavior System
// ============================================================================

void ai_behavior_system(scene::World& world, double dt) {
    auto view = world.view<AIControllerComponent>();

    for (auto entity : view) {
        auto& controller = view.get<AIControllerComponent>(entity);

        if (!controller.enabled) continue;

        // Check if we should update this frame
        if (!controller.should_update(static_cast<float>(dt))) continue;

        // Ensure blackboard exists
        controller.ensure_blackboard();

        // Update blackboard with current state
        Vec3 self_pos = get_entity_position(world, entity);
        controller.blackboard->set_position(bb::SELF_POSITION, self_pos);

        // Update perception data in blackboard
        auto* perception = world.try_get<AIPerceptionComponent>(entity);
        if (perception) {
            scene::Entity threat = perception->get_primary_threat();
            if (threat != scene::NullEntity) {
                controller.blackboard->set_entity(bb::TARGET_ENTITY, threat);
                controller.blackboard->set_bool(bb::CAN_SEE_TARGET, perception->can_see(threat));

                auto last_known = perception->get_last_known_position(threat);
                if (last_known) {
                    controller.blackboard->set_position(bb::TARGET_POSITION, *last_known);
                    controller.blackboard->set_position(bb::LAST_KNOWN_POSITION, *last_known);
                    controller.blackboard->set_float(bb::TARGET_DISTANCE,
                        glm::length(*last_known - self_pos));
                }

                controller.blackboard->set_float(bb::TIME_SINCE_SEEN,
                    perception->get_awareness_of(threat) > 0.8f ? 0.0f : 99.0f);
            } else {
                controller.blackboard->set_entity(bb::TARGET_ENTITY, scene::NullEntity);
                controller.blackboard->set_bool(bb::CAN_SEE_TARGET, false);
            }

            controller.blackboard->set_bool(bb::IS_ALERTED, perception->has_threat());
        }

        // Tick behavior tree
        if (controller.behavior_tree) {
            BTContext ctx;
            ctx.world = &world;
            ctx.entity = entity;
            ctx.blackboard = controller.blackboard.get();
            ctx.delta_time = controller.update_interval;

            controller.last_status = controller.behavior_tree->tick(ctx);
        }
    }
}

// ============================================================================
// AI Combat System
// ============================================================================

void ai_combat_system(scene::World& world, double dt) {
    auto view = world.view<AICombatComponent>();

    for (auto entity : view) {
        auto& combat = view.get<AICombatComponent>(entity);

        // Update attack cooldown
        combat.time_since_attack += static_cast<float>(dt);

        // Get controller for target info
        auto* controller = world.try_get<AIControllerComponent>(entity);
        if (controller && controller->blackboard) {
            scene::Entity target = controller->blackboard->get_entity(bb::TARGET_ENTITY);
            if (target != combat.threat) {
                // Target changed
                scene::Entity old_threat = combat.threat;
                combat.threat = target;

                AITargetChangedEvent event;
                event.entity = entity;
                event.old_target = old_threat;
                event.new_target = target;
                core::events().dispatch(event);
            }

            // Update combat state based on distance
            if (combat.threat != scene::NullEntity) {
                float distance = controller->blackboard->get_float(bb::TARGET_DISTANCE, 999.0f);

                // Update blackboard with combat info
                controller->blackboard->set_bool(bb::IN_ATTACK_RANGE, combat.in_attack_range(distance));
                controller->blackboard->set_bool(bb::CAN_ATTACK, combat.can_attack());
            }
        }
    }
}

// ============================================================================
// AI Patrol System
// ============================================================================

void ai_patrol_system(scene::World& world, double dt) {
    auto view = world.view<AIPatrolComponent>();

    for (auto entity : view) {
        auto& patrol = view.get<AIPatrolComponent>(entity);

        if (!patrol.patrol_active || patrol.waypoints.empty()) continue;
        if (patrol.type == AIPatrolComponent::PatrolType::None) continue;

        // Check if AI is alerted (pause patrol)
        auto* controller = world.try_get<AIControllerComponent>(entity);
        if (controller && controller->blackboard) {
            if (controller->blackboard->get_bool(bb::IS_ALERTED, false)) {
                continue; // Don't patrol while alerted
            }
        }

        Vec3 current_pos = get_entity_position(world, entity);
        Vec3 target_pos = patrol.get_current_waypoint();

        if (patrol.is_waiting) {
            // Waiting at waypoint
            patrol.time_at_waypoint += static_cast<float>(dt);
            if (patrol.time_at_waypoint >= patrol.current_wait_time) {
                patrol.is_waiting = false;
                patrol.advance_waypoint();
            }
        } else {
            // Moving to waypoint
            float distance = glm::length(target_pos - current_pos);

            if (distance <= patrol.arrival_distance) {
                // Arrived at waypoint
                patrol.is_waiting = true;
                patrol.time_at_waypoint = 0.0f;
                patrol.current_wait_time = random_range(patrol.wait_time_min, patrol.wait_time_max);
            } else {
                // Set move target in blackboard
                if (controller && controller->blackboard) {
                    controller->blackboard->set_position(bb::MOVE_TARGET, target_pos);
                    controller->blackboard->set_float(bb::MOVE_SPEED, patrol.patrol_speed);
                }
            }
        }
    }
}

// ============================================================================
// Component Registration
// ============================================================================

void register_ai_components() {
    using namespace reflect;

    // AIControllerComponent
    TypeRegistry::instance().register_component<AIControllerComponent>("AIControllerComponent",
        TypeMeta().set_display_name("AI Controller").set_category(TypeCategory::Component));

    TypeRegistry::instance().register_property<AIControllerComponent, &AIControllerComponent::enabled>("enabled",
        PropertyMeta().set_display_name("Enabled"));

    TypeRegistry::instance().register_property<AIControllerComponent, &AIControllerComponent::update_interval>("update_interval",
        PropertyMeta().set_display_name("Update Interval").set_range(0.01f, 1.0f));

    // AICombatComponent
    TypeRegistry::instance().register_component<AICombatComponent>("AICombatComponent",
        TypeMeta().set_display_name("AI Combat").set_category(TypeCategory::Component));

    TypeRegistry::instance().register_property<AICombatComponent, &AICombatComponent::attack_range>("attack_range",
        PropertyMeta().set_display_name("Attack Range").set_range(0.5f, 100.0f));

    TypeRegistry::instance().register_property<AICombatComponent, &AICombatComponent::attack_cooldown>("attack_cooldown",
        PropertyMeta().set_display_name("Attack Cooldown").set_range(0.1f, 10.0f));

    TypeRegistry::instance().register_property<AICombatComponent, &AICombatComponent::aggression>("aggression",
        PropertyMeta().set_display_name("Aggression").set_range(0.0f, 1.0f));

    // AIPatrolComponent
    TypeRegistry::instance().register_component<AIPatrolComponent>("AIPatrolComponent",
        TypeMeta().set_display_name("AI Patrol").set_category(TypeCategory::Component));

    TypeRegistry::instance().register_property<AIPatrolComponent, &AIPatrolComponent::patrol_speed>("patrol_speed",
        PropertyMeta().set_display_name("Patrol Speed").set_range(0.1f, 10.0f));

    // Register perception components
    register_perception_components();

    core::log(core::LogLevel::Info, "AI components registered");
}

void register_ai_systems(scene::World& world) {
    core::log(core::LogLevel::Info, "AI systems ready for registration");
}

} // namespace engine::ai
