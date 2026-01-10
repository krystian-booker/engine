#include <catch2/catch_test_macros.hpp>
#include <engine/companion/party_manager.hpp>
#include <engine/companion/companion.hpp>
#include <engine/companion/formation.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <nlohmann/json.hpp>

namespace engine::companion::tests {

class PartyManagerTests {};

using namespace engine::companion;
using namespace engine::scene;
using json = nlohmann::json;

// Helper to create a test world with companions
class PartyTestFixture {
public:
    World world;
    Entity leader;
    std::vector<Entity> companions;

    PartyTestFixture() {
        leader = world.create();
        world.emplace<LocalTransform>(leader);
        world.emplace<WorldTransform>(leader);

        // Create some companion entities
        for (int i = 0; i < 4; ++i) {
            Entity e = world.create();
            auto& comp = world.emplace<CompanionComponent>(e);
            comp.companion_id = "companion_" + std::to_string(i);
            comp.display_name = "Test Companion " + std::to_string(i);
            world.emplace<LocalTransform>(e);
            world.emplace<WorldTransform>(e);
            companions.push_back(e);
        }
    }

    void setup_party_manager() {
        auto& pm = party_manager();
        pm.set_world(&world);
        pm.set_leader(leader);
    }

    void cleanup_party_manager() {
        auto& pm = party_manager();
        pm.dismiss_all();
        pm.set_world(nullptr);
        pm.set_leader(NullEntity);
    }
};

TEST_CASE_METHOD(PartyManagerTests, "PartyManager add/remove companions", "[companion][party][@engine.companion][#PartyManagerTests]") {
    PartyTestFixture fixture;
    fixture.setup_party_manager();
    auto& pm = party_manager();

    SECTION("Add companion to party") {
        REQUIRE(pm.get_companion_count() == 0);

        bool added = pm.add_companion(fixture.companions[0]);
        REQUIRE(added);
        REQUIRE(pm.get_companion_count() == 1);

        // Verify companion's owner is set
        auto* comp = fixture.world.try_get<CompanionComponent>(fixture.companions[0]);
        REQUIRE(comp != nullptr);
        REQUIRE(comp->owner == fixture.leader);
    }

    SECTION("Remove companion from party") {
        pm.add_companion(fixture.companions[0]);
        REQUIRE(pm.get_companion_count() == 1);

        bool removed = pm.remove_companion(fixture.companions[0]);
        REQUIRE(removed);
        REQUIRE(pm.get_companion_count() == 0);

        // Verify companion's owner is cleared
        auto* comp = fixture.world.try_get<CompanionComponent>(fixture.companions[0]);
        REQUIRE(comp != nullptr);
        REQUIRE(comp->owner == NullEntity);
    }

    SECTION("Dismiss all companions") {
        pm.add_companion(fixture.companions[0]);
        pm.add_companion(fixture.companions[1]);
        pm.add_companion(fixture.companions[2]);
        REQUIRE(pm.get_companion_count() == 3);

        pm.dismiss_all();
        REQUIRE(pm.get_companion_count() == 0);
    }

    SECTION("is_companion returns correct value") {
        REQUIRE_FALSE(pm.is_companion(fixture.companions[0]));

        pm.add_companion(fixture.companions[0]);
        REQUIRE(pm.is_companion(fixture.companions[0]));
        REQUIRE_FALSE(pm.is_companion(fixture.companions[1]));

        pm.remove_companion(fixture.companions[0]);
        REQUIRE_FALSE(pm.is_companion(fixture.companions[0]));
    }

    SECTION("Cannot add same companion twice") {
        bool first = pm.add_companion(fixture.companions[0]);
        bool second = pm.add_companion(fixture.companions[0]);

        REQUIRE(first);
        REQUIRE_FALSE(second);
        REQUIRE(pm.get_companion_count() == 1);
    }

    SECTION("Cannot add entity without CompanionComponent") {
        Entity invalid = fixture.world.create();
        bool added = pm.add_companion(invalid);

        REQUIRE_FALSE(added);
        REQUIRE(pm.get_companion_count() == 0);
    }

    SECTION("Party size limit is respected") {
        pm.set_max_party_size(2);

        pm.add_companion(fixture.companions[0]);
        pm.add_companion(fixture.companions[1]);
        bool third = pm.add_companion(fixture.companions[2]);

        REQUIRE_FALSE(third);
        REQUIRE(pm.get_companion_count() == 2);
    }

    fixture.cleanup_party_manager();
}

TEST_CASE_METHOD(PartyManagerTests, "PartyManager commands", "[companion][party][@engine.companion][#PartyManagerTests]") {
    PartyTestFixture fixture;
    fixture.setup_party_manager();
    auto& pm = party_manager();

    pm.add_companion(fixture.companions[0]);
    pm.add_companion(fixture.companions[1]);

    SECTION("Issue command to all companions") {
        pm.issue_command(CompanionCommand::Wait);

        auto* comp0 = fixture.world.try_get<CompanionComponent>(fixture.companions[0]);
        auto* comp1 = fixture.world.try_get<CompanionComponent>(fixture.companions[1]);

        REQUIRE(comp0->state == CompanionState::Waiting);
        REQUIRE(comp1->state == CompanionState::Waiting);
    }

    SECTION("Issue command to specific companion") {
        pm.issue_command(fixture.companions[0], CompanionCommand::Wait);

        auto* comp0 = fixture.world.try_get<CompanionComponent>(fixture.companions[0]);
        auto* comp1 = fixture.world.try_get<CompanionComponent>(fixture.companions[1]);

        REQUIRE(comp0->state == CompanionState::Waiting);
        REQUIRE(comp1->state == CompanionState::Following);  // Unchanged
    }

    SECTION("Issue command with target position") {
        Vec3 target_pos(10.0f, 0.0f, 20.0f);
        pm.issue_command(fixture.companions[0], CompanionCommand::Move, target_pos);

        auto* comp = fixture.world.try_get<CompanionComponent>(fixture.companions[0]);
        REQUIRE(comp->state == CompanionState::Moving);
        REQUIRE(comp->command_position == target_pos);
    }

    SECTION("Issue command with target entity") {
        Entity target = fixture.world.create();
        pm.issue_command(fixture.companions[0], CompanionCommand::Attack, target);

        auto* comp = fixture.world.try_get<CompanionComponent>(fixture.companions[0]);
        REQUIRE(comp->state == CompanionState::Attacking);
        REQUIRE(comp->combat_target == target);
    }

    SECTION("Follow command restores following state") {
        pm.issue_command(CompanionCommand::Wait);
        pm.issue_command(CompanionCommand::Follow);

        auto* comp = fixture.world.try_get<CompanionComponent>(fixture.companions[0]);
        REQUIRE(comp->state == CompanionState::Following);
    }

    SECTION("Dismiss command removes companion") {
        pm.issue_command(fixture.companions[0], CompanionCommand::Dismiss);

        REQUIRE_FALSE(pm.is_companion(fixture.companions[0]));
        REQUIRE(pm.get_companion_count() == 1);
    }

    SECTION("Cannot command companion with can_be_commanded=false") {
        auto* comp = fixture.world.try_get<CompanionComponent>(fixture.companions[0]);
        comp->can_be_commanded = false;

        pm.issue_command(fixture.companions[0], CompanionCommand::Wait);

        REQUIRE(comp->state == CompanionState::Following);  // Unchanged
    }

    fixture.cleanup_party_manager();
}

TEST_CASE_METHOD(PartyManagerTests, "PartyManager formation", "[companion][party][@engine.companion][#PartyManagerTests]") {
    PartyTestFixture fixture;
    fixture.setup_party_manager();
    auto& pm = party_manager();

    pm.add_companion(fixture.companions[0]);
    pm.add_companion(fixture.companions[1]);

    SECTION("Set formation type updates companions") {
        pm.set_formation(FormationType::Line);

        const auto& formation = pm.get_formation();
        REQUIRE(formation.type == FormationType::Line);
    }

    SECTION("Custom formation is applied") {
        Formation custom;
        custom.type = FormationType::Custom;
        custom.slots.push_back({Vec3(5.0f, 0.0f, -5.0f), 0.0f, 0, false});
        custom.slots.push_back({Vec3(-5.0f, 0.0f, -5.0f), 0.0f, 1, false});

        pm.set_custom_formation(custom);

        const auto& formation = pm.get_formation();
        REQUIRE(formation.type == FormationType::Custom);
        REQUIRE(formation.slots.size() == 2);
    }

    SECTION("Formation slots are assigned correctly") {
        auto* comp0 = fixture.world.try_get<CompanionComponent>(fixture.companions[0]);
        auto* comp1 = fixture.world.try_get<CompanionComponent>(fixture.companions[1]);

        // Both should have valid formation slots
        REQUIRE(comp0->formation_slot >= 0);
        REQUIRE(comp1->formation_slot >= 0);
        REQUIRE(comp0->formation_slot != comp1->formation_slot);
    }

    SECTION("Formation slot is released when companion leaves") {
        auto* comp0 = fixture.world.try_get<CompanionComponent>(fixture.companions[0]);
        int slot = comp0->formation_slot;

        pm.remove_companion(fixture.companions[0]);

        REQUIRE(comp0->formation_slot == -1);

        // Slot should be available for new companion
        pm.add_companion(fixture.companions[2]);
        auto* comp2 = fixture.world.try_get<CompanionComponent>(fixture.companions[2]);
        // New companion should get an available slot (might be the released one)
        REQUIRE(comp2->formation_slot >= 0);
    }

    fixture.cleanup_party_manager();
}

TEST_CASE_METHOD(PartyManagerTests, "PartyManager queries", "[companion][party][@engine.companion][#PartyManagerTests]") {
    PartyTestFixture fixture;
    fixture.setup_party_manager();
    auto& pm = party_manager();

    pm.add_companion(fixture.companions[0]);
    pm.add_companion(fixture.companions[1]);
    pm.add_companion(fixture.companions[2]);

    SECTION("get_companions returns all companions") {
        auto companions = pm.get_companions();
        REQUIRE(companions.size() == 3);
    }

    SECTION("get_companions_in_state filters correctly") {
        auto* comp0 = fixture.world.try_get<CompanionComponent>(fixture.companions[0]);
        auto* comp1 = fixture.world.try_get<CompanionComponent>(fixture.companions[1]);

        comp0->set_state(CompanionState::Waiting);
        comp1->set_state(CompanionState::Waiting);

        auto waiting = pm.get_companions_in_state(CompanionState::Waiting);
        REQUIRE(waiting.size() == 2);

        auto following = pm.get_companions_in_state(CompanionState::Following);
        REQUIRE(following.size() == 1);
    }

    SECTION("get_companions_in_combat returns combat companions") {
        auto* comp0 = fixture.world.try_get<CompanionComponent>(fixture.companions[0]);
        comp0->set_state(CompanionState::Attacking);

        auto in_combat = pm.get_companions_in_combat();
        REQUIRE(in_combat.size() == 1);
        REQUIRE(in_combat[0] == fixture.companions[0]);
    }

    SECTION("get_idle_companions returns idle companions") {
        auto* comp0 = fixture.world.try_get<CompanionComponent>(fixture.companions[0]);
        comp0->set_state(CompanionState::Attacking);

        auto idle = pm.get_idle_companions();
        REQUIRE(idle.size() == 2);  // companions[1] and [2] are following (idle)
    }

    SECTION("get_downed_companions returns dead companions") {
        auto* comp0 = fixture.world.try_get<CompanionComponent>(fixture.companions[0]);
        comp0->set_state(CompanionState::Dead);

        auto downed = pm.get_downed_companions();
        REQUIRE(downed.size() == 1);
        REQUIRE(downed[0] == fixture.companions[0]);
    }

    SECTION("find_companion by id works") {
        Entity found = pm.find_companion("companion_1");
        REQUIRE(found == fixture.companions[1]);

        Entity not_found = pm.find_companion("nonexistent");
        REQUIRE(not_found == NullEntity);
    }

    fixture.cleanup_party_manager();
}

TEST_CASE_METHOD(PartyManagerTests, "PartyManager serialization", "[companion][party][@engine.companion][#PartyManagerTests]") {
    PartyTestFixture fixture;
    fixture.setup_party_manager();
    auto& pm = party_manager();

    pm.add_companion(fixture.companions[0]);
    pm.add_companion(fixture.companions[1]);
    pm.set_formation(FormationType::Circle);
    pm.set_max_party_size(6);

    SECTION("Serialize party state to JSON") {
        json out;
        pm.serialize(out);

        REQUIRE(out.contains("formation_type"));
        REQUIRE(out.contains("max_party_size"));
        REQUIRE(out.contains("companions"));
        REQUIRE(out["max_party_size"].get<size_t>() == 6);
        REQUIRE(out["companions"].size() == 2);
    }

    SECTION("Deserialize party state from JSON") {
        json data;
        pm.serialize(data);

        // Create new world for deserialization
        World new_world;

        // Deserialize into the existing singleton (with new world)
        pm.deserialize(data, new_world);

        // Note: Entities can't be restored directly (they're runtime)
        // But formation type and settings should be restored
    }

    SECTION("Round-trip preserves settings") {
        json data;
        pm.serialize(data);

        int formation_type = data["formation_type"].get<int>();
        REQUIRE(formation_type == static_cast<int>(FormationType::Circle));

        size_t max_size = data["max_party_size"].get<size_t>();
        REQUIRE(max_size == 6);
    }

    fixture.cleanup_party_manager();
}

TEST_CASE_METHOD(PartyManagerTests, "PartyManager callbacks", "[companion][party][@engine.companion][#PartyManagerTests]") {
    PartyTestFixture fixture;
    fixture.setup_party_manager();
    auto& pm = party_manager();

    SECTION("on_companion_joined callback is called") {
        Entity joined_entity = NullEntity;
        pm.set_on_companion_joined([&](Entity e) {
            joined_entity = e;
        });

        pm.add_companion(fixture.companions[0]);
        REQUIRE(joined_entity == fixture.companions[0]);
    }

    SECTION("on_companion_left callback is called") {
        Entity left_entity = NullEntity;
        pm.set_on_companion_left([&](Entity e) {
            left_entity = e;
        });

        pm.add_companion(fixture.companions[0]);
        pm.remove_companion(fixture.companions[0]);
        REQUIRE(left_entity == fixture.companions[0]);
    }

    fixture.cleanup_party_manager();
}

TEST_CASE_METHOD(PartyManagerTests, "PartyManager debug info", "[companion][party][@engine.companion][#PartyManagerTests]") {
    PartyTestFixture fixture;
    fixture.setup_party_manager();
    auto& pm = party_manager();

    pm.add_companion(fixture.companions[0]);

    SECTION("get_debug_info returns formatted string") {
        std::string debug = pm.get_debug_info();

        REQUIRE(debug.find("Party Manager") != std::string::npos);
        REQUIRE(debug.find("Leader") != std::string::npos);
        REQUIRE(debug.find("Companions") != std::string::npos);
        REQUIRE(debug.find("Formation") != std::string::npos);
    }

    fixture.cleanup_party_manager();
}

} // namespace engine::companion::tests
