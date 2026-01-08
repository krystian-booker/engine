#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/navigation/nav_crowd.hpp>

using namespace engine::navigation;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("CrowdAgentParams defaults", "[navigation][crowd]") {
    CrowdAgentParams params;

    REQUIRE_THAT(params.radius, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(params.height, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(params.max_acceleration, WithinAbs(8.0f, 0.001f));
    REQUIRE_THAT(params.max_speed, WithinAbs(3.5f, 0.001f));
    REQUIRE_THAT(params.separation_weight, WithinAbs(2.0f, 0.001f));
    REQUIRE(params.avoidance_quality == 3);
    REQUIRE(params.obstacle_avoidance_type == 3);
    REQUIRE(params.update_flags == 0xFF);
}

TEST_CASE("CrowdAgentParams custom values", "[navigation][crowd]") {
    CrowdAgentParams params;
    params.radius = 0.3f;
    params.height = 1.8f;
    params.max_acceleration = 10.0f;
    params.max_speed = 5.0f;
    params.separation_weight = 1.5f;
    params.avoidance_quality = 2;

    REQUIRE_THAT(params.radius, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(params.height, WithinAbs(1.8f, 0.001f));
    REQUIRE_THAT(params.max_acceleration, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(params.max_speed, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(params.separation_weight, WithinAbs(1.5f, 0.001f));
    REQUIRE(params.avoidance_quality == 2);
}

TEST_CASE("CrowdAgentHandle defaults", "[navigation][crowd]") {
    CrowdAgentHandle handle;

    REQUIRE(handle.index == -1);
    REQUIRE_FALSE(handle.valid());
}

TEST_CASE("CrowdAgentHandle valid", "[navigation][crowd]") {
    CrowdAgentHandle handle;
    handle.index = 0;

    REQUIRE(handle.index == 0);
    REQUIRE(handle.valid());
}

TEST_CASE("CrowdAgentHandle positive index", "[navigation][crowd]") {
    CrowdAgentHandle handle;
    handle.index = 42;

    REQUIRE(handle.valid());
    REQUIRE(handle.index == 42);
}

TEST_CASE("CrowdAgentState defaults", "[navigation][crowd]") {
    CrowdAgentState state;

    REQUIRE_THAT(state.position.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(state.position.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(state.position.z, WithinAbs(0.0f, 0.001f));

    REQUIRE_THAT(state.velocity.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(state.velocity.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(state.velocity.z, WithinAbs(0.0f, 0.001f));

    REQUIRE_THAT(state.desired_velocity.x, WithinAbs(0.0f, 0.001f));

    REQUIRE_THAT(state.target.x, WithinAbs(0.0f, 0.001f));

    REQUIRE(state.has_target == false);
    REQUIRE(state.partial_path == false);
    REQUIRE(state.at_target == false);
}

TEST_CASE("CrowdAgentState with values", "[navigation][crowd]") {
    CrowdAgentState state;
    state.position = Vec3{10.0f, 0.0f, 10.0f};
    state.velocity = Vec3{3.0f, 0.0f, 0.0f};
    state.desired_velocity = Vec3{3.5f, 0.0f, 0.0f};
    state.target = Vec3{50.0f, 0.0f, 50.0f};
    state.has_target = true;
    state.partial_path = false;
    state.at_target = false;

    REQUIRE_THAT(state.position.x, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(state.velocity.x, WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(state.desired_velocity.x, WithinAbs(3.5f, 0.001f));
    REQUIRE_THAT(state.target.x, WithinAbs(50.0f, 0.001f));
    REQUIRE(state.has_target == true);
}

TEST_CASE("CrowdAgentState at target", "[navigation][crowd]") {
    CrowdAgentState state;
    state.position = Vec3{50.0f, 0.0f, 50.0f};
    state.target = Vec3{50.0f, 0.0f, 50.0f};
    state.has_target = true;
    state.at_target = true;
    state.velocity = Vec3{0.0f};

    REQUIRE(state.at_target == true);
    REQUIRE_THAT(state.velocity.x, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("NavCrowd default construction", "[navigation][crowd]") {
    NavCrowd crowd;

    REQUIRE_FALSE(crowd.is_initialized());
    REQUIRE(crowd.get_detour_crowd() == nullptr);
    REQUIRE(crowd.get_max_agents() == 128);
}

TEST_CASE("NavCrowd get_active_agent_count before init", "[navigation][crowd]") {
    NavCrowd crowd;

    // Should return 0 when not initialized
    REQUIRE(crowd.get_active_agent_count() == 0);
}

TEST_CASE("NavCrowd operations with invalid handle", "[navigation][crowd]") {
    NavCrowd crowd;
    CrowdAgentHandle invalid_handle;

    // These should be safe with invalid handle
    auto state = crowd.get_agent_state(invalid_handle);
    REQUIRE_FALSE(state.has_target);
    REQUIRE(state.at_target == false);

    auto pos = crowd.get_agent_position(invalid_handle);
    REQUIRE_THAT(pos.x, WithinAbs(0.0f, 0.001f));

    auto vel = crowd.get_agent_velocity(invalid_handle);
    REQUIRE_THAT(vel.x, WithinAbs(0.0f, 0.001f));

    REQUIRE_FALSE(crowd.has_reached_target(invalid_handle));
}
