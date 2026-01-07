#include <engine/dialogue/dialogue.hpp>
#include <engine/dialogue/dialogue_player.hpp>
#include <engine/dialogue/dialogue_components.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/core/event_dispatcher.hpp>
#include <engine/core/log.hpp>
#include <engine/reflect/type_registry.hpp>
#include <random>

namespace engine::dialogue {

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

float random_float() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(gen);
}

} // anonymous namespace

// ============================================================================
// Dialogue System - Main update
// ============================================================================

void dialogue_system(scene::World& world, double dt) {
    DialoguePlayer::instance().update(static_cast<float>(dt));
}

// ============================================================================
// Dialogue Trigger System
// ============================================================================

void dialogue_trigger_system(scene::World& world, double dt) {
    // Skip if dialogue is already active
    if (DialoguePlayer::instance().is_active()) return;

    // Find player entity (would typically be cached)
    scene::Entity player = scene::NullEntity;
    // Look for entity that initiated dialogue last or has player tag
    // For now, just use the initiator stored in player component or similar

    auto trigger_view = world.view<DialogueTriggerComponent>();

    DialogueTriggerComponent* best_trigger = nullptr;
    scene::Entity best_entity = scene::NullEntity;
    float best_distance = std::numeric_limits<float>::max();

    for (auto trigger_entity : trigger_view) {
        auto& trigger = trigger_view.get<DialogueTriggerComponent>(trigger_entity);

        if (!trigger.enabled) continue;
        if (trigger.once_ever && trigger.triggered) continue;

        // Check conditions
        // Would check required_flags, excluded_flags, quest state etc.

        // For now, just track which triggers are in range
        // The actual player position check would happen here

        // Simplified: just mark as best if this is the first one
        if (!best_trigger) {
            best_trigger = &trigger;
            best_entity = trigger_entity;
        }
    }

    // Auto-start dialogues that don't require interaction
    // Would check distance and !require_interaction here
}

// ============================================================================
// Barks System
// ============================================================================

void barks_system(scene::World& world, double dt) {
    float current_time = 0.0f;  // Would use actual game time

    auto view = world.view<BarksComponent>();

    for (auto entity : view) {
        auto& barks = view.get<BarksComponent>(entity);

        if (!barks.enabled) continue;

        // Check cooldown
        if (current_time - barks.last_bark_time < barks.min_bark_interval) {
            continue;
        }

        // Select bark category based on state
        // Would check combat, alert, etc. states

        // For idle barks
        if (barks.idle_barks.empty()) continue;

        // Find eligible bark
        for (auto& bark : barks.idle_barks) {
            if (current_time - bark.last_played < bark.cooldown) continue;

            // Check conditions (flags)
            // Check trigger chance
            if (random_float() > bark.trigger_chance) continue;

            // Play bark
            // Would play voice clip and/or show subtitle
            bark.last_played = current_time;
            barks.last_bark_time = current_time;

            core::log(core::LogLevel::Debug, "Bark played: {}", bark.id);
            break;
        }
    }
}

// ============================================================================
// Component Registration
// ============================================================================

