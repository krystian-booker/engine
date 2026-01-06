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
        return world_transform->get_position();
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
    TypeRegistry::instance().register_component<DialogueTriggerComponent>("DialogueTriggerComponent")
        .display_name("Dialogue Trigger")
        .category("Dialogue");

    TypeRegistry::instance().register_property<DialogueTriggerComponent>("dialogue_id",
        [](const DialogueTriggerComponent& c) { return c.dialogue_id; },
        [](DialogueTriggerComponent& c, const std::string& v) { c.dialogue_id = v; })
        .display_name("Dialogue ID");

    TypeRegistry::instance().register_property<DialogueTriggerComponent>("interaction_range",
        [](const DialogueTriggerComponent& c) { return c.interaction_range; },
        [](DialogueTriggerComponent& c, float v) { c.interaction_range = v; })
        .display_name("Interaction Range").min(0.5f);

    TypeRegistry::instance().register_property<DialogueTriggerComponent>("require_interaction",
        [](const DialogueTriggerComponent& c) { return c.require_interaction; },
        [](DialogueTriggerComponent& c, bool v) { c.require_interaction = v; })
        .display_name("Require Interaction");

    TypeRegistry::instance().register_property<DialogueTriggerComponent>("enabled",
        [](const DialogueTriggerComponent& c) { return c.enabled; },
        [](DialogueTriggerComponent& c, bool v) { c.enabled = v; })
        .display_name("Enabled");

    // DialogueStateComponent
    TypeRegistry::instance().register_component<DialogueStateComponent>("DialogueStateComponent")
        .display_name("Dialogue State")
        .category("Dialogue");

    TypeRegistry::instance().register_property<DialogueStateComponent>("affinity",
        [](const DialogueStateComponent& c) { return c.affinity; },
        [](DialogueStateComponent& c, int32_t v) { c.affinity = v; })
        .display_name("Affinity");

    TypeRegistry::instance().register_property<DialogueStateComponent>("relationship_level",
        [](const DialogueStateComponent& c) { return c.relationship_level; },
        [](DialogueStateComponent& c, const std::string& v) { c.relationship_level = v; })
        .display_name("Relationship Level");

    // DialogueSpeakerComponent
    TypeRegistry::instance().register_component<DialogueSpeakerComponent>("DialogueSpeakerComponent")
        .display_name("Dialogue Speaker")
        .category("Dialogue");

    TypeRegistry::instance().register_property<DialogueSpeakerComponent>("speaker_id",
        [](const DialogueSpeakerComponent& c) { return c.speaker_id; },
        [](DialogueSpeakerComponent& c, const std::string& v) { c.speaker_id = v; })
        .display_name("Speaker ID");

    TypeRegistry::instance().register_property<DialogueSpeakerComponent>("display_name_key",
        [](const DialogueSpeakerComponent& c) { return c.display_name_key; },
        [](DialogueSpeakerComponent& c, const std::string& v) { c.display_name_key = v; })
        .display_name("Display Name Key");

    TypeRegistry::instance().register_property<DialogueSpeakerComponent>("portrait",
        [](const DialogueSpeakerComponent& c) { return c.portrait; },
        [](DialogueSpeakerComponent& c, const std::string& v) { c.portrait = v; })
        .display_name("Portrait");

    TypeRegistry::instance().register_property<DialogueSpeakerComponent>("face_player_during_dialogue",
        [](const DialogueSpeakerComponent& c) { return c.face_player_during_dialogue; },
        [](DialogueSpeakerComponent& c, bool v) { c.face_player_during_dialogue = v; })
        .display_name("Face Player");

    // DialogueCameraComponent
    TypeRegistry::instance().register_component<DialogueCameraComponent>("DialogueCameraComponent")
        .display_name("Dialogue Camera")
        .category("Dialogue");

    TypeRegistry::instance().register_property<DialogueCameraComponent>("shot_id",
        [](const DialogueCameraComponent& c) { return c.shot_id; },
        [](DialogueCameraComponent& c, const std::string& v) { c.shot_id = v; })
        .display_name("Shot ID");

    TypeRegistry::instance().register_property<DialogueCameraComponent>("transition_time",
        [](const DialogueCameraComponent& c) { return c.transition_time; },
        [](DialogueCameraComponent& c, float v) { c.transition_time = v; })
        .display_name("Transition Time").min(0.0f).max(5.0f);

    TypeRegistry::instance().register_property<DialogueCameraComponent>("enable_dof",
        [](const DialogueCameraComponent& c) { return c.enable_dof; },
        [](DialogueCameraComponent& c, bool v) { c.enable_dof = v; })
        .display_name("Enable DOF");

    // BarksComponent
    TypeRegistry::instance().register_component<BarksComponent>("BarksComponent")
        .display_name("Barks")
        .category("Dialogue");

    TypeRegistry::instance().register_property<BarksComponent>("enabled",
        [](const BarksComponent& c) { return c.enabled; },
        [](BarksComponent& c, bool v) { c.enabled = v; })
        .display_name("Enabled");

    TypeRegistry::instance().register_property<BarksComponent>("bark_range",
        [](const BarksComponent& c) { return c.bark_range; },
        [](BarksComponent& c, float v) { c.bark_range = v; })
        .display_name("Bark Range").min(1.0f);

    TypeRegistry::instance().register_property<BarksComponent>("min_bark_interval",
        [](const BarksComponent& c) { return c.min_bark_interval; },
        [](BarksComponent& c, float v) { c.min_bark_interval = v; })
        .display_name("Min Interval").min(0.0f);

    // SubtitleComponent
    TypeRegistry::instance().register_component<SubtitleComponent>("SubtitleComponent")
        .display_name("Subtitle")
        .category("Dialogue");

    TypeRegistry::instance().register_property<SubtitleComponent>("show_subtitles",
        [](const SubtitleComponent& c) { return c.show_subtitles; },
        [](SubtitleComponent& c, bool v) { c.show_subtitles = v; })
        .display_name("Show Subtitles");

    TypeRegistry::instance().register_property<SubtitleComponent>("show_speaker_name",
        [](const SubtitleComponent& c) { return c.show_speaker_name; },
        [](SubtitleComponent& c, bool v) { c.show_speaker_name = v; })
        .display_name("Show Speaker Name");

    TypeRegistry::instance().register_property<SubtitleComponent>("font_size",
        [](const SubtitleComponent& c) { return c.font_size; },
        [](SubtitleComponent& c, float v) { c.font_size = v; })
        .display_name("Font Size").min(8.0f).max(72.0f);

    core::log(core::LogLevel::Info, "Dialogue components registered");
}

// ============================================================================
// System Registration
// ============================================================================

void register_dialogue_systems(scene::World& world) {
    core::log(core::LogLevel::Info, "Dialogue systems ready for registration");
}

} // namespace engine::dialogue
