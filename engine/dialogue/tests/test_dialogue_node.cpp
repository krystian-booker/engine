#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/dialogue/dialogue_node.hpp>

using namespace engine::dialogue;
using namespace engine::scene;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// DialogueSpeaker Tests
// ============================================================================

TEST_CASE("DialogueSpeaker defaults", "[dialogue][node]") {
    DialogueSpeaker speaker;

    REQUIRE(speaker.id.empty());
    REQUIRE(speaker.display_name_key.empty());
    REQUIRE(speaker.portrait.empty());
    REQUIRE(speaker.voice_id.empty());
    REQUIRE(speaker.entity == NullEntity);
    REQUIRE_THAT(speaker.name_color.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(speaker.name_color.y, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(speaker.name_color.z, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(speaker.name_color.w, WithinAbs(1.0f, 0.001f));
    REQUIRE(speaker.text_style.empty());
}

TEST_CASE("DialogueSpeaker custom values", "[dialogue][node]") {
    DialogueSpeaker speaker;
    speaker.id = "npc_merchant";
    speaker.display_name_key = "NPC_MERCHANT_NAME";
    speaker.portrait = "portraits/merchant.png";
    speaker.voice_id = "voice_merchant";
    speaker.entity = Entity{42};
    speaker.name_color = Vec4{0.8f, 0.6f, 0.2f, 1.0f};  // Gold color
    speaker.text_style = "fantasy_font";

    REQUIRE(speaker.id == "npc_merchant");
    REQUIRE(speaker.display_name_key == "NPC_MERCHANT_NAME");
    REQUIRE(speaker.portrait == "portraits/merchant.png");
    REQUIRE(speaker.voice_id == "voice_merchant");
    REQUIRE(speaker.entity == Entity{42});
    REQUIRE_THAT(speaker.name_color.x, WithinAbs(0.8f, 0.001f));
    REQUIRE(speaker.text_style == "fantasy_font");
}

// ============================================================================
// DialogueCondition Tests
// ============================================================================

TEST_CASE("DialogueCondition Type enum", "[dialogue][node]") {
    REQUIRE(static_cast<int>(DialogueCondition::Type::Flag) == 0);
    REQUIRE(static_cast<int>(DialogueCondition::Type::Counter) == 1);
    REQUIRE(static_cast<int>(DialogueCondition::Type::QuestState) == 2);
    REQUIRE(static_cast<int>(DialogueCondition::Type::QuestComplete) == 3);
    REQUIRE(static_cast<int>(DialogueCondition::Type::HasItem) == 4);
    REQUIRE(static_cast<int>(DialogueCondition::Type::Reputation) == 5);
    REQUIRE(static_cast<int>(DialogueCondition::Type::Custom) == 6);
}

TEST_CASE("DialogueCondition defaults", "[dialogue][node]") {
    DialogueCondition condition;

    REQUIRE(condition.type == DialogueCondition::Type::Flag);
    REQUIRE(condition.key.empty());
    REQUIRE(condition.compare_op.empty());
    REQUIRE(condition.value == 0);
    REQUIRE(condition.negate == false);
    REQUIRE_FALSE(condition.custom_check);
}

TEST_CASE("DialogueCondition flag check", "[dialogue][node]") {
    DialogueCondition condition;
    condition.type = DialogueCondition::Type::Flag;
    condition.key = "met_merchant";

    REQUIRE(condition.type == DialogueCondition::Type::Flag);
    REQUIRE(condition.key == "met_merchant");
}

TEST_CASE("DialogueCondition counter check", "[dialogue][node]") {
    DialogueCondition condition;
    condition.type = DialogueCondition::Type::Counter;
    condition.key = "gold_donated";
    condition.compare_op = ">=";
    condition.value = 100;

    REQUIRE(condition.type == DialogueCondition::Type::Counter);
    REQUIRE(condition.key == "gold_donated");
    REQUIRE(condition.compare_op == ">=");
    REQUIRE(condition.value == 100);
}

TEST_CASE("DialogueCondition negated", "[dialogue][node]") {
    DialogueCondition condition;
    condition.type = DialogueCondition::Type::Flag;
    condition.key = "villain_alive";
    condition.negate = true;  // Must NOT have this flag

    REQUIRE(condition.negate);
}

// ============================================================================
// DialogueAction Tests
// ============================================================================

TEST_CASE("DialogueAction Type enum", "[dialogue][node]") {
    REQUIRE(static_cast<int>(DialogueAction::Type::SetFlag) == 0);
    REQUIRE(static_cast<int>(DialogueAction::Type::ClearFlag) == 1);
    REQUIRE(static_cast<int>(DialogueAction::Type::IncrementCounter) == 2);
    REQUIRE(static_cast<int>(DialogueAction::Type::SetCounter) == 3);
    REQUIRE(static_cast<int>(DialogueAction::Type::StartQuest) == 4);
    REQUIRE(static_cast<int>(DialogueAction::Type::CompleteObjective) == 5);
    REQUIRE(static_cast<int>(DialogueAction::Type::GiveItem) == 6);
    REQUIRE(static_cast<int>(DialogueAction::Type::TakeItem) == 7);
    REQUIRE(static_cast<int>(DialogueAction::Type::ChangeReputation) == 8);
    REQUIRE(static_cast<int>(DialogueAction::Type::PlaySound) == 9);
    REQUIRE(static_cast<int>(DialogueAction::Type::PlayAnimation) == 10);
    REQUIRE(static_cast<int>(DialogueAction::Type::TriggerEvent) == 11);
    REQUIRE(static_cast<int>(DialogueAction::Type::StartCinematic) == 12);
    REQUIRE(static_cast<int>(DialogueAction::Type::Custom) == 13);
}

TEST_CASE("DialogueAction defaults", "[dialogue][node]") {
    DialogueAction action;

    REQUIRE(action.type == DialogueAction::Type::SetFlag);
    REQUIRE(action.key.empty());
    REQUIRE(action.value.empty());
    REQUIRE(action.amount == 1);
    REQUIRE_FALSE(action.custom_action);
}

TEST_CASE("DialogueAction set flag", "[dialogue][node]") {
    DialogueAction action;
    action.type = DialogueAction::Type::SetFlag;
    action.key = "talked_to_merchant";

    REQUIRE(action.type == DialogueAction::Type::SetFlag);
    REQUIRE(action.key == "talked_to_merchant");
}

TEST_CASE("DialogueAction give item", "[dialogue][node]") {
    DialogueAction action;
    action.type = DialogueAction::Type::GiveItem;
    action.key = "";
    action.value = "healing_potion";
    action.amount = 5;

    REQUIRE(action.type == DialogueAction::Type::GiveItem);
    REQUIRE(action.value == "healing_potion");
    REQUIRE(action.amount == 5);
}

TEST_CASE("DialogueAction start quest", "[dialogue][node]") {
    DialogueAction action;
    action.type = DialogueAction::Type::StartQuest;
    action.key = "merchant_delivery";

    REQUIRE(action.type == DialogueAction::Type::StartQuest);
    REQUIRE(action.key == "merchant_delivery");
}

// ============================================================================
// DialogueChoice Tests
// ============================================================================

TEST_CASE("DialogueChoice defaults", "[dialogue][node]") {
    DialogueChoice choice;

    REQUIRE(choice.id.empty());
    REQUIRE(choice.text_key.empty());
    REQUIRE(choice.target_node_id.empty());
    REQUIRE(choice.conditions.empty());
    REQUIRE(choice.actions.empty());
    REQUIRE(choice.is_highlighted == false);
    REQUIRE(choice.is_exit == false);
    REQUIRE(choice.show_unavailable == false);
    REQUIRE(choice.unavailable_reason_key.empty());
    REQUIRE(choice.skill_check_type.empty());
    REQUIRE(choice.skill_check_value == 0);
    REQUIRE(choice.skill_check_passed == false);
}

TEST_CASE("DialogueChoice simple", "[dialogue][node]") {
    DialogueChoice choice;
    choice.id = "choice_accept";
    choice.text_key = "CHOICE_ACCEPT_QUEST";
    choice.target_node_id = "node_quest_accepted";

    REQUIRE(choice.id == "choice_accept");
    REQUIRE(choice.text_key == "CHOICE_ACCEPT_QUEST");
    REQUIRE(choice.target_node_id == "node_quest_accepted");
}

TEST_CASE("DialogueChoice exit choice", "[dialogue][node]") {
    DialogueChoice choice;
    choice.id = "choice_goodbye";
    choice.text_key = "CHOICE_GOODBYE";
    choice.is_exit = true;

    REQUIRE(choice.is_exit);
    REQUIRE(choice.target_node_id.empty());  // No target needed for exit
}

TEST_CASE("DialogueChoice with skill check", "[dialogue][node]") {
    DialogueChoice choice;
    choice.id = "choice_persuade";
    choice.text_key = "CHOICE_PERSUADE";
    choice.target_node_id = "node_persuaded";
    choice.skill_check_type = "persuasion";
    choice.skill_check_value = 15;

    REQUIRE(choice.skill_check_type == "persuasion");
    REQUIRE(choice.skill_check_value == 15);
    REQUIRE_FALSE(choice.skill_check_passed);
}

TEST_CASE("DialogueChoice highlighted", "[dialogue][node]") {
    DialogueChoice choice;
    choice.id = "choice_important";
    choice.text_key = "CHOICE_IMPORTANT";
    choice.is_highlighted = true;

    REQUIRE(choice.is_highlighted);
}

// ============================================================================
// DialogueNode Tests
// ============================================================================

TEST_CASE("DialogueNode defaults", "[dialogue][node]") {
    DialogueNode node;

    REQUIRE(node.id.empty());
    REQUIRE(node.speaker_id.empty());
    REQUIRE(node.text_key.empty());
    REQUIRE(node.choices.empty());
    REQUIRE(node.on_enter_actions.empty());
    REQUIRE(node.on_exit_actions.empty());
    REQUIRE(node.voice_clip.empty());
    REQUIRE_THAT(node.voice_delay, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(node.auto_advance_delay, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(node.min_display_time, WithinAbs(0.0f, 0.001f));
    REQUIRE(node.speaker_animation.empty());
    REQUIRE(node.speaker_expression.empty());
    REQUIRE(node.camera_shot.empty());
    REQUIRE(node.camera_focus_speaker == false);
    REQUIRE(node.next_node_id.empty());
    REQUIRE(node.is_entry_point == false);
    REQUIRE(node.is_exit_point == false);
    REQUIRE(node.once_only == false);
    REQUIRE(node.shown == false);
}

TEST_CASE("DialogueNode has_choices", "[dialogue][node]") {
    DialogueNode node;

    SECTION("No choices") {
        REQUIRE_FALSE(node.has_choices());
    }

    SECTION("With choices") {
        DialogueChoice choice;
        choice.id = "choice_1";
        node.choices.push_back(choice);
        REQUIRE(node.has_choices());
    }
}

TEST_CASE("DialogueNode has_next", "[dialogue][node]") {
    DialogueNode node;

    SECTION("No next") {
        REQUIRE_FALSE(node.has_next());
    }

    SECTION("With next_node_id") {
        node.next_node_id = "next_node";
        REQUIRE(node.has_next());
    }

    SECTION("With choices") {
        DialogueChoice choice;
        choice.id = "choice_1";
        node.choices.push_back(choice);
        REQUIRE(node.has_next());
    }
}

TEST_CASE("DialogueNode is_terminal", "[dialogue][node]") {
    DialogueNode node;

    SECTION("Terminal - exit point") {
        node.is_exit_point = true;
        REQUIRE(node.is_terminal());
    }

    SECTION("Terminal - no next and no choices") {
        REQUIRE(node.is_terminal());
    }

    SECTION("Not terminal - has next") {
        node.next_node_id = "next_node";
        REQUIRE_FALSE(node.is_terminal());
    }

    SECTION("Not terminal - has choices") {
        DialogueChoice choice;
        choice.id = "choice_1";
        node.choices.push_back(choice);
        REQUIRE_FALSE(node.is_terminal());
    }
}

// ============================================================================
// DialogueNodeBuilder Tests
// ============================================================================

TEST_CASE("DialogueNodeBuilder simple node", "[dialogue][node]") {
    auto node = make_node("node_greeting")
        .speaker("npc_merchant")
        .text("MERCHANT_GREETING")
        .next("node_offer")
        .build();

    REQUIRE(node.id == "node_greeting");
    REQUIRE(node.speaker_id == "npc_merchant");
    REQUIRE(node.text_key == "MERCHANT_GREETING");
    REQUIRE(node.next_node_id == "node_offer");
}

TEST_CASE("DialogueNodeBuilder with choices", "[dialogue][node]") {
    auto node = make_node("node_question")
        .speaker("npc_guard")
        .text("GUARD_QUESTION")
        .choice("choice_yes", "CHOICE_YES", "node_yes")
        .choice("choice_no", "CHOICE_NO", "node_no")
        .exit_choice("choice_leave", "CHOICE_LEAVE")
        .build();

    REQUIRE(node.id == "node_question");
    REQUIRE(node.choices.size() == 3);
    REQUIRE(node.choices[0].id == "choice_yes");
    REQUIRE(node.choices[0].target_node_id == "node_yes");
    REQUIRE(node.choices[1].id == "choice_no");
    REQUIRE(node.choices[2].id == "choice_leave");
    REQUIRE(node.choices[2].is_exit);
}

TEST_CASE("DialogueNodeBuilder with voice", "[dialogue][node]") {
    auto node = make_node("node_voiced")
        .speaker("npc_king")
        .text("KING_SPEECH")
        .voice("audio/king_speech_01.wav")
        .build();

    REQUIRE(node.voice_clip == "audio/king_speech_01.wav");
}

TEST_CASE("DialogueNodeBuilder with animation", "[dialogue][node]") {
    auto node = make_node("node_animated")
        .speaker("npc_bard")
        .text("BARD_SONG")
        .animation("anim_playing_lute")
        .expression("happy")
        .build();

    REQUIRE(node.speaker_animation == "anim_playing_lute");
    REQUIRE(node.speaker_expression == "happy");
}

TEST_CASE("DialogueNodeBuilder with camera", "[dialogue][node]") {
    auto node = make_node("node_dramatic")
        .speaker("npc_villain")
        .text("VILLAIN_REVEAL")
        .camera("shot_closeup_villain")
        .build();

    REQUIRE(node.camera_shot == "shot_closeup_villain");
}

TEST_CASE("DialogueNodeBuilder entry/exit points", "[dialogue][node]") {
    auto entry = make_node("node_start")
        .speaker("narrator")
        .text("INTRO")
        .entry_point()
        .next("node_1")
        .build();

    auto exit = make_node("node_end")
        .speaker("narrator")
        .text("OUTRO")
        .exit_point()
        .build();

    REQUIRE(entry.is_entry_point);
    REQUIRE_FALSE(entry.is_exit_point);
    REQUIRE_FALSE(exit.is_entry_point);
    REQUIRE(exit.is_exit_point);
}

TEST_CASE("DialogueNodeBuilder auto advance", "[dialogue][node]") {
    auto node = make_node("node_auto")
        .speaker("narrator")
        .text("NARRATION")
        .auto_advance(3.0f)
        .build();

    REQUIRE_THAT(node.auto_advance_delay, WithinAbs(3.0f, 0.001f));
}

TEST_CASE("DialogueNodeBuilder once only", "[dialogue][node]") {
    auto node = make_node("node_secret")
        .speaker("npc_sage")
        .text("SAGE_SECRET")
        .once_only()
        .build();

    REQUIRE(node.once_only);
}

// ============================================================================
// DialogueChoiceBuilder Tests
// ============================================================================

TEST_CASE("DialogueChoiceBuilder simple", "[dialogue][node]") {
    auto choice = make_choice("choice_accept")
        .text("ACCEPT")
        .target("node_accepted")
        .build();

    REQUIRE(choice.id == "choice_accept");
    REQUIRE(choice.text_key == "ACCEPT");
    REQUIRE(choice.target_node_id == "node_accepted");
}

TEST_CASE("DialogueChoiceBuilder exit", "[dialogue][node]") {
    auto choice = make_choice("choice_bye")
        .text("GOODBYE")
        .exit()
        .build();

    REQUIRE(choice.is_exit);
}

TEST_CASE("DialogueChoiceBuilder with conditions", "[dialogue][node]") {
    auto choice = make_choice("choice_secret")
        .text("SECRET_OPTION")
        .target("node_secret")
        .requires_flag("found_clue")
        .requires_quest_complete("investigate_murder")
        .build();

    REQUIRE(choice.conditions.size() == 2);
    REQUIRE(choice.conditions[0].type == DialogueCondition::Type::Flag);
    REQUIRE(choice.conditions[0].key == "found_clue");
    REQUIRE(choice.conditions[1].type == DialogueCondition::Type::QuestComplete);
    REQUIRE(choice.conditions[1].key == "investigate_murder");
}

TEST_CASE("DialogueChoiceBuilder with actions", "[dialogue][node]") {
    auto choice = make_choice("choice_accept_quest")
        .text("ACCEPT_QUEST")
        .target("node_quest_started")
        .sets_flag("accepted_merchant_quest")
        .starts_quest("merchant_delivery")
        .build();

    REQUIRE(choice.actions.size() == 2);
    REQUIRE(choice.actions[0].type == DialogueAction::Type::SetFlag);
    REQUIRE(choice.actions[0].key == "accepted_merchant_quest");
    REQUIRE(choice.actions[1].type == DialogueAction::Type::StartQuest);
    REQUIRE(choice.actions[1].key == "merchant_delivery");
}

TEST_CASE("DialogueChoiceBuilder with skill check", "[dialogue][node]") {
    auto choice = make_choice("choice_intimidate")
        .text("INTIMIDATE")
        .target("node_intimidated")
        .skill_check("intimidation", 12)
        .highlighted()
        .build();

    REQUIRE(choice.skill_check_type == "intimidation");
    REQUIRE(choice.skill_check_value == 12);
    REQUIRE(choice.is_highlighted);
}