void register_dialogue_components() {
    using namespace reflect;

    // DialogueTriggerComponent
    TypeRegistry::instance().register_component<DialogueTriggerComponent>("DialogueTriggerComponent",
        TypeMeta().set_display_name("Dialogue Trigger").set_category(TypeCategory::Component));

    TypeRegistry::instance().register_property<DialogueTriggerComponent, &DialogueTriggerComponent::dialogue_id>("dialogue_id",
        PropertyMeta().set_display_name("Dialogue ID"));

    TypeRegistry::instance().register_property<DialogueTriggerComponent, &DialogueTriggerComponent::interaction_range>("interaction_range",
        PropertyMeta().set_display_name("Interaction Range").set_range(0.5f, 1000.0f));

    TypeRegistry::instance().register_property<DialogueTriggerComponent, &DialogueTriggerComponent::require_interaction>("require_interaction",
        PropertyMeta().set_display_name("Require Interaction"));

    TypeRegistry::instance().register_property<DialogueTriggerComponent, &DialogueTriggerComponent::enabled>("enabled",
        PropertyMeta().set_display_name("Enabled"));

    // DialogueStateComponent
    TypeRegistry::instance().register_component<DialogueStateComponent>("DialogueStateComponent",
        TypeMeta().set_display_name("Dialogue State").set_category(TypeCategory::Component));

    TypeRegistry::instance().register_property<DialogueStateComponent, &DialogueStateComponent::affinity>("affinity",
        PropertyMeta().set_display_name("Affinity"));

    TypeRegistry::instance().register_property<DialogueStateComponent, &DialogueStateComponent::relationship_level>("relationship_level",
        PropertyMeta().set_display_name("Relationship Level"));

    // DialogueSpeakerComponent
    TypeRegistry::instance().register_component<DialogueSpeakerComponent>("DialogueSpeakerComponent",
        TypeMeta().set_display_name("Dialogue Speaker").set_category(TypeCategory::Component));

    TypeRegistry::instance().register_property<DialogueSpeakerComponent, &DialogueSpeakerComponent::speaker_id>("speaker_id",
        PropertyMeta().set_display_name("Speaker ID"));

    TypeRegistry::instance().register_property<DialogueSpeakerComponent, &DialogueSpeakerComponent::display_name_key>("display_name_key",
        PropertyMeta().set_display_name("Display Name Key"));

    TypeRegistry::instance().register_property<DialogueSpeakerComponent, &DialogueSpeakerComponent::portrait>("portrait",
        PropertyMeta().set_display_name("Portrait"));

    TypeRegistry::instance().register_property<DialogueSpeakerComponent, &DialogueSpeakerComponent::face_player_during_dialogue>("face_player_during_dialogue",
        PropertyMeta().set_display_name("Face Player"));

    // DialogueCameraComponent
    TypeRegistry::instance().register_component<DialogueCameraComponent>("DialogueCameraComponent",
        TypeMeta().set_display_name("Dialogue Camera").set_category(TypeCategory::Component));

    TypeRegistry::instance().register_property<DialogueCameraComponent, &DialogueCameraComponent::shot_id>("shot_id",
        PropertyMeta().set_display_name("Shot ID"));

    TypeRegistry::instance().register_property<DialogueCameraComponent, &DialogueCameraComponent::transition_time>("transition_time",
        PropertyMeta().set_display_name("Transition Time").set_range(0.0f, 5.0f));

    TypeRegistry::instance().register_property<DialogueCameraComponent, &DialogueCameraComponent::enable_dof>("enable_dof",
        PropertyMeta().set_display_name("Enable DOF"));

    // BarksComponent
    TypeRegistry::instance().register_component<BarksComponent>("BarksComponent",
        TypeMeta().set_display_name("Barks").set_category(TypeCategory::Component));

    TypeRegistry::instance().register_property<BarksComponent, &BarksComponent::enabled>("enabled",
        PropertyMeta().set_display_name("Enabled"));

    TypeRegistry::instance().register_property<BarksComponent, &BarksComponent::bark_range>("bark_range",
        PropertyMeta().set_display_name("Bark Range").set_range(1.0f, 1000.0f));

    TypeRegistry::instance().register_property<BarksComponent, &BarksComponent::min_bark_interval>("min_bark_interval",
        PropertyMeta().set_display_name("Min Interval").set_range(0.0f, 300.0f));

    // SubtitleComponent
    TypeRegistry::instance().register_component<SubtitleComponent>("SubtitleComponent",
        TypeMeta().set_display_name("Subtitle").set_category(TypeCategory::Component));

    TypeRegistry::instance().register_property<SubtitleComponent, &SubtitleComponent::show_subtitles>("show_subtitles",
        PropertyMeta().set_display_name("Show Subtitles"));

    TypeRegistry::instance().register_property<SubtitleComponent, &SubtitleComponent::show_speaker_name>("show_speaker_name",
        PropertyMeta().set_display_name("Show Speaker Name"));

    TypeRegistry::instance().register_property<SubtitleComponent, &SubtitleComponent::font_size>("font_size",
        PropertyMeta().set_display_name("Font Size").set_range(8.0f, 72.0f));

    core::log(core::LogLevel::Info, "Dialogue components registered");
}

// ============================================================================
// System Registration
// ============================================================================

void register_dialogue_systems(scene::World& world) {
    core::log(core::LogLevel::Info, "Dialogue systems ready for registration");
}

} // namespace engine::dialogue
