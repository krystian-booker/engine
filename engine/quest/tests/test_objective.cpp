#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/quest/objective.hpp>

using namespace engine::quest;
using namespace engine::scene;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// ObjectiveState Tests
// ============================================================================

TEST_CASE("ObjectiveState enum", "[quest][objective]") {
    REQUIRE(static_cast<int>(ObjectiveState::Inactive) == 0);
    REQUIRE(static_cast<int>(ObjectiveState::Active) == 1);
    REQUIRE(static_cast<int>(ObjectiveState::Completed) == 2);
    REQUIRE(static_cast<int>(ObjectiveState::Failed) == 3);
}

// ============================================================================
// ObjectiveType Tests
// ============================================================================

TEST_CASE("ObjectiveType enum", "[quest][objective]") {
    REQUIRE(static_cast<int>(ObjectiveType::Simple) == 0);
    REQUIRE(static_cast<int>(ObjectiveType::Counter) == 1);
    REQUIRE(static_cast<int>(ObjectiveType::Location) == 2);
    REQUIRE(static_cast<int>(ObjectiveType::Interact) == 3);
    REQUIRE(static_cast<int>(ObjectiveType::Kill) == 4);
    REQUIRE(static_cast<int>(ObjectiveType::Timer) == 5);
    REQUIRE(static_cast<int>(ObjectiveType::Escort) == 6);
    REQUIRE(static_cast<int>(ObjectiveType::Custom) == 7);
}

// ============================================================================
// CounterData Tests
// ============================================================================

TEST_CASE("CounterData defaults", "[quest][objective]") {
    CounterData data;

    REQUIRE(data.current == 0);
    REQUIRE(data.target == 1);
    REQUIRE(data.counter_key.empty());
}

TEST_CASE("CounterData custom values", "[quest][objective]") {
    CounterData data;
    data.current = 5;
    data.target = 10;
    data.counter_key = "herbs_collected";

    REQUIRE(data.current == 5);
    REQUIRE(data.target == 10);
    REQUIRE(data.counter_key == "herbs_collected");
}

// ============================================================================
// LocationData Tests
// ============================================================================

