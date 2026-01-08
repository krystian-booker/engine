#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/dialogue/dialogue_components.hpp>

using namespace engine::dialogue;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// DialogueTriggerComponent Tests
// ============================================================================

TEST_CASE("DialogueTriggerComponent defaults", "[dialogue][component]") {
    DialogueTriggerComponent trigger;

    REQUIRE(trigger.dialogue_id.empty());
    REQUIRE_THAT(trigger.interaction_range, WithinAbs(3.0f, 0.001f));
    REQUIRE(trigger.require_interaction == true);
    REQUIRE(trigger.face_player == true);
    REQUIRE(trigger.priority == 0);
    REQUIRE(trigger.required_flags.empty());
    REQUIRE(trigger.excluded_flags.empty());
    REQUIRE(trigger.required_quest.empty());
    REQUIRE(trigger.required_quest_state.empty());
    REQUIRE(trigger.once_per_session == false);
    REQUIRE(trigger.once_ever == false);
    REQUIRE(trigger.triggered == false);
    REQUIRE(trigger.enabled == true);
    REQUIRE(trigger.in_range == false);
}

TEST_CASE("DialogueTriggerComponent custom values", "[dialogue][component]") {
    DialogueTriggerComponent trigger;
    trigger.dialogue_id = "merchant_shop";
    trigger.interaction_range = 5.0f;
    trigger.require_interaction = true;
    trigger.face_player = true;
    trigger.priority = 10;
    trigger.required_flags = {"met_merchant", "shop_unlocked"};
    trigger.excluded_flags = {"merchant_angry"};
    trigger.required_quest = "merchant_intro";
    trigger.required_quest_state = "completed";

    REQUIRE(trigger.dialogue_id == "merchant_shop");
    REQUIRE_THAT(trigger.interaction_range, WithinAbs(5.0f, 0.001f));
    REQUIRE(trigger.priority == 10);
    REQUIRE(trigger.required_flags.size() == 2);
    REQUIRE(trigger.excluded_flags.size() == 1);
    REQUIRE(trigger.required_quest == "merchant_intro");
    REQUIRE(trigger.required_quest_state == "completed");
}

TEST_CASE("DialogueTriggerComponent one-shot", "[dialogue][component]") {
    DialogueTriggerComponent trigger;
    trigger.dialogue_id = "secret_info";
    trigger.once_ever = true;
    trigger.triggered = false;

    REQUIRE(trigger.once_ever);
    REQUIRE_FALSE(trigger.triggered);

    // Simulate triggering
    trigger.triggered = true;
    REQUIRE(trigger.triggered);
}

// ============================================================================
// DialogueStateComponent Tests
// ============================================================================

