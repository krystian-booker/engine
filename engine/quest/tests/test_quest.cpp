#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/quest/quest.hpp>

using namespace engine::quest;
using namespace engine::scene;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// QuestState Tests
// ============================================================================

TEST_CASE("QuestState enum", "[quest]") {
    REQUIRE(static_cast<int>(QuestState::Unavailable) == 0);
    REQUIRE(static_cast<int>(QuestState::Available) == 1);
    REQUIRE(static_cast<int>(QuestState::Active) == 2);
    REQUIRE(static_cast<int>(QuestState::Completed) == 3);
    REQUIRE(static_cast<int>(QuestState::Failed) == 4);
    REQUIRE(static_cast<int>(QuestState::Abandoned) == 5);
}

// ============================================================================
// QuestCategory Tests
// ============================================================================

TEST_CASE("QuestCategory enum", "[quest]") {
    REQUIRE(static_cast<int>(QuestCategory::Main) == 0);
    REQUIRE(static_cast<int>(QuestCategory::Side) == 1);
    REQUIRE(static_cast<int>(QuestCategory::Faction) == 2);
    REQUIRE(static_cast<int>(QuestCategory::Bounty) == 3);
    REQUIRE(static_cast<int>(QuestCategory::Collection) == 4);
    REQUIRE(static_cast<int>(QuestCategory::Exploration) == 5);
    REQUIRE(static_cast<int>(QuestCategory::Tutorial) == 6);
}

// ============================================================================
// QuestReward Tests
// ============================================================================

TEST_CASE("QuestReward defaults", "[quest]") {
    QuestReward reward;

    REQUIRE(reward.type.empty());
    REQUIRE(reward.value.empty());
    REQUIRE(reward.amount == 1);
    REQUIRE(reward.display_name.empty());
    REQUIRE(reward.icon.empty());
}

TEST_CASE("QuestReward experience", "[quest]") {
    QuestReward reward;
    reward.type = "experience";
    reward.value = "";
    reward.amount = 1000;
    reward.display_name = "1000 XP";

    REQUIRE(reward.type == "experience");
    REQUIRE(reward.amount == 1000);
    REQUIRE(reward.display_name == "1000 XP");
}

TEST_CASE("QuestReward item", "[quest]") {
    QuestReward reward;
    reward.type = "item";
    reward.value = "legendary_sword";
    reward.amount = 1;
    reward.display_name = "Legendary Sword";
    reward.icon = "icons/weapons/legendary_sword.png";

    REQUIRE(reward.type == "item");
    REQUIRE(reward.value == "legendary_sword");
    REQUIRE(reward.amount == 1);
    REQUIRE(reward.display_name == "Legendary Sword");
    REQUIRE(reward.icon == "icons/weapons/legendary_sword.png");
}

// ============================================================================
// QuestPrerequisite Tests
// ============================================================================

TEST_CASE("QuestPrerequisite Type enum", "[quest]") {
    REQUIRE(static_cast<int>(QuestPrerequisite::Type::QuestCompleted) == 0);
    REQUIRE(static_cast<int>(QuestPrerequisite::Type::QuestActive) == 1);
    REQUIRE(static_cast<int>(QuestPrerequisite::Type::Level) == 2);
    REQUIRE(static_cast<int>(QuestPrerequisite::Type::Reputation) == 3);
    REQUIRE(static_cast<int>(QuestPrerequisite::Type::Item) == 4);
    REQUIRE(static_cast<int>(QuestPrerequisite::Type::Flag) == 5);
    REQUIRE(static_cast<int>(QuestPrerequisite::Type::Custom) == 6);
}

TEST_CASE("QuestPrerequisite defaults", "[quest]") {
    QuestPrerequisite prereq;

    REQUIRE(prereq.type == QuestPrerequisite::Type::QuestCompleted);
    REQUIRE(prereq.key.empty());
    REQUIRE(prereq.value == 0);
    REQUIRE_FALSE(prereq.custom_check);
}

TEST_CASE("QuestPrerequisite quest completed", "[quest]") {
    QuestPrerequisite prereq;
    prereq.type = QuestPrerequisite::Type::QuestCompleted;
    prereq.key = "prologue_quest";

    REQUIRE(prereq.type == QuestPrerequisite::Type::QuestCompleted);
    REQUIRE(prereq.key == "prologue_quest");
}

TEST_CASE("QuestPrerequisite level", "[quest]") {
    QuestPrerequisite prereq;
    prereq.type = QuestPrerequisite::Type::Level;
    prereq.value = 10;

    REQUIRE(prereq.type == QuestPrerequisite::Type::Level);
    REQUIRE(prereq.value == 10);
}