TEST_CASE("LocationData defaults", "[quest][objective]") {
    LocationData data;

    REQUIRE_THAT(data.target_position.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(data.target_position.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(data.target_position.z, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(data.radius, WithinAbs(5.0f, 0.001f));
    REQUIRE(data.location_name.empty());
    REQUIRE(data.any_party_member == false);
}

TEST_CASE("LocationData custom values", "[quest][objective]") {
    LocationData data;
    data.target_position = Vec3{100.0f, 0.0f, 200.0f};
    data.radius = 10.0f;
    data.location_name = "Ancient Ruins";
    data.any_party_member = true;

    REQUIRE_THAT(data.target_position.x, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(data.target_position.z, WithinAbs(200.0f, 0.001f));
    REQUIRE_THAT(data.radius, WithinAbs(10.0f, 0.001f));
    REQUIRE(data.location_name == "Ancient Ruins");
    REQUIRE(data.any_party_member);
}

// ============================================================================
// InteractData Tests
// ============================================================================

TEST_CASE("InteractData defaults", "[quest][objective]") {
    InteractData data;

    REQUIRE(data.target_entity == NullEntity);
    REQUIRE(data.target_tag.empty());
    REQUIRE(data.interaction_type.empty());
}

TEST_CASE("InteractData with entity", "[quest][objective]") {
    InteractData data;
    data.target_entity = Entity{42};
    data.interaction_type = "talk";

    REQUIRE(data.target_entity == Entity{42});
    REQUIRE(data.interaction_type == "talk");
}

TEST_CASE("InteractData with tag", "[quest][objective]") {
    InteractData data;
    data.target_tag = "quest_npc";
    data.interaction_type = "examine";

    REQUIRE(data.target_tag == "quest_npc");
    REQUIRE(data.interaction_type == "examine");
}

// ============================================================================
// KillData Tests
// ============================================================================

TEST_CASE("KillData defaults", "[quest][objective]") {
    KillData data;

    REQUIRE(data.current == 0);
    REQUIRE(data.target == 1);
    REQUIRE(data.enemy_type.empty());
    REQUIRE(data.enemy_faction.empty());
}

TEST_CASE("KillData custom values", "[quest][objective]") {
    KillData data;
    data.current = 3;
    data.target = 10;
    data.enemy_type = "goblin";
    data.enemy_faction = "monsters";

    REQUIRE(data.current == 3);
    REQUIRE(data.target == 10);
    REQUIRE(data.enemy_type == "goblin");
    REQUIRE(data.enemy_faction == "monsters");
}

// ============================================================================
// TimerData Tests
// ============================================================================

TEST_CASE("TimerData defaults", "[quest][objective]") {
    TimerData data;

    REQUIRE_THAT(data.time_limit, WithinAbs(60.0f, 0.001f));
    REQUIRE_THAT(data.elapsed, WithinAbs(0.0f, 0.001f));
    REQUIRE(data.fail_on_timeout == true);
}

TEST_CASE("TimerData custom values", "[quest][objective]") {
    TimerData data;
    data.time_limit = 300.0f;  // 5 minutes
    data.elapsed = 150.0f;
    data.fail_on_timeout = false;

    REQUIRE_THAT(data.time_limit, WithinAbs(300.0f, 0.001f));
    REQUIRE_THAT(data.elapsed, WithinAbs(150.0f, 0.001f));
    REQUIRE_FALSE(data.fail_on_timeout);
}

// ============================================================================
// EscortData Tests
// ============================================================================

TEST_CASE("EscortData defaults", "[quest][objective]") {
    EscortData data;

    REQUIRE(data.escort_target == NullEntity);
    REQUIRE_THAT(data.destination.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(data.destination.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(data.destination.z, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(data.destination_radius, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(data.max_distance, WithinAbs(20.0f, 0.001f));
}

TEST_CASE("EscortData custom values", "[quest][objective]") {
    EscortData data;
    data.escort_target = Entity{100};
    data.destination = Vec3{500.0f, 0.0f, 300.0f};
    data.destination_radius = 10.0f;
    data.max_distance = 30.0f;

    REQUIRE(data.escort_target == Entity{100});
    REQUIRE_THAT(data.destination.x, WithinAbs(500.0f, 0.001f));
    REQUIRE_THAT(data.destination.z, WithinAbs(300.0f, 0.001f));
    REQUIRE_THAT(data.destination_radius, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(data.max_distance, WithinAbs(30.0f, 0.001f));
}

// ============================================================================
// Objective Tests
// ============================================================================

TEST_CASE("Objective defaults", "[quest][objective]") {
    Objective obj;

    REQUIRE(obj.id.empty());
    REQUIRE(obj.title_key.empty());
    REQUIRE(obj.description_key.empty());
    REQUIRE(obj.type == ObjectiveType::Simple);
    REQUIRE(obj.state == ObjectiveState::Inactive);
    REQUIRE(obj.show_in_hud == true);
    REQUIRE(obj.show_waypoint == true);
    REQUIRE(obj.display_order == 0);
    REQUIRE_FALSE(obj.waypoint_position.has_value());
    REQUIRE(obj.waypoint_entity == NullEntity);
    REQUIRE(obj.is_optional == false);
    REQUIRE(obj.is_hidden == false);
    REQUIRE(obj.auto_complete == true);
}

TEST_CASE("Objective state queries", "[quest][objective]") {
    Objective obj;

    SECTION("is_active") {
        obj.state = ObjectiveState::Active;
        REQUIRE(obj.is_active());
        REQUIRE_FALSE(obj.is_completed());
        REQUIRE_FALSE(obj.is_failed());
    }

    SECTION("is_completed") {
        obj.state = ObjectiveState::Completed;
        REQUIRE_FALSE(obj.is_active());
        REQUIRE(obj.is_completed());
        REQUIRE_FALSE(obj.is_failed());
    }

    SECTION("is_failed") {
        obj.state = ObjectiveState::Failed;
        REQUIRE_FALSE(obj.is_active());
        REQUIRE_FALSE(obj.is_completed());
        REQUIRE(obj.is_failed());
    }
}

TEST_CASE("Objective get_progress", "[quest][objective]") {
    Objective obj;

    SECTION("Simple objective - incomplete") {
        obj.type = ObjectiveType::Simple;
        obj.state = ObjectiveState::Active;
        float progress = obj.get_progress();
        REQUIRE_THAT(progress, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Simple objective - completed") {
        obj.type = ObjectiveType::Simple;
        obj.state = ObjectiveState::Completed;
        float progress = obj.get_progress();
        REQUIRE_THAT(progress, WithinAbs(1.0f, 0.001f));
    }

    SECTION("Counter objective - partial") {
        obj.type = ObjectiveType::Counter;
        CounterData data;
        data.current = 5;
        data.target = 10;
        obj.data = data;

        float progress = obj.get_progress();
        REQUIRE_THAT(progress, WithinAbs(0.5f, 0.01f));
    }

    SECTION("Kill objective - partial") {
        obj.type = ObjectiveType::Kill;
        KillData data;
        data.current = 7;
        data.target = 10;
        obj.data = data;

        float progress = obj.get_progress();
        REQUIRE_THAT(progress, WithinAbs(0.7f, 0.01f));
    }

    SECTION("Timer objective - partial") {
        obj.type = ObjectiveType::Timer;
        TimerData data;
        data.time_limit = 100.0f;
        data.elapsed = 25.0f;
        obj.data = data;

        float progress = obj.get_progress();
        REQUIRE_THAT(progress, WithinAbs(0.25f, 0.01f));
    }
}

TEST_CASE("Objective get_progress_text", "[quest][objective]") {
    Objective obj;

    SECTION("Counter objective") {
        obj.type = ObjectiveType::Counter;
        CounterData data;
        data.current = 3;
        data.target = 10;
        obj.data = data;

        std::string text = obj.get_progress_text();
        REQUIRE(text == "3/10");
    }

    SECTION("Kill objective") {
        obj.type = ObjectiveType::Kill;
        KillData data;
        data.current = 7;
        data.target = 10;
        obj.data = data;

        std::string text = obj.get_progress_text();
        REQUIRE(text == "7/10");
    }

    SECTION("Timer objective") {
        obj.type = ObjectiveType::Timer;
        TimerData data;
        data.time_limit = 125.0f;  // 2:05
        data.elapsed = 0.0f;
        obj.data = data;

        std::string text = obj.get_progress_text();
        REQUIRE(text == "2:05");
    }

    SECTION("Simple objective - empty text") {
        obj.type = ObjectiveType::Simple;
        std::string text = obj.get_progress_text();
        REQUIRE(text.empty());
    }
}

// ============================================================================
// ObjectiveBuilder Tests
// ============================================================================

TEST_CASE("ObjectiveBuilder simple objective", "[quest][objective]") {
    auto obj = make_objective("obj_talk_npc")
        .title("TALK_TO_NPC")
        .description("TALK_TO_NPC_DESC")
        .simple()
        .build();

    REQUIRE(obj.id == "obj_talk_npc");
    REQUIRE(obj.title_key == "TALK_TO_NPC");
    REQUIRE(obj.description_key == "TALK_TO_NPC_DESC");
    REQUIRE(obj.type == ObjectiveType::Simple);
}

TEST_CASE("ObjectiveBuilder counter objective", "[quest][objective]") {
    auto obj = make_objective("obj_collect_herbs")
        .title("COLLECT_HERBS")
        .counter("herbs_collected", 10)
        .build();

    REQUIRE(obj.id == "obj_collect_herbs");
    REQUIRE(obj.type == ObjectiveType::Counter);

    auto* data = std::get_if<CounterData>(&obj.data);
    REQUIRE(data != nullptr);
    REQUIRE(data->counter_key == "herbs_collected");
    REQUIRE(data->target == 10);
}

TEST_CASE("ObjectiveBuilder location objective", "[quest][objective]") {
    Vec3 pos{100.0f, 0.0f, 200.0f};
    auto obj = make_objective("obj_reach_ruins")
        .title("REACH_RUINS")
        .location(pos, 10.0f, "Ancient Ruins")
        .build();

    REQUIRE(obj.id == "obj_reach_ruins");
    REQUIRE(obj.type == ObjectiveType::Location);
    REQUIRE(obj.waypoint_position.has_value());

    auto* data = std::get_if<LocationData>(&obj.data);
    REQUIRE(data != nullptr);
    REQUIRE_THAT(data->target_position.x, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(data->radius, WithinAbs(10.0f, 0.001f));
    REQUIRE(data->location_name == "Ancient Ruins");
}

TEST_CASE("ObjectiveBuilder kill objective", "[quest][objective]") {
    auto obj = make_objective("obj_kill_goblins")
        .title("KILL_GOBLINS")
        .kill("goblin", 5)
        .build();

    REQUIRE(obj.id == "obj_kill_goblins");
    REQUIRE(obj.type == ObjectiveType::Kill);

    auto* data = std::get_if<KillData>(&obj.data);
    REQUIRE(data != nullptr);
    REQUIRE(data->enemy_type == "goblin");
    REQUIRE(data->target == 5);
}

TEST_CASE("ObjectiveBuilder timer objective", "[quest][objective]") {
    auto obj = make_objective("obj_timed_escape")
        .title("ESCAPE_TIMER")
        .timer(300.0f, true)
        .build();

    REQUIRE(obj.id == "obj_timed_escape");
    REQUIRE(obj.type == ObjectiveType::Timer);

    auto* data = std::get_if<TimerData>(&obj.data);
    REQUIRE(data != nullptr);
    REQUIRE_THAT(data->time_limit, WithinAbs(300.0f, 0.001f));
    REQUIRE(data->fail_on_timeout);
}

TEST_CASE("ObjectiveBuilder escort objective", "[quest][objective]") {
    Vec3 dest{500.0f, 0.0f, 300.0f};
    auto obj = make_objective("obj_escort_merchant")
        .title("ESCORT_MERCHANT")
        .escort(Entity{100}, dest, 10.0f)
        .build();

    REQUIRE(obj.id == "obj_escort_merchant");
    REQUIRE(obj.type == ObjectiveType::Escort);
    REQUIRE(obj.waypoint_position.has_value());

    auto* data = std::get_if<EscortData>(&obj.data);
    REQUIRE(data != nullptr);
    REQUIRE(data->escort_target == Entity{100});
    REQUIRE_THAT(data->destination.x, WithinAbs(500.0f, 0.001f));
    REQUIRE_THAT(data->destination_radius, WithinAbs(10.0f, 0.001f));
}

TEST_CASE("ObjectiveBuilder optional and hidden", "[quest][objective]") {
    auto obj = make_objective("obj_bonus")
        .title("BONUS")
        .simple()
        .optional()
        .hidden()
        .build();

    REQUIRE(obj.is_optional);
    REQUIRE(obj.is_hidden);
}

TEST_CASE("ObjectiveBuilder no waypoint/hud", "[quest][objective]") {
    auto obj = make_objective("obj_secret")
        .title("SECRET")
        .simple()
        .no_waypoint()
        .no_hud()
        .build();

    REQUIRE_FALSE(obj.show_waypoint);
    REQUIRE_FALSE(obj.show_in_hud);
}
