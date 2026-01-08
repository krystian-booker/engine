#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/navigation/nav_agent.hpp>

using namespace engine::navigation;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("NavAgentState enum", "[navigation][agent]") {
    REQUIRE(static_cast<uint8_t>(NavAgentState::Idle) == 0);
    REQUIRE(static_cast<uint8_t>(NavAgentState::Moving) == 1);
    REQUIRE(static_cast<uint8_t>(NavAgentState::Waiting) == 2);
    REQUIRE(static_cast<uint8_t>(NavAgentState::Arrived) == 3);
    REQUIRE(static_cast<uint8_t>(NavAgentState::Failed) == 4);
}

TEST_CASE("NavAgentEvent enum", "[navigation][agent]") {
    REQUIRE(static_cast<uint8_t>(NavAgentEvent::Arrived) == 0);
    REQUIRE(static_cast<uint8_t>(NavAgentEvent::Failed) == 1);
    REQUIRE(static_cast<uint8_t>(NavAgentEvent::PathBlocked) == 2);
    REQUIRE(static_cast<uint8_t>(NavAgentEvent::Waiting) == 3);
    REQUIRE(static_cast<uint8_t>(NavAgentEvent::Rerouted) == 4);
}

TEST_CASE("AvoidanceQuality enum", "[navigation][agent]") {
    REQUIRE(static_cast<uint8_t>(AvoidanceQuality::None) == 0);
    REQUIRE(static_cast<uint8_t>(AvoidanceQuality::Low) == 1);
    REQUIRE(static_cast<uint8_t>(AvoidanceQuality::Medium) == 2);
    REQUIRE(static_cast<uint8_t>(AvoidanceQuality::High) == 3);
}

TEST_CASE("NavAgentComponent defaults", "[navigation][agent]") {
    NavAgentComponent agent;

    // Movement settings
    REQUIRE_THAT(agent.speed, WithinAbs(3.5f, 0.001f));
    REQUIRE_THAT(agent.acceleration, WithinAbs(8.0f, 0.001f));
    REQUIRE_THAT(agent.deceleration, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(agent.turning_speed, WithinAbs(360.0f, 0.001f));

    // Path following
    REQUIRE_THAT(agent.path_radius, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(agent.stopping_distance, WithinAbs(0.1f, 0.001f));
    REQUIRE_THAT(agent.height, WithinAbs(2.0f, 0.001f));

    // Avoidance
    REQUIRE_THAT(agent.avoidance_radius, WithinAbs(0.5f, 0.001f));
    REQUIRE(agent.avoidance == AvoidanceQuality::Medium);
    REQUIRE(agent.avoidance_priority == 50);

    // Crowd settings
    REQUIRE(agent.use_crowd == true);
    REQUIRE_THAT(agent.separation_weight, WithinAbs(2.0f, 0.001f));

    // Path settings
    REQUIRE(agent.auto_repath == true);
    REQUIRE_THAT(agent.repath_interval, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(agent.corner_threshold, WithinAbs(0.1f, 0.001f));

    // State
    REQUIRE(agent.state == NavAgentState::Idle);
    REQUIRE_THAT(agent.target.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(agent.velocity.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(agent.current_speed, WithinAbs(0.0f, 0.001f));
    REQUIRE(agent.has_target == false);

    // Path data
    REQUIRE(agent.path.empty());
    REQUIRE(agent.path_index == 0);
    REQUIRE_THAT(agent.path_distance, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(agent.time_since_repath, WithinAbs(0.0f, 0.001f));

    // Crowd
    REQUIRE(agent.crowd_agent_index == -1);

    // Debug
    REQUIRE(agent.debug_draw == false);
}

TEST_CASE("NavAgentComponent custom values", "[navigation][agent]") {
    NavAgentComponent agent;
    agent.speed = 5.0f;
    agent.acceleration = 10.0f;
    agent.avoidance = AvoidanceQuality::High;
    agent.avoidance_priority = 10;
    agent.use_crowd = false;
    agent.state = NavAgentState::Moving;
    agent.target = Vec3{10.0f, 0.0f, 10.0f};
    agent.has_target = true;

    REQUIRE_THAT(agent.speed, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(agent.acceleration, WithinAbs(10.0f, 0.001f));
    REQUIRE(agent.avoidance == AvoidanceQuality::High);
    REQUIRE(agent.avoidance_priority == 10);
    REQUIRE(agent.use_crowd == false);
    REQUIRE(agent.state == NavAgentState::Moving);
    REQUIRE_THAT(agent.target.x, WithinAbs(10.0f, 0.001f));
    REQUIRE(agent.has_target == true);
}

TEST_CASE("NavAgentComponent callback", "[navigation][agent]") {
    NavAgentComponent agent;

    bool callback_called = false;
    NavAgentEvent received_event = NavAgentEvent::Failed;

    agent.on_event = [&](NavAgentEvent event) {
        callback_called = true;
        received_event = event;
    };

    // Simulate callback invocation
    if (agent.on_event) {
        agent.on_event(NavAgentEvent::Arrived);
    }

    REQUIRE(callback_called);
    REQUIRE(received_event == NavAgentEvent::Arrived);
}

TEST_CASE("NavAgentComponent path data", "[navigation][agent]") {
    NavAgentComponent agent;
    agent.path = {
        Vec3{0.0f, 0.0f, 0.0f},
        Vec3{5.0f, 0.0f, 0.0f},
        Vec3{10.0f, 0.0f, 5.0f}
    };
    agent.path_index = 1;
    agent.path_distance = 10.0f;
    agent.state = NavAgentState::Moving;

    REQUIRE(agent.path.size() == 3);
    REQUIRE(agent.path_index == 1);
    REQUIRE_THAT(agent.path_distance, WithinAbs(10.0f, 0.001f));
}

TEST_CASE("NavAgentSystem default", "[navigation][agent]") {
    NavAgentSystem system;

    REQUIRE_FALSE(system.has_crowd());
    REQUIRE(system.get_crowd() == nullptr);
    REQUIRE(system.get_max_agents() == 128);
}

TEST_CASE("NavAgentSystem set_max_agents", "[navigation][agent]") {
    NavAgentSystem system;

    system.set_max_agents(256);
    REQUIRE(system.get_max_agents() == 256);

    system.set_max_agents(64);
    REQUIRE(system.get_max_agents() == 64);
}
