#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/core/timer.hpp>

using namespace engine::core;
using Catch::Matchers::WithinAbs;

// Helper to simulate frame updates
class TimerTestFixture {
protected:
    void simulate_time(TimerManager& tm, float total_seconds, float dt = 0.016f) {
        float elapsed = 0.0f;
        while (elapsed < total_seconds) {
            float frame_dt = std::min(dt, total_seconds - elapsed);
            tm.update(frame_dt);
            elapsed += frame_dt;
        }
    }
};

TEST_CASE_METHOD(TimerTestFixture, "TimerHandle validity", "[core][timer]") {
    TimerHandle handle;
    REQUIRE_FALSE(handle.valid());
    REQUIRE_FALSE(static_cast<bool>(handle));

    handle.id = 1;
    REQUIRE(handle.valid());
    REQUIRE(static_cast<bool>(handle));
}

TEST_CASE_METHOD(TimerTestFixture, "TimerManager set_timeout", "[core][timer]") {
    // Use the global timer manager for these tests
    auto& tm = timers();
    tm.cancel_all();

    SECTION("One-shot timer fires after delay") {
        int call_count = 0;
        auto handle = tm.set_timeout(0.5f, [&]() { call_count++; });

        REQUIRE(handle.valid());
        REQUIRE(tm.is_active(handle));

        // Not yet fired
        tm.update(0.3f);
        REQUIRE(call_count == 0);

        // Should fire now
        tm.update(0.3f);
        REQUIRE(call_count == 1);

        // Should not fire again (one-shot)
        tm.update(1.0f);
        REQUIRE(call_count == 1);
    }

    SECTION("Timer cancel prevents execution") {
        int call_count = 0;
        auto handle = tm.set_timeout(0.5f, [&]() { call_count++; });

        tm.cancel(handle);
        REQUIRE_FALSE(tm.is_active(handle));

        tm.update(1.0f);
        REQUIRE(call_count == 0);
    }

    tm.cancel_all();
}

TEST_CASE_METHOD(TimerTestFixture, "TimerManager set_interval", "[core][timer]") {
    auto& tm = timers();
    tm.cancel_all();

    SECTION("Repeating timer fires multiple times") {
        int call_count = 0;
        auto handle = tm.set_interval(0.1f, [&]() { call_count++; });

        tm.update(0.35f); // Should fire 3 times (0.1, 0.2, 0.3)
        REQUIRE(call_count >= 3);

        tm.cancel(handle);
    }

    SECTION("Limited repeat count") {
        int call_count = 0;
        auto handle = tm.set_interval(0.1f, 3, [&]() { call_count++; });

        tm.update(1.0f); // More than enough time for all repeats
        REQUIRE(call_count == 3);
    }

    tm.cancel_all();
}

TEST_CASE_METHOD(TimerTestFixture, "TimerManager pause and resume", "[core][timer]") {
    auto& tm = timers();
    tm.cancel_all();

    SECTION("Paused timer does not advance") {
        int call_count = 0;
        auto handle = tm.set_timeout(0.5f, [&]() { call_count++; });

        tm.update(0.2f);
        tm.pause(handle);
        REQUIRE(tm.is_paused(handle));

        tm.update(1.0f); // Should not fire while paused
        REQUIRE(call_count == 0);

        tm.resume(handle);
        REQUIRE_FALSE(tm.is_paused(handle));

        tm.update(0.4f); // Should fire now (0.2 + 0.4 >= 0.5)
        REQUIRE(call_count == 1);
    }

    SECTION("Pause and resume all") {
        int count1 = 0, count2 = 0;
        auto h1 = tm.set_timeout(0.5f, [&]() { count1++; });
        auto h2 = tm.set_timeout(0.5f, [&]() { count2++; });

        tm.pause_all();
        tm.update(1.0f);
        REQUIRE(count1 == 0);
        REQUIRE(count2 == 0);

        tm.resume_all();
        tm.update(0.6f);
        REQUIRE(count1 == 1);
        REQUIRE(count2 == 1);
    }

    tm.cancel_all();
}