// ============================================================================
// Quest Tests
// ============================================================================

TEST_CASE("Quest defaults", "[quest]") {
    Quest quest;

    REQUIRE(quest.id.empty());
    REQUIRE(quest.title_key.empty());
    REQUIRE(quest.description_key.empty());
    REQUIRE(quest.summary_key.empty());
    REQUIRE(quest.category == QuestCategory::Side);
    REQUIRE(quest.state == QuestState::Unavailable);
    REQUIRE(quest.objectives.empty());
    REQUIRE(quest.rewards.empty());
    REQUIRE(quest.prerequisites.empty());
    REQUIRE(quest.quest_giver == NullEntity);
    REQUIRE(quest.quest_giver_name.empty());
    REQUIRE(quest.turn_in_entity == NullEntity);
    REQUIRE_FALSE(quest.turn_in_location.has_value());
    REQUIRE(quest.icon.empty());
    REQUIRE(quest.display_order == 0);
    REQUIRE(quest.is_tracked == false);
    REQUIRE(quest.is_repeatable == false);
    REQUIRE(quest.auto_track_on_accept == true);
    REQUIRE(quest.fail_on_objective_fail == true);
    REQUIRE(quest.repeat_count == 0);
    REQUIRE_THAT(quest.time_started, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(quest.time_completed, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("Quest state queries", "[quest]") {
    Quest quest;

    SECTION("is_available") {
        quest.state = QuestState::Available;
        REQUIRE(quest.is_available());
        REQUIRE_FALSE(quest.is_active());
        REQUIRE_FALSE(quest.is_completed());
        REQUIRE_FALSE(quest.is_failed());
    }

    SECTION("is_active") {
        quest.state = QuestState::Active;
        REQUIRE_FALSE(quest.is_available());
        REQUIRE(quest.is_active());
        REQUIRE_FALSE(quest.is_completed());
        REQUIRE_FALSE(quest.is_failed());
    }

    SECTION("is_completed") {
        quest.state = QuestState::Completed;
        REQUIRE_FALSE(quest.is_available());
        REQUIRE_FALSE(quest.is_active());
        REQUIRE(quest.is_completed());
        REQUIRE_FALSE(quest.is_failed());
    }

    SECTION("is_failed") {
        quest.state = QuestState::Failed;
        REQUIRE_FALSE(quest.is_available());
        REQUIRE_FALSE(quest.is_active());
        REQUIRE_FALSE(quest.is_completed());
        REQUIRE(quest.is_failed());
    }
}

TEST_CASE("Quest find_objective", "[quest]") {
    Quest quest;

    Objective obj1;
    obj1.id = "obj_1";
    quest.objectives.push_back(obj1);

    Objective obj2;
    obj2.id = "obj_2";
    quest.objectives.push_back(obj2);

    SECTION("Find existing objective") {
        Objective* found = quest.find_objective("obj_1");
        REQUIRE(found != nullptr);
        REQUIRE(found->id == "obj_1");
    }

    SECTION("Find non-existing objective") {
        Objective* found = quest.find_objective("obj_3");
        REQUIRE(found == nullptr);
    }

    SECTION("Const find_objective") {
        const Quest& const_quest = quest;
        const Objective* found = const_quest.find_objective("obj_2");
        REQUIRE(found != nullptr);
        REQUIRE(found->id == "obj_2");
    }
}

TEST_CASE("Quest all_required_complete", "[quest]") {
    Quest quest;

    SECTION("Empty objectives - returns true") {
        REQUIRE(quest.all_required_complete());
    }

    SECTION("All required completed") {
        Objective obj1;
        obj1.id = "obj_1";
        obj1.is_optional = false;
        obj1.state = ObjectiveState::Completed;

        Objective obj2;
        obj2.id = "obj_2";
        obj2.is_optional = false;
        obj2.state = ObjectiveState::Completed;

        quest.objectives.push_back(obj1);
        quest.objectives.push_back(obj2);

        REQUIRE(quest.all_required_complete());
    }

    SECTION("Some required incomplete") {
        Objective obj1;
        obj1.id = "obj_1";
        obj1.is_optional = false;
        obj1.state = ObjectiveState::Completed;

        Objective obj2;
        obj2.id = "obj_2";
        obj2.is_optional = false;
        obj2.state = ObjectiveState::Active;

        quest.objectives.push_back(obj1);
        quest.objectives.push_back(obj2);

        REQUIRE_FALSE(quest.all_required_complete());
    }

    SECTION("Optional incomplete doesn't affect") {
        Objective obj1;
        obj1.id = "obj_1";
        obj1.is_optional = false;
        obj1.state = ObjectiveState::Completed;

        Objective obj2;
        obj2.id = "obj_2";
        obj2.is_optional = true;  // Optional
        obj2.state = ObjectiveState::Active;

        quest.objectives.push_back(obj1);
        quest.objectives.push_back(obj2);

        REQUIRE(quest.all_required_complete());
    }
}

TEST_CASE("Quest any_failed", "[quest]") {
    Quest quest;

    SECTION("No failed objectives") {
        Objective obj;
        obj.is_optional = false;
        obj.state = ObjectiveState::Active;
        quest.objectives.push_back(obj);

        REQUIRE_FALSE(quest.any_failed());
    }

    SECTION("Required objective failed") {
        Objective obj;
        obj.is_optional = false;
        obj.state = ObjectiveState::Failed;
        quest.objectives.push_back(obj);

        REQUIRE(quest.any_failed());
    }

    SECTION("Optional objective failed - doesn't count") {
        Objective obj;
        obj.is_optional = true;
        obj.state = ObjectiveState::Failed;
        quest.objectives.push_back(obj);

        REQUIRE_FALSE(quest.any_failed());
    }
}

TEST_CASE("Quest get_active_objective_count", "[quest]") {
    Quest quest;

    Objective obj1;
    obj1.state = ObjectiveState::Active;
    quest.objectives.push_back(obj1);

    Objective obj2;
    obj2.state = ObjectiveState::Active;
    quest.objectives.push_back(obj2);

    Objective obj3;
    obj3.state = ObjectiveState::Completed;
    quest.objectives.push_back(obj3);

    REQUIRE(quest.get_active_objective_count() == 2);
}

TEST_CASE("Quest get_completed_objective_count", "[quest]") {
    Quest quest;

    Objective obj1;
    obj1.state = ObjectiveState::Completed;
    quest.objectives.push_back(obj1);

    Objective obj2;
    obj2.state = ObjectiveState::Completed;
    quest.objectives.push_back(obj2);

    Objective obj3;
    obj3.state = ObjectiveState::Active;
    quest.objectives.push_back(obj3);

    REQUIRE(quest.get_completed_objective_count() == 2);
}

TEST_CASE("Quest get_progress", "[quest]") {
    Quest quest;

    SECTION("Empty objectives - incomplete") {
        quest.state = QuestState::Active;
        float progress = quest.get_progress();
        REQUIRE_THAT(progress, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Empty objectives - completed") {
        quest.state = QuestState::Completed;
        float progress = quest.get_progress();
        REQUIRE_THAT(progress, WithinAbs(1.0f, 0.001f));
    }

    SECTION("Half required completed") {
        Objective obj1;
        obj1.is_optional = false;
        obj1.state = ObjectiveState::Completed;

        Objective obj2;
        obj2.is_optional = false;
        obj2.state = ObjectiveState::Active;

        quest.objectives.push_back(obj1);
        quest.objectives.push_back(obj2);

        float progress = quest.get_progress();
        REQUIRE_THAT(progress, WithinAbs(0.5f, 0.01f));
    }

    SECTION("Optional objectives not counted") {
        Objective obj1;
        obj1.is_optional = false;
        obj1.state = ObjectiveState::Completed;

        Objective obj2;
        obj2.is_optional = true;  // Optional - not counted
        obj2.state = ObjectiveState::Active;

        quest.objectives.push_back(obj1);
        quest.objectives.push_back(obj2);

        float progress = quest.get_progress();
        REQUIRE_THAT(progress, WithinAbs(1.0f, 0.001f));  // 1/1 required
    }
}

TEST_CASE("Quest get_active_objectives", "[quest]") {
    Quest quest;

    Objective obj1;
    obj1.id = "obj_1";
    obj1.state = ObjectiveState::Active;
    obj1.is_hidden = false;
    quest.objectives.push_back(obj1);

    Objective obj2;
    obj2.id = "obj_2";
    obj2.state = ObjectiveState::Completed;
    obj2.is_hidden = false;
    quest.objectives.push_back(obj2);

    Objective obj3;
    obj3.id = "obj_3";
    obj3.state = ObjectiveState::Active;
    obj3.is_hidden = true;  // Hidden
    quest.objectives.push_back(obj3);

    auto active = quest.get_active_objectives();
    REQUIRE(active.size() == 1);  // Only obj_1 (active and not hidden)
    REQUIRE(active[0]->id == "obj_1");
}

// ============================================================================
// QuestBuilder Tests
// ============================================================================

TEST_CASE("QuestBuilder main quest", "[quest]") {
    auto quest = make_quest("main_quest_1")
        .title("MAIN_QUEST_TITLE")
        .description("MAIN_QUEST_DESC")
        .summary("MAIN_QUEST_SUMMARY")
        .main_quest()
        .icon("icons/quests/main.png")
        .order(1)
        .build();

    REQUIRE(quest.id == "main_quest_1");
    REQUIRE(quest.title_key == "MAIN_QUEST_TITLE");
    REQUIRE(quest.description_key == "MAIN_QUEST_DESC");
    REQUIRE(quest.summary_key == "MAIN_QUEST_SUMMARY");
    REQUIRE(quest.category == QuestCategory::Main);
    REQUIRE(quest.icon == "icons/quests/main.png");
    REQUIRE(quest.display_order == 1);
}

TEST_CASE("QuestBuilder side quest", "[quest]") {
    auto quest = make_quest("side_quest_1")
        .title("SIDE_QUEST_TITLE")
        .side_quest()
        .build();

    REQUIRE(quest.id == "side_quest_1");
    REQUIRE(quest.category == QuestCategory::Side);
}

TEST_CASE("QuestBuilder with objectives", "[quest]") {
    auto obj1 = make_objective("obj_1").title("OBJ_1").simple().build();
    auto obj2 = make_objective("obj_2").title("OBJ_2").counter("items", 5).build();

    auto quest = make_quest("quest_with_objectives")
        .title("QUEST_TITLE")
        .objective(obj1)
        .objective(obj2)
        .build();

    REQUIRE(quest.objectives.size() == 2);
    REQUIRE(quest.objectives[0].id == "obj_1");
    REQUIRE(quest.objectives[1].id == "obj_2");
}

TEST_CASE("QuestBuilder with rewards", "[quest]") {
    auto quest = make_quest("quest_rewards")
        .title("QUEST_TITLE")
        .reward("experience", "", 1000)
        .reward("gold", "", 500)
        .reward("item", "rare_sword", 1)
        .build();

    REQUIRE(quest.rewards.size() == 3);
    REQUIRE(quest.rewards[0].type == "experience");
    REQUIRE(quest.rewards[0].amount == 1000);
    REQUIRE(quest.rewards[1].type == "gold");
    REQUIRE(quest.rewards[1].amount == 500);
    REQUIRE(quest.rewards[2].type == "item");
    REQUIRE(quest.rewards[2].value == "rare_sword");
}

TEST_CASE("QuestBuilder with prerequisites", "[quest]") {
    auto quest = make_quest("quest_prereqs")
        .title("QUEST_TITLE")
        .requires_quest("prologue")
        .requires_level(10)
        .requires_flag("chapter_1_complete")
        .build();

    REQUIRE(quest.prerequisites.size() == 3);
    REQUIRE(quest.prerequisites[0].type == QuestPrerequisite::Type::QuestCompleted);
    REQUIRE(quest.prerequisites[0].key == "prologue");
    REQUIRE(quest.prerequisites[1].type == QuestPrerequisite::Type::Level);
    REQUIRE(quest.prerequisites[1].value == 10);
    REQUIRE(quest.prerequisites[2].type == QuestPrerequisite::Type::Flag);
    REQUIRE(quest.prerequisites[2].key == "chapter_1_complete");
}

TEST_CASE("QuestBuilder with giver and turn-in", "[quest]") {
    Vec3 turnin_pos{100.0f, 0.0f, 200.0f};

    auto quest = make_quest("quest_giver")
        .title("QUEST_TITLE")
        .giver(Entity{50}, "Mayor Johnson")
        .turn_in(Entity{51})
        .turn_in_location(turnin_pos)
        .build();

    REQUIRE(quest.quest_giver == Entity{50});
    REQUIRE(quest.quest_giver_name == "Mayor Johnson");
    REQUIRE(quest.turn_in_entity == Entity{51});
    REQUIRE(quest.turn_in_location.has_value());
    REQUIRE_THAT(quest.turn_in_location->x, WithinAbs(100.0f, 0.001f));
}

TEST_CASE("QuestBuilder repeatable", "[quest]") {
    auto quest = make_quest("bounty_quest")
        .title("BOUNTY")
        .category(QuestCategory::Bounty)
        .repeatable()
        .build();

    REQUIRE(quest.category == QuestCategory::Bounty);
    REQUIRE(quest.is_repeatable);
}