TEST_CASE("DialogueStateComponent defaults", "[dialogue][component]") {
    DialogueStateComponent state;

    REQUIRE(state.seen_nodes.empty());
    REQUIRE(state.choice_history.empty());
    REQUIRE(state.dialogue_counts.empty());
    REQUIRE(state.state_vars.empty());
    REQUIRE(state.affinity == 0);
    REQUIRE(state.relationship_level.empty());
    REQUIRE_THAT(state.last_dialogue_time, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(state.total_dialogue_time, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("DialogueStateComponent has_seen_node", "[dialogue][component]") {
    DialogueStateComponent state;

    SECTION("Not seen") {
        REQUIRE_FALSE(state.has_seen_node("node_1"));
    }

    SECTION("Seen") {
        state.seen_nodes.push_back("node_1");
        REQUIRE(state.has_seen_node("node_1"));
        REQUIRE_FALSE(state.has_seen_node("node_2"));
    }
}

TEST_CASE("DialogueStateComponent mark_node_seen", "[dialogue][component]") {
    DialogueStateComponent state;

    state.mark_node_seen("node_1");
    REQUIRE(state.seen_nodes.size() == 1);
    REQUIRE(state.has_seen_node("node_1"));

    // Marking again shouldn't duplicate
    state.mark_node_seen("node_1");
    REQUIRE(state.seen_nodes.size() == 1);

    state.mark_node_seen("node_2");
    REQUIRE(state.seen_nodes.size() == 2);
}

TEST_CASE("DialogueStateComponent choice history", "[dialogue][component]") {
    DialogueStateComponent state;

    SECTION("Get empty choice") {
        std::string choice = state.get_choice("node_1");
        REQUIRE(choice.empty());
    }

    SECTION("Set and get choice") {
        state.set_choice("node_question", "choice_yes");
        std::string choice = state.get_choice("node_question");
        REQUIRE(choice == "choice_yes");
    }

    SECTION("Update choice") {
        state.set_choice("node_question", "choice_yes");
        state.set_choice("node_question", "choice_no");
        std::string choice = state.get_choice("node_question");
        REQUIRE(choice == "choice_no");
    }
}

TEST_CASE("DialogueStateComponent dialogue counts", "[dialogue][component]") {
    DialogueStateComponent state;

    SECTION("Get zero count") {
        int count = state.get_dialogue_count("dialogue_1");
        REQUIRE(count == 0);
    }

    SECTION("Increment count") {
        state.increment_dialogue_count("dialogue_1");
        REQUIRE(state.get_dialogue_count("dialogue_1") == 1);

        state.increment_dialogue_count("dialogue_1");
        state.increment_dialogue_count("dialogue_1");
        REQUIRE(state.get_dialogue_count("dialogue_1") == 3);
    }

    SECTION("Multiple dialogues") {
        state.increment_dialogue_count("dialogue_1");
        state.increment_dialogue_count("dialogue_2");
        state.increment_dialogue_count("dialogue_1");

        REQUIRE(state.get_dialogue_count("dialogue_1") == 2);
        REQUIRE(state.get_dialogue_count("dialogue_2") == 1);
    }
}

TEST_CASE("DialogueStateComponent relationship", "[dialogue][component]") {
    DialogueStateComponent state;
    state.affinity = 50;
    state.relationship_level = "friend";

    REQUIRE(state.affinity == 50);
    REQUIRE(state.relationship_level == "friend");
}

// ============================================================================
// DialogueSpeakerComponent Tests
// ============================================================================

TEST_CASE("DialogueSpeakerComponent defaults", "[dialogue][component]") {
    DialogueSpeakerComponent speaker;

    REQUIRE(speaker.speaker_id.empty());
    REQUIRE(speaker.display_name_key.empty());
    REQUIRE(speaker.portrait.empty());
    REQUIRE(speaker.voice_bank.empty());
    REQUIRE(speaker.face_player_during_dialogue == true);
    REQUIRE(speaker.stop_movement_during_dialogue == true);
    REQUIRE(speaker.idle_animation.empty());
    REQUIRE(speaker.talk_animation.empty());
    REQUIRE_THAT(speaker.voice_pitch, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(speaker.voice_volume, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("DialogueSpeakerComponent custom values", "[dialogue][component]") {
    DialogueSpeakerComponent speaker;
    speaker.speaker_id = "npc_merchant";
    speaker.display_name_key = "MERCHANT_NAME";
    speaker.portrait = "portraits/merchant.png";
    speaker.voice_bank = "voices/merchant";
    speaker.face_player_during_dialogue = true;
    speaker.stop_movement_during_dialogue = true;
    speaker.idle_animation = "anim_idle";
    speaker.talk_animation = "anim_talk";
    speaker.voice_pitch = 1.2f;
    speaker.voice_volume = 0.8f;

    REQUIRE(speaker.speaker_id == "npc_merchant");
    REQUIRE(speaker.display_name_key == "MERCHANT_NAME");
    REQUIRE(speaker.portrait == "portraits/merchant.png");
    REQUIRE(speaker.voice_bank == "voices/merchant");
    REQUIRE(speaker.idle_animation == "anim_idle");
    REQUIRE(speaker.talk_animation == "anim_talk");
    REQUIRE_THAT(speaker.voice_pitch, WithinAbs(1.2f, 0.001f));
    REQUIRE_THAT(speaker.voice_volume, WithinAbs(0.8f, 0.001f));
}

// ============================================================================
// DialogueCameraComponent Tests
// ============================================================================

TEST_CASE("DialogueCameraComponent ShotType enum", "[dialogue][component]") {
    using ShotType = DialogueCameraComponent::ShotType;

    REQUIRE(static_cast<int>(ShotType::CloseUp) == 0);
    REQUIRE(static_cast<int>(ShotType::MediumShot) == 1);
    REQUIRE(static_cast<int>(ShotType::WideShot) == 2);
    REQUIRE(static_cast<int>(ShotType::OverShoulder) == 3);
    REQUIRE(static_cast<int>(ShotType::TwoShot) == 4);
    REQUIRE(static_cast<int>(ShotType::Custom) == 5);
}

TEST_CASE("DialogueCameraComponent defaults", "[dialogue][component]") {
    DialogueCameraComponent camera;

    REQUIRE(camera.shot_id.empty());
    REQUIRE(camera.shot_type == DialogueCameraComponent::ShotType::MediumShot);
    REQUIRE_THAT(camera.position_offset.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(camera.position_offset.y, WithinAbs(1.5f, 0.001f));
    REQUIRE_THAT(camera.position_offset.z, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(camera.look_at_offset.y, WithinAbs(1.5f, 0.001f));
    REQUIRE_THAT(camera.transition_time, WithinAbs(0.5f, 0.001f));
    REQUIRE(camera.smooth_transition == true);
    REQUIRE(camera.enable_dof == true);
    REQUIRE_THAT(camera.focus_distance, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(camera.aperture, WithinAbs(2.8f, 0.001f));
}

TEST_CASE("DialogueCameraComponent custom shot", "[dialogue][component]") {
    DialogueCameraComponent camera;
    camera.shot_id = "dramatic_reveal";
    camera.shot_type = DialogueCameraComponent::ShotType::CloseUp;
    camera.position_offset = Vec3{0.5f, 1.7f, 1.0f};
    camera.look_at_offset = Vec3{0.0f, 1.7f, 0.0f};
    camera.transition_time = 1.0f;
    camera.enable_dof = true;
    camera.focus_distance = 1.0f;
    camera.aperture = 1.4f;

    REQUIRE(camera.shot_id == "dramatic_reveal");
    REQUIRE(camera.shot_type == DialogueCameraComponent::ShotType::CloseUp);
    REQUIRE_THAT(camera.position_offset.x, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(camera.transition_time, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(camera.aperture, WithinAbs(1.4f, 0.001f));
}

// ============================================================================
// BarksComponent Tests
// ============================================================================

TEST_CASE("BarksComponent Bark defaults", "[dialogue][component]") {
    BarksComponent::Bark bark;

    REQUIRE(bark.id.empty());
    REQUIRE(bark.text_key.empty());
    REQUIRE(bark.voice_clip.empty());
    REQUIRE_THAT(bark.cooldown, WithinAbs(30.0f, 0.001f));
    REQUIRE_THAT(bark.last_played, WithinAbs(-1000.0f, 0.001f));
    REQUIRE(bark.required_flags.empty());
    REQUIRE_THAT(bark.trigger_chance, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("BarksComponent defaults", "[dialogue][component]") {
    BarksComponent barks;

    REQUIRE(barks.idle_barks.empty());
    REQUIRE(barks.combat_barks.empty());
    REQUIRE(barks.alert_barks.empty());
    REQUIRE(barks.damage_barks.empty());
    REQUIRE(barks.death_barks.empty());
    REQUIRE(barks.greeting_barks.empty());
    REQUIRE(barks.reaction_barks.empty());
    REQUIRE(barks.enabled == true);
    REQUIRE_THAT(barks.bark_range, WithinAbs(15.0f, 0.001f));
    REQUIRE_THAT(barks.min_bark_interval, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(barks.last_bark_time, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("BarksComponent with barks", "[dialogue][component]") {
    BarksComponent barks;

    BarksComponent::Bark greeting;
    greeting.id = "greeting_1";
    greeting.text_key = "BARK_GREETING_1";
    greeting.voice_clip = "audio/bark_greeting_1.wav";
    greeting.cooldown = 60.0f;
    greeting.trigger_chance = 0.5f;

    barks.greeting_barks.push_back(greeting);

    BarksComponent::Bark combat;
    combat.id = "combat_1";
    combat.text_key = "BARK_COMBAT_1";
    combat.voice_clip = "audio/bark_combat_1.wav";
    combat.cooldown = 10.0f;

    barks.combat_barks.push_back(combat);

    REQUIRE(barks.greeting_barks.size() == 1);
    REQUIRE(barks.greeting_barks[0].id == "greeting_1");
    REQUIRE_THAT(barks.greeting_barks[0].trigger_chance, WithinAbs(0.5f, 0.001f));
    REQUIRE(barks.combat_barks.size() == 1);
    REQUIRE(barks.combat_barks[0].id == "combat_1");
}

// ============================================================================
// SubtitleComponent Tests
// ============================================================================

TEST_CASE("SubtitleComponent defaults", "[dialogue][component]") {
    SubtitleComponent subtitle;

    REQUIRE(subtitle.show_subtitles == true);
    REQUIRE(subtitle.show_speaker_name == true);
    REQUIRE(subtitle.font_style.empty());
    REQUIRE_THAT(subtitle.font_size, WithinAbs(24.0f, 0.001f));
    REQUIRE_THAT(subtitle.text_color.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(subtitle.text_color.y, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(subtitle.text_color.z, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(subtitle.text_color.w, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(subtitle.background_color.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(subtitle.background_color.w, WithinAbs(0.7f, 0.001f));
    REQUIRE_THAT(subtitle.screen_position.x, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(subtitle.screen_position.y, WithinAbs(0.9f, 0.001f));
    REQUIRE_THAT(subtitle.max_width, WithinAbs(0.8f, 0.001f));
    REQUIRE_THAT(subtitle.min_display_time, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(subtitle.chars_per_second, WithinAbs(15.0f, 0.001f));
}

TEST_CASE("SubtitleComponent custom style", "[dialogue][component]") {
    SubtitleComponent subtitle;
    subtitle.show_subtitles = true;
    subtitle.show_speaker_name = true;
    subtitle.font_style = "fantasy_font";
    subtitle.font_size = 28.0f;
    subtitle.text_color = Vec4{1.0f, 0.9f, 0.7f, 1.0f};  // Warm white
    subtitle.background_color = Vec4{0.1f, 0.1f, 0.1f, 0.8f};
    subtitle.screen_position = Vec2{0.5f, 0.85f};
    subtitle.max_width = 0.7f;

    REQUIRE(subtitle.font_style == "fantasy_font");
    REQUIRE_THAT(subtitle.font_size, WithinAbs(28.0f, 0.001f));
    REQUIRE_THAT(subtitle.text_color.y, WithinAbs(0.9f, 0.001f));
    REQUIRE_THAT(subtitle.screen_position.y, WithinAbs(0.85f, 0.001f));
    REQUIRE_THAT(subtitle.max_width, WithinAbs(0.7f, 0.001f));
}