TEST_CASE_METHOD(TimerTestFixture, "TimerManager get_remaining", "[core][timer]") {
    auto& tm = timers();
    tm.cancel_all();

    auto handle = tm.set_timeout(1.0f, []() {});
    REQUIRE_THAT(tm.get_remaining(handle), WithinAbs(1.0f, 0.001f));

    tm.update(0.3f);
    REQUIRE_THAT(tm.get_remaining(handle), WithinAbs(0.7f, 0.001f));

    tm.update(0.4f);
    REQUIRE_THAT(tm.get_remaining(handle), WithinAbs(0.3f, 0.001f));

    tm.cancel_all();
}

TEST_CASE_METHOD(TimerTestFixture, "TimerManager reset", "[core][timer]") {
    auto& tm = timers();
    tm.cancel_all();

    int call_count = 0;
    auto handle = tm.set_timeout(0.5f, [&]() { call_count++; });

    tm.update(0.4f);
    REQUIRE(call_count == 0);

    tm.reset(handle); // Reset back to 0.5s remaining

    tm.update(0.4f); // Still not enough
    REQUIRE(call_count == 0);

    tm.update(0.2f); // Now should fire
    REQUIRE(call_count == 1);

    tm.cancel_all();
}

TEST_CASE_METHOD(TimerTestFixture, "TimerManager time scale", "[core][timer]") {
    auto& tm = timers();
    tm.cancel_all();

    SECTION("Scaled timer respects time scale") {
        int call_count = 0;
        TimerConfig config;
        config.delay = 1.0f;
        config.use_scaled_time = true;

        auto handle = tm.create_timer(config, [&]() { call_count++; });

        // At 0.5x time scale, need 2 seconds real time
        tm.update(1.5f, 0.5f);
        REQUIRE(call_count == 0);

        tm.update(0.6f, 0.5f); // 0.75 + 0.3 = 1.05 scaled time
        REQUIRE(call_count == 1);
    }

    SECTION("Unscaled timer ignores time scale") {
        int call_count = 0;
        TimerConfig config;
        config.delay = 1.0f;
        config.use_scaled_time = false;

        auto handle = tm.create_timer(config, [&]() { call_count++; });

        // Should fire based on real time regardless of scale
        tm.update(0.6f, 0.0f); // Zero time scale shouldn't matter
        REQUIRE(call_count == 0);

        tm.update(0.5f, 0.0f);
        REQUIRE(call_count == 1);
    }

    tm.cancel_all();
}

TEST_CASE_METHOD(TimerTestFixture, "TimerManager sequence builder", "[core][timer]") {
    auto& tm = timers();
    tm.cancel_all();

    SECTION("Simple sequence execution") {
        std::vector<int> order;

        auto handle = tm.sequence()
            .then([&]() { order.push_back(1); })
            .delay(0.1f)
            .then([&]() { order.push_back(2); })
            .delay(0.1f)
            .then([&]() { order.push_back(3); })
            .start();

        REQUIRE(handle.valid());

        tm.update(0.05f);
        REQUIRE(order.size() >= 1);
        REQUIRE(order[0] == 1);

        tm.update(0.1f);
        REQUIRE(order.size() >= 2);
        REQUIRE(order[1] == 2);

        tm.update(0.1f);
        REQUIRE(order.size() == 3);
        REQUIRE(order[2] == 3);
    }

    tm.cancel_all();
}

TEST_CASE_METHOD(TimerTestFixture, "TimerManager statistics", "[core][timer]") {
    auto& tm = timers();
    tm.cancel_all();

    SECTION("Stats track active timers") {
        auto stats_before = tm.get_stats();

        auto h1 = tm.set_timeout(1.0f, []() {});
        auto h2 = tm.set_timeout(1.0f, []() {});

        auto stats_after = tm.get_stats();
        REQUIRE(stats_after.active_timers == stats_before.active_timers + 2);

        tm.cancel(h1);
        auto stats_after_cancel = tm.get_stats();
        // Note: might be cleaned up on next update
        tm.update(0.0f);
    }

    tm.cancel_all();
}
