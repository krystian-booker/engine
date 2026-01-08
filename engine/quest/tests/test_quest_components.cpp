#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/quest/quest_components.hpp>

using namespace engine::quest;
using Catch::Matchers::WithinAbs;

// ============================================================================
// QuestLogComponent Tests
// ============================================================================

TEST_CASE("QuestLogComponent defaults", "[quest][component]") {
    QuestLogComponent log;

    REQUIRE(log.active_quests.empty());
    REQUIRE(log.completed_quests.empty());
    REQUIRE(log.failed_quests.empty());
    REQUIRE(log.selected_quest.empty());
    REQUIRE(log.log_open == false);
    REQUIRE(log.total_quests_completed == 0);
    REQUIRE(log.total_objectives_completed == 0);
}

TEST_CASE("QuestLogComponent with quests", "[quest][component]") {
    QuestLogComponent log;

    log.active_quests.push_back("quest_1");
    log.active_quests.push_back("quest_2");
    log.completed_quests.push_back("prologue");
    log.failed_quests.push_back("timed_quest");
    log.selected_quest = "quest_1";
    log.log_open = true;
    log.total_quests_completed = 5;
    log.total_objectives_completed = 25;

    REQUIRE(log.active_quests.size() == 2);
    REQUIRE(log.completed_quests.size() == 1);
    REQUIRE(log.failed_quests.size() == 1);
    REQUIRE(log.selected_quest == "quest_1");
    REQUIRE(log.log_open);
    REQUIRE(log.total_quests_completed == 5);
    REQUIRE(log.total_objectives_completed == 25);
}

// ============================================================================
// QuestParticipantComponent Tests
// ============================================================================

TEST_CASE("QuestParticipantComponent defaults", "[quest][component]") {
    QuestParticipantComponent participant;

    REQUIRE(participant.quest_id.empty());
    REQUIRE(participant.role.empty());
    REQUIRE(participant.must_survive == false);
    REQUIRE_THAT(participant.current_health, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(participant.max_health, WithinAbs(100.0f, 0.001f));
    REQUIRE(participant.has_been_interacted == false);
    REQUIRE(participant.required_interaction.empty());
}

TEST_CASE("QuestParticipantComponent escort target", "[quest][component]") {
    QuestParticipantComponent participant;
    participant.quest_id = "escort_mission";
    participant.role = "escort_target";
    participant.must_survive = true;
    participant.current_health = 80.0f;
    participant.max_health = 100.0f;

    REQUIRE(participant.quest_id == "escort_mission");
    REQUIRE(participant.role == "escort_target");
    REQUIRE(participant.must_survive);
    REQUIRE_THAT(participant.current_health, WithinAbs(80.0f, 0.001f));
}

TEST_CASE("QuestParticipantComponent interaction target", "[quest][component]") {
    QuestParticipantComponent participant;
    participant.quest_id = "gather_info";
    participant.role = "informant";
    participant.required_interaction = "talk";
    participant.has_been_interacted = false;

    REQUIRE(participant.quest_id == "gather_info");
    REQUIRE(participant.role == "informant");
    REQUIRE(participant.required_interaction == "talk");
    REQUIRE_FALSE(participant.has_been_interacted);

    // Simulate interaction
    participant.has_been_interacted = true;
    REQUIRE(participant.has_been_interacted);
}

// ============================================================================
// KillTrackerComponent Tests
// ============================================================================

TEST_CASE("KillTrackerComponent defaults", "[quest][component]") {
    KillTrackerComponent tracker;

    REQUIRE(tracker.enemy_type.empty());
    REQUIRE(tracker.faction.empty());
    REQUIRE(tracker.report_on_death == true);
}

TEST_CASE("KillTrackerComponent custom values", "[quest][component]") {
    KillTrackerComponent tracker;
    tracker.enemy_type = "goblin";
    tracker.faction = "monsters";
    tracker.report_on_death = true;

    REQUIRE(tracker.enemy_type == "goblin");
    REQUIRE(tracker.faction == "monsters");
    REQUIRE(tracker.report_on_death);
}

// ============================================================================
// CollectionItemComponent Tests
// ============================================================================

TEST_CASE("CollectionItemComponent defaults", "[quest][component]") {
    CollectionItemComponent item;

    REQUIRE(item.counter_key.empty());
    REQUIRE(item.amount == 1);
    REQUIRE(item.destroy_on_collect == true);
    REQUIRE(item.require_interaction == false);
    REQUIRE(item.collect_sound.empty());
    REQUIRE(item.collect_effect.empty());
}

TEST_CASE("CollectionItemComponent herb pickup", "[quest][component]") {
    CollectionItemComponent item;
    item.counter_key = "herbs_collected";
    item.amount = 1;
    item.destroy_on_collect = true;
    item.require_interaction = false;  // Auto-collect on touch
    item.collect_sound = "sfx/pickup_herb.wav";
    item.collect_effect = "vfx/sparkle";

    REQUIRE(item.counter_key == "herbs_collected");
    REQUIRE(item.amount == 1);
    REQUIRE(item.destroy_on_collect);
    REQUIRE_FALSE(item.require_interaction);
    REQUIRE(item.collect_sound == "sfx/pickup_herb.wav");
    REQUIRE(item.collect_effect == "vfx/sparkle");
}

TEST_CASE("CollectionItemComponent treasure chest", "[quest][component]") {
    CollectionItemComponent item;
    item.counter_key = "treasures_found";
    item.amount = 1;
    item.destroy_on_collect = false;  // Chest remains but empty
    item.require_interaction = true;  // Must interact to open

    REQUIRE(item.counter_key == "treasures_found");
    REQUIRE_FALSE(item.destroy_on_collect);
    REQUIRE(item.require_interaction);
}

// ============================================================================
// QuestZoneComponent Tests
// ============================================================================

TEST_CASE("QuestZoneComponent defaults", "[quest][component]") {
    QuestZoneComponent zone;

    REQUIRE(zone.zone_id.empty());
    REQUIRE(zone.zone_name.empty());
    REQUIRE(zone.zone_quests.empty());
    REQUIRE(zone.discovered == false);
    REQUIRE(zone.show_on_map == true);
}

TEST_CASE("QuestZoneComponent custom values", "[quest][component]") {
    QuestZoneComponent zone;
    zone.zone_id = "haunted_forest";
    zone.zone_name = "Haunted Forest";
    zone.zone_quests = {"forest_quest_1", "forest_quest_2", "forest_boss"};
    zone.discovered = true;
    zone.show_on_map = true;

    REQUIRE(zone.zone_id == "haunted_forest");
    REQUIRE(zone.zone_name == "Haunted Forest");
    REQUIRE(zone.zone_quests.size() == 3);
    REQUIRE(zone.zone_quests[0] == "forest_quest_1");
    REQUIRE(zone.zone_quests[2] == "forest_boss");
    REQUIRE(zone.discovered);
    REQUIRE(zone.show_on_map);
}

TEST_CASE("QuestZoneComponent undiscovered", "[quest][component]") {
    QuestZoneComponent zone;
    zone.zone_id = "secret_area";
    zone.zone_name = "???";  // Hidden name until discovered
    zone.discovered = false;
    zone.show_on_map = false;

    REQUIRE(zone.zone_id == "secret_area");
    REQUIRE(zone.zone_name == "???");
    REQUIRE_FALSE(zone.discovered);
    REQUIRE_FALSE(zone.show_on_map);
}
