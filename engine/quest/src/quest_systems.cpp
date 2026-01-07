#include <engine/quest/quest.h>
#include <engine/quest/quest_manager.hpp>
#include <engine/quest/waypoint.hpp>
#include <engine/quest/quest_components.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/core/event_dispatcher.hpp>
#include <engine/core/log.hpp>
#include <engine/reflect/type_registry.hpp>

namespace engine::quest {

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

bool is_inside_trigger(const Vec3& position, const Vec3& trigger_pos,
                       const QuestTriggerComponent& trigger) {
    if (trigger.shape == QuestTriggerComponent::Shape::Sphere) {
        return glm::length(position - trigger_pos) <= trigger.radius;
    } else {
        Vec3 local = position - trigger_pos;
        return std::abs(local.x) <= trigger.extents.x &&
               std::abs(local.y) <= trigger.extents.y &&
               std::abs(local.z) <= trigger.extents.z;
    }
}

} // anonymous namespace

// ============================================================================
// Quest System - Main update
// ============================================================================

void quest_system(scene::World& world, double dt) {
    QuestManager::instance().update(world, static_cast<float>(dt));
}

// ============================================================================
// Waypoint System
// ============================================================================

void waypoint_system(scene::World& world, double dt) {
    WaypointSystem::instance().update(world, static_cast<float>(dt));
}

// ============================================================================
// Quest Trigger System
// ============================================================================

void quest_trigger_system(scene::World& world, double dt) {
    // Find player entity (would typically be cached or passed in)
    scene::Entity player = scene::NullEntity;

    // Simple player detection - look for entity with QuestLogComponent
    auto log_view = world.view<QuestLogComponent>();
    for (auto entity : log_view) {
        player = entity;
        break;
    }

    if (player == scene::NullEntity) return;

    Vec3 player_pos = get_entity_position(world, player);

    // Check triggers
    auto trigger_view = world.view<QuestTriggerComponent>();

    for (auto trigger_entity : trigger_view) {
        auto& trigger = trigger_view.get<QuestTriggerComponent>(trigger_entity);

        if (trigger.triggered && trigger.one_shot) continue;

        Vec3 trigger_pos = get_entity_position(world, trigger_entity);

        if (!is_inside_trigger(player_pos, trigger_pos, trigger)) continue;

        // Check requirements
        if (!trigger.required_flag.empty()) {
            if (!QuestManager::instance().get_flag(trigger.required_flag)) {
                continue;
            }
        }

        // Process trigger
        bool should_trigger = false;

        switch (trigger.type) {
            case QuestTriggerComponent::TriggerType::StartQuest:
                if (QuestManager::instance().can_start_quest(trigger.quest_id)) {
                    QuestManager::instance().start_quest(trigger.quest_id);
                    should_trigger = true;
                }
                break;

            case QuestTriggerComponent::TriggerType::CompleteObjective:
                if (QuestManager::instance().is_quest_active(trigger.quest_id)) {
                    QuestManager::instance().complete_objective(trigger.quest_id, trigger.objective_id);
                    should_trigger = true;
                }
                break;

            case QuestTriggerComponent::TriggerType::FailObjective:
                if (QuestManager::instance().is_quest_active(trigger.quest_id)) {
                    QuestManager::instance().fail_objective(trigger.quest_id, trigger.objective_id);
                    should_trigger = true;
                }
                break;

            case QuestTriggerComponent::TriggerType::Custom:
                // Custom triggers handled via events
                should_trigger = true;
                break;
        }

        if (should_trigger) {
            trigger.triggered = true;

            if (trigger.show_feedback && !trigger.feedback_message.empty()) {
                // Would emit UI notification event
                core::log(core::LogLevel::Info, "Quest trigger: {}", trigger.feedback_message);
            }
        }
    }

    // Check quest giver interactions (when player is in range)
    auto giver_view = world.view<QuestGiverComponent>();

    for (auto giver_entity : giver_view) {
        auto& giver = giver_view.get<QuestGiverComponent>(giver_entity);

        Vec3 giver_pos = get_entity_position(world, giver_entity);
        float distance = glm::length(player_pos - giver_pos);

        // Update quest giver state based on available quests
        // This is used by the UI to show indicators
        bool has_available = false;
        bool has_turn_in = false;

        for (const auto& quest_id : giver.available_quests) {
            const Quest* quest = QuestManager::instance().get_quest(quest_id);
            if (quest && quest->state == QuestState::Available) {
                has_available = true;
                break;
            }
        }

        for (const auto& quest_id : giver.turn_in_quests) {
            const Quest* quest = QuestManager::instance().get_quest(quest_id);
            if (quest && quest->state == QuestState::Active && quest->all_required_complete()) {
                has_turn_in = true;
                break;
            }
        }

        // Could emit state change events here for UI updates
    }

    // Process kill trackers on death (simplified - would integrate with combat system)
    auto kill_view = world.view<KillTrackerComponent>();

    // Collection items
    auto collect_view = world.view<CollectionItemComponent>();

    for (auto collect_entity : collect_view) {
        auto& collect = collect_view.get<CollectionItemComponent>(collect_entity);

        if (collect.require_interaction) continue;  // Handled by interaction system

        Vec3 item_pos = get_entity_position(world, collect_entity);
        float distance = glm::length(player_pos - item_pos);

        if (distance <= 2.0f) {  // Auto-collect radius
            QuestManager::instance().increment_counter(collect.counter_key, collect.amount);

            if (collect.destroy_on_collect) {
                world.destroy(collect_entity);
            }
        }
    }
}

