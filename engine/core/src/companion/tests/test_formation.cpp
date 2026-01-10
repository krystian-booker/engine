#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/companion/formation.hpp>

namespace engine::companion::tests {

class FormationTests {};

using namespace engine::companion;
using Catch::Matchers::WithinAbs;

TEST_CASE_METHOD(FormationTests, "Formation slot calculation", "[companion][formation][@engine.companion][#FormationTests]") {
    SECTION("Wedge formation calculates correct offsets") {
        Formation f = Formation::wedge(4, 2.0f);

        REQUIRE(f.type == FormationType::Wedge);
        REQUIRE(f.slots.size() == 4);

        // First two slots should be left and right
        REQUIRE(f.slots[0].offset.x < 0.0f);  // Left
        REQUIRE(f.slots[1].offset.x > 0.0f);  // Right

        // All slots should be behind (negative z)
        for (const auto& slot : f.slots) {
            REQUIRE(slot.offset.z < 0.0f);
        }
    }

    SECTION("Line formation calculates correct offsets") {
        Formation f = Formation::line(4, 2.0f);

        REQUIRE(f.type == FormationType::Line);
        REQUIRE(f.slots.size() == 4);

        // All slots should be at same z (behind leader)
        float expected_z = f.slots[0].offset.z;
        for (const auto& slot : f.slots) {
            REQUIRE_THAT(slot.offset.z, WithinAbs(expected_z, 0.01f));
        }

        // Slots should be spread horizontally
        REQUIRE(f.slots[0].offset.x != f.slots[1].offset.x);
    }

    SECTION("Circle formation calculates correct offsets") {
        Formation f = Formation::circle(4, 3.0f);

        REQUIRE(f.type == FormationType::Circle);
        REQUIRE(f.slots.size() == 4);

        // All slots should be at radius distance
        for (const auto& slot : f.slots) {
            float dist = glm::length(Vec3(slot.offset.x, 0.0f, slot.offset.z));
            REQUIRE_THAT(dist, WithinAbs(3.0f, 0.01f));
        }
    }

    SECTION("Column formation calculates correct offsets") {
        Formation f = Formation::column(4, 2.0f);

        REQUIRE(f.type == FormationType::Column);
        REQUIRE(f.slots.size() == 4);

        // All slots should be at x=0 (single file)
        for (const auto& slot : f.slots) {
            REQUIRE_THAT(slot.offset.x, WithinAbs(0.0f, 0.01f));
        }

        // Each slot should be further behind
        for (size_t i = 1; i < f.slots.size(); ++i) {
            REQUIRE(f.slots[i].offset.z < f.slots[i-1].offset.z);
        }
    }

    SECTION("Custom formation respects slot positions") {
        Formation f;
        f.type = FormationType::Custom;
        f.slots.push_back({Vec3(1.0f, 0.0f, -1.0f), 0.0f, 0, false});
        f.slots.push_back({Vec3(-1.0f, 0.0f, -1.0f), 0.0f, 1, false});

        REQUIRE(f.slots.size() == 2);
        REQUIRE(f.slots[0].offset.x == 1.0f);
        REQUIRE(f.slots[1].offset.x == -1.0f);
    }
}

TEST_CASE_METHOD(FormationTests, "Formation position calculation", "[companion][formation][@engine.companion][#FormationTests]") {
    Formation f = Formation::wedge(4, 2.0f);
    Vec3 leader_pos(10.0f, 0.0f, 10.0f);
    Vec3 leader_forward(0.0f, 0.0f, 1.0f);

    SECTION("get_formation_position returns world position") {
        Vec3 pos = calculate_formation_position(f, 0, leader_pos, leader_forward);

        // Should be offset from leader
        REQUIRE(pos != leader_pos);

        // Y should be same as leader (no vertical offset in wedge)
        REQUIRE_THAT(pos.y, WithinAbs(leader_pos.y, 0.01f));
    }

    SECTION("Formation rotates with leader facing") {
        Vec3 forward_z(0.0f, 0.0f, 1.0f);
        Vec3 forward_x(1.0f, 0.0f, 0.0f);

        Vec3 pos_z = calculate_formation_position(f, 0, leader_pos, forward_z);
        Vec3 pos_x = calculate_formation_position(f, 0, leader_pos, forward_x);

        // Positions should be different when facing different directions
        REQUIRE(pos_z != pos_x);
    }

    SECTION("Spacing scales correctly") {
        Formation small = Formation::wedge(4, 1.0f);
        Formation large = Formation::wedge(4, 4.0f);

        Vec3 pos_small = calculate_formation_position(small, 0, leader_pos, leader_forward);
        Vec3 pos_large = calculate_formation_position(large, 0, leader_pos, leader_forward);

        // Large spacing should result in further position
        float dist_small = glm::distance(leader_pos, pos_small);
        float dist_large = glm::distance(leader_pos, pos_large);

        REQUIRE(dist_large > dist_small);
    }
}

TEST_CASE_METHOD(FormationTests, "Formation slot management", "[companion][formation][@engine.companion][#FormationTests]") {
    Formation f = Formation::wedge(4, 2.0f);

    SECTION("get_next_available_slot returns first unoccupied") {
        REQUIRE(f.get_next_available_slot() == 0);

        f.set_slot_occupied(0, true);
        REQUIRE(f.get_next_available_slot() == 1);
    }

    SECTION("get_next_available_slot returns -1 when full") {
        for (size_t i = 0; i < f.slots.size(); ++i) {
            f.set_slot_occupied(static_cast<int>(i), true);
        }

        REQUIRE(f.get_next_available_slot() == -1);
    }

    SECTION("get_occupied_count tracks occupancy") {
        REQUIRE(f.get_occupied_count() == 0);

        f.set_slot_occupied(0, true);
        REQUIRE(f.get_occupied_count() == 1);

        f.set_slot_occupied(2, true);
        REQUIRE(f.get_occupied_count() == 2);
    }

    SECTION("clear_occupancy resets all slots") {
        f.set_slot_occupied(0, true);
        f.set_slot_occupied(1, true);
        REQUIRE(f.get_occupied_count() == 2);

        f.clear_occupancy();
        REQUIRE(f.get_occupied_count() == 0);
    }

    SECTION("get_capacity returns total slots") {
        REQUIRE(f.get_capacity() == 4);
    }
}

TEST_CASE_METHOD(FormationTests, "Formation type string conversion", "[companion][formation][@engine.companion][#FormationTests]") {
    SECTION("All types have string representation") {
        REQUIRE(std::string(formation_type_to_string(FormationType::Line)) == "Line");
        REQUIRE(std::string(formation_type_to_string(FormationType::Wedge)) == "Wedge");
        REQUIRE(std::string(formation_type_to_string(FormationType::Circle)) == "Circle");
        REQUIRE(std::string(formation_type_to_string(FormationType::Column)) == "Column");
        REQUIRE(std::string(formation_type_to_string(FormationType::Spread)) == "Spread");
        REQUIRE(std::string(formation_type_to_string(FormationType::Custom)) == "Custom");
    }
}

} // namespace engine::companion::tests
