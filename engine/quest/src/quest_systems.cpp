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
        return world_transform->get_position();
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
    TypeRegistry::instance().register_component<WaypointComponent>("WaypointComponent")
        .display_name("Waypoint")
        .category("Quest");

    TypeRegistry::instance().register_property<WaypointComponent>("enabled",
        [](const WaypointComponent& c) { return c.enabled; },
        [](WaypointComponent& c, bool v) { c.enabled = v; })
        .display_name("Enabled");

    TypeRegistry::instance().register_property<WaypointComponent>("label",
        [](const WaypointComponent& c) { return c.label; },
        [](WaypointComponent& c, const std::string& v) { c.label = v; })
        .display_name("Label");

    TypeRegistry::instance().register_property<WaypointComponent>("show_distance",
        [](const WaypointComponent& c) { return c.show_distance; },
        [](WaypointComponent& c, bool v) { c.show_distance = v; })
        .display_name("Show Distance");

    // QuestTriggerComponent
    TypeRegistry::instance().register_component<QuestTriggerComponent>("QuestTriggerComponent")
        .display_name("Quest Trigger")
        .category("Quest");

    TypeRegistry::instance().register_property<QuestTriggerComponent>("quest_id",
        [](const QuestTriggerComponent& c) { return c.quest_id; },
        [](QuestTriggerComponent& c, const std::string& v) { c.quest_id = v; })
        .display_name("Quest ID");

    TypeRegistry::instance().register_property<QuestTriggerComponent>("objective_id",
        [](const QuestTriggerComponent& c) { return c.objective_id; },
        [](QuestTriggerComponent& c, const std::string& v) { c.objective_id = v; })
        .display_name("Objective ID");

    TypeRegistry::instance().register_property<QuestTriggerComponent>("radius",
        [](const QuestTriggerComponent& c) { return c.radius; },
        [](QuestTriggerComponent& c, float v) { c.radius = v; })
        .display_name("Radius").min(0.1f);

    TypeRegistry::instance().register_property<QuestTriggerComponent>("one_shot",
        [](const QuestTriggerComponent& c) { return c.one_shot; },
        [](QuestTriggerComponent& c, bool v) { c.one_shot = v; })
        .display_name("One Shot");

    // QuestGiverComponent
    TypeRegistry::instance().register_component<QuestGiverComponent>("QuestGiverComponent")
        .display_name("Quest Giver")
        .category("Quest");

    TypeRegistry::instance().register_property<QuestGiverComponent>("npc_name",
        [](const QuestGiverComponent& c) { return c.npc_name; },
        [](QuestGiverComponent& c, const std::string& v) { c.npc_name = v; })
        .display_name("NPC Name");

    TypeRegistry::instance().register_property<QuestGiverComponent>("interaction_range",
        [](const QuestGiverComponent& c) { return c.interaction_range; },
        [](QuestGiverComponent& c, float v) { c.interaction_range = v; })
        .display_name("Interaction Range").min(0.5f);

    TypeRegistry::instance().register_property<QuestGiverComponent>("show_indicator",
        [](const QuestGiverComponent& c) { return c.show_indicator; },
        [](QuestGiverComponent& c, bool v) { c.show_indicator = v; })
        .display_name("Show Indicator");

    // QuestLogComponent
    TypeRegistry::instance().register_component<QuestLogComponent>("QuestLogComponent")
        .display_name("Quest Log")
        .category("Quest");

    // QuestParticipantComponent
    TypeRegistry::instance().register_component<QuestParticipantComponent>("QuestParticipantComponent")
        .display_name("Quest Participant")
        .category("Quest");

    TypeRegistry::instance().register_property<QuestParticipantComponent>("quest_id",
        [](const QuestParticipantComponent& c) { return c.quest_id; },
        [](QuestParticipantComponent& c, const std::string& v) { c.quest_id = v; })
        .display_name("Quest ID");

    TypeRegistry::instance().register_property<QuestParticipantComponent>("role",
        [](const QuestParticipantComponent& c) { return c.role; },
        [](QuestParticipantComponent& c, const std::string& v) { c.role = v; })
        .display_name("Role");

    // KillTrackerComponent
    TypeRegistry::instance().register_component<KillTrackerComponent>("KillTrackerComponent")
        .display_name("Kill Tracker")
        .category("Quest");

    TypeRegistry::instance().register_property<KillTrackerComponent>("enemy_type",
        [](const KillTrackerComponent& c) { return c.enemy_type; },
        [](KillTrackerComponent& c, const std::string& v) { c.enemy_type = v; })
        .display_name("Enemy Type");

    TypeRegistry::instance().register_property<KillTrackerComponent>("faction",
        [](const KillTrackerComponent& c) { return c.faction; },
        [](KillTrackerComponent& c, const std::string& v) { c.faction = v; })
        .display_name("Faction");

    // CollectionItemComponent
    TypeRegistry::instance().register_component<CollectionItemComponent>("CollectionItemComponent")
        .display_name("Collection Item")
        .category("Quest");

    TypeRegistry::instance().register_property<CollectionItemComponent>("counter_key",
        [](const CollectionItemComponent& c) { return c.counter_key; },
        [](CollectionItemComponent& c, const std::string& v) { c.counter_key = v; })
        .display_name("Counter Key");

    TypeRegistry::instance().register_property<CollectionItemComponent>("amount",
        [](const CollectionItemComponent& c) { return c.amount; },
        [](CollectionItemComponent& c, int32_t v) { c.amount = v; })
        .display_name("Amount").min(1);

    // QuestZoneComponent
    TypeRegistry::instance().register_component<QuestZoneComponent>("QuestZoneComponent")
        .display_name("Quest Zone")
        .category("Quest");

    TypeRegistry::instance().register_property<QuestZoneComponent>("zone_id",
        [](const QuestZoneComponent& c) { return c.zone_id; },
        [](QuestZoneComponent& c, const std::string& v) { c.zone_id = v; })
        .display_name("Zone ID");

    TypeRegistry::instance().register_property<QuestZoneComponent>("zone_name",
        [](const QuestZoneComponent& c) { return c.zone_name; },
        [](QuestZoneComponent& c, const std::string& v) { c.zone_name = v; })
        .display_name("Zone Name");

    core::log(core::LogLevel::Info, "Quest components registered");
}

// ============================================================================
// System Registration
// ============================================================================

void register_quest_systems(scene::World& world) {
    core::log(core::LogLevel::Info, "Quest systems ready for registration");
}

} // namespace engine::quest