// ============================================================================
// Component Registration
// ============================================================================

void register_quest_components() {
    using namespace reflect;

    // WaypointComponent
    TypeRegistry::instance().register_component<WaypointComponent>("WaypointComponent",
        TypeMeta().set_display_name("Waypoint").set_category(TypeCategory::Component));

    TypeRegistry::instance().register_property<WaypointComponent, &WaypointComponent::enabled>("enabled",
        PropertyMeta().set_display_name("Enabled"));

    TypeRegistry::instance().register_property<WaypointComponent, &WaypointComponent::label>("label",
        PropertyMeta().set_display_name("Label"));

    TypeRegistry::instance().register_property<WaypointComponent, &WaypointComponent::show_distance>("show_distance",
        PropertyMeta().set_display_name("Show Distance"));

    // QuestTriggerComponent
    TypeRegistry::instance().register_component<QuestTriggerComponent>("QuestTriggerComponent",
        TypeMeta().set_display_name("Quest Trigger").set_category(TypeCategory::Component));

    TypeRegistry::instance().register_property<QuestTriggerComponent, &QuestTriggerComponent::quest_id>("quest_id",
        PropertyMeta().set_display_name("Quest ID"));

    TypeRegistry::instance().register_property<QuestTriggerComponent, &QuestTriggerComponent::objective_id>("objective_id",
        PropertyMeta().set_display_name("Objective ID"));

    TypeRegistry::instance().register_property<QuestTriggerComponent, &QuestTriggerComponent::radius>("radius",
        PropertyMeta().set_display_name("Radius").set_range(0.1f, 1000.0f));

    TypeRegistry::instance().register_property<QuestTriggerComponent, &QuestTriggerComponent::one_shot>("one_shot",
        PropertyMeta().set_display_name("One Shot"));

    // QuestGiverComponent
    TypeRegistry::instance().register_component<QuestGiverComponent>("QuestGiverComponent",
        TypeMeta().set_display_name("Quest Giver").set_category(TypeCategory::Component));

    TypeRegistry::instance().register_property<QuestGiverComponent, &QuestGiverComponent::npc_name>("npc_name",
        PropertyMeta().set_display_name("NPC Name"));

    TypeRegistry::instance().register_property<QuestGiverComponent, &QuestGiverComponent::interaction_range>("interaction_range",
        PropertyMeta().set_display_name("Interaction Range").set_range(0.5f, 100.0f));

    TypeRegistry::instance().register_property<QuestGiverComponent, &QuestGiverComponent::show_indicator>("show_indicator",
        PropertyMeta().set_display_name("Show Indicator"));

    // QuestLogComponent
    TypeRegistry::instance().register_component<QuestLogComponent>("QuestLogComponent",
        TypeMeta().set_display_name("Quest Log").set_category(TypeCategory::Component));

    // QuestParticipantComponent
    TypeRegistry::instance().register_component<QuestParticipantComponent>("QuestParticipantComponent",
        TypeMeta().set_display_name("Quest Participant").set_category(TypeCategory::Component));

    TypeRegistry::instance().register_property<QuestParticipantComponent, &QuestParticipantComponent::quest_id>("quest_id",
        PropertyMeta().set_display_name("Quest ID"));

    TypeRegistry::instance().register_property<QuestParticipantComponent, &QuestParticipantComponent::role>("role",
        PropertyMeta().set_display_name("Role"));

    // KillTrackerComponent
    TypeRegistry::instance().register_component<KillTrackerComponent>("KillTrackerComponent",
        TypeMeta().set_display_name("Kill Tracker").set_category(TypeCategory::Component));

    TypeRegistry::instance().register_property<KillTrackerComponent, &KillTrackerComponent::enemy_type>("enemy_type",
        PropertyMeta().set_display_name("Enemy Type"));

    TypeRegistry::instance().register_property<KillTrackerComponent, &KillTrackerComponent::faction>("faction",
        PropertyMeta().set_display_name("Faction"));

    // CollectionItemComponent
    TypeRegistry::instance().register_component<CollectionItemComponent>("CollectionItemComponent",
        TypeMeta().set_display_name("Collection Item").set_category(TypeCategory::Component));

    TypeRegistry::instance().register_property<CollectionItemComponent, &CollectionItemComponent::counter_key>("counter_key",
        PropertyMeta().set_display_name("Counter Key"));

    TypeRegistry::instance().register_property<CollectionItemComponent, &CollectionItemComponent::amount>("amount",
        PropertyMeta().set_display_name("Amount").set_range(1.0f, 10000.0f));

    // QuestZoneComponent
    TypeRegistry::instance().register_component<QuestZoneComponent>("QuestZoneComponent",
        TypeMeta().set_display_name("Quest Zone").set_category(TypeCategory::Component));

    TypeRegistry::instance().register_property<QuestZoneComponent, &QuestZoneComponent::zone_id>("zone_id",
        PropertyMeta().set_display_name("Zone ID"));

    TypeRegistry::instance().register_property<QuestZoneComponent, &QuestZoneComponent::zone_name>("zone_name",
        PropertyMeta().set_display_name("Zone Name"));

    core::log(core::LogLevel::Info, "Quest components registered");
}

// ============================================================================
// System Registration
// ============================================================================

void register_quest_systems(scene::World& world) {
    core::log(core::LogLevel::Info, "Quest systems ready for registration");
}

} // namespace engine::quest
