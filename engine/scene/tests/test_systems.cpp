#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/scene/systems.hpp>

using namespace engine::scene;
using Catch::Matchers::WithinAbs;

TEST_CASE("Phase enum values", "[scene][systems]") {
    REQUIRE(static_cast<int>(Phase::PreUpdate) == 0);
    REQUIRE(static_cast<int>(Phase::FixedUpdate) == 1);
    REQUIRE(static_cast<int>(Phase::Update) == 2);
    REQUIRE(static_cast<int>(Phase::PostUpdate) == 3);
    REQUIRE(static_cast<int>(Phase::PreRender) == 4);
    REQUIRE(static_cast<int>(Phase::Render) == 5);
    REQUIRE(static_cast<int>(Phase::PostRender) == 6);
}

TEST_CASE("Scheduler basic operations", "[scene][systems]") {
    Scheduler scheduler;
    World world;

    SECTION("Add anonymous system") {
        int call_count = 0;
        scheduler.add(Phase::Update, [&](World&, double) {
            call_count++;
        });

        scheduler.run(world, 0.016, Phase::Update);
        REQUIRE(call_count == 1);
    }

    SECTION("Add named system") {
        int call_count = 0;
        scheduler.add(Phase::Update, [&](World&, double) {
            call_count++;
        }, "TestSystem");

        scheduler.run(world, 0.016, Phase::Update);
        REQUIRE(call_count == 1);
    }

    SECTION("Run correct phase") {
        int update_count = 0;
        int render_count = 0;

        scheduler.add(Phase::Update, [&](World&, double) {
            update_count++;
        }, "UpdateSystem");

        scheduler.add(Phase::Render, [&](World&, double) {
            render_count++;
        }, "RenderSystem");

        scheduler.run(world, 0.016, Phase::Update);
        REQUIRE(update_count == 1);
        REQUIRE(render_count == 0);

        scheduler.run(world, 0.016, Phase::Render);
        REQUIRE(update_count == 1);
        REQUIRE(render_count == 1);
    }
}

TEST_CASE("Scheduler system priority", "[scene][systems]") {
    Scheduler scheduler;
    World world;
    std::vector<int> execution_order;

    scheduler.add(Phase::Update, [&](World&, double) {
        execution_order.push_back(1);
    }, "LowPriority", 0);

    scheduler.add(Phase::Update, [&](World&, double) {
        execution_order.push_back(2);
    }, "HighPriority", 100);

    scheduler.add(Phase::Update, [&](World&, double) {
        execution_order.push_back(3);
    }, "MediumPriority", 50);

    scheduler.run(world, 0.016, Phase::Update);

    REQUIRE(execution_order.size() == 3);
    // Higher priority should run first
    REQUIRE(execution_order[0] == 2); // HighPriority (100)
    REQUIRE(execution_order[1] == 3); // MediumPriority (50)
    REQUIRE(execution_order[2] == 1); // LowPriority (0)
}

TEST_CASE("Scheduler delta time", "[scene][systems]") {
    Scheduler scheduler;
    World world;
    double received_dt = 0.0;

    scheduler.add(Phase::Update, [&](World&, double dt) {
        received_dt = dt;
    }, "DtSystem");

    scheduler.run(world, 0.016, Phase::Update);
    REQUIRE_THAT(received_dt, WithinAbs(0.016, 0.0001));

    scheduler.run(world, 0.033, Phase::Update);
    REQUIRE_THAT(received_dt, WithinAbs(0.033, 0.0001));
}

TEST_CASE("Scheduler world access", "[scene][systems]") {
    Scheduler scheduler;
    World world;

    Entity e = world.create();
    world.emplace<EntityInfo>(e).name = "TestEntity";

    SECTION("System can read world") {
        std::string found_name;
        scheduler.add(Phase::Update, [&](World& w, double) {
            auto view = w.view<EntityInfo>();
            for (auto entity : view) {
                found_name = view.get<EntityInfo>(entity).name;
            }
        }, "ReadSystem");

        scheduler.run(world, 0.016, Phase::Update);
        REQUIRE(found_name == "TestEntity");
    }

    SECTION("System can modify world") {
        scheduler.add(Phase::Update, [&](World& w, double) {
            auto view = w.view<EntityInfo>();
            for (auto entity : view) {
                view.get<EntityInfo>(entity).name = "Modified";
            }
        }, "WriteSystem");

        scheduler.run(world, 0.016, Phase::Update);
        REQUIRE(world.get<EntityInfo>(e).name == "Modified");
    }
}

TEST_CASE("Scheduler remove system", "[scene][systems]") {
    Scheduler scheduler;
    World world;
    int call_count = 0;

    scheduler.add(Phase::Update, [&](World&, double) {
        call_count++;
    }, "RemovableSystem");

    scheduler.run(world, 0.016, Phase::Update);
    REQUIRE(call_count == 1);

    scheduler.remove("RemovableSystem");

    scheduler.run(world, 0.016, Phase::Update);
    REQUIRE(call_count == 1); // Should not increase
}

TEST_CASE("Scheduler enable/disable system", "[scene][systems]") {
    Scheduler scheduler;
    World world;
    int call_count = 0;

    scheduler.add(Phase::Update, [&](World&, double) {
        call_count++;
    }, "ToggleSystem");

    SECTION("System enabled by default") {
        REQUIRE(scheduler.is_enabled("ToggleSystem"));
        scheduler.run(world, 0.016, Phase::Update);
        REQUIRE(call_count == 1);
    }

    SECTION("Disable system") {
        scheduler.set_enabled("ToggleSystem", false);
        REQUIRE_FALSE(scheduler.is_enabled("ToggleSystem"));

        scheduler.run(world, 0.016, Phase::Update);
        REQUIRE(call_count == 0);
    }

    SECTION("Re-enable system") {
        scheduler.set_enabled("ToggleSystem", false);
        scheduler.run(world, 0.016, Phase::Update);
        REQUIRE(call_count == 0);

        scheduler.set_enabled("ToggleSystem", true);
        scheduler.run(world, 0.016, Phase::Update);
        REQUIRE(call_count == 1);
    }
}

TEST_CASE("Scheduler clear", "[scene][systems]") {
    Scheduler scheduler;
    World world;
    int count1 = 0, count2 = 0;

    scheduler.add(Phase::Update, [&](World&, double) { count1++; }, "System1");
    scheduler.add(Phase::Render, [&](World&, double) { count2++; }, "System2");

    scheduler.clear();

    scheduler.run(world, 0.016, Phase::Update);
    scheduler.run(world, 0.016, Phase::Render);

    REQUIRE(count1 == 0);
    REQUIRE(count2 == 0);
}

TEST_CASE("Scheduler multiple systems same phase", "[scene][systems]") {
    Scheduler scheduler;
    World world;
    int total = 0;

    scheduler.add(Phase::Update, [&](World&, double) { total += 1; }, "System1");
    scheduler.add(Phase::Update, [&](World&, double) { total += 10; }, "System2");
    scheduler.add(Phase::Update, [&](World&, double) { total += 100; }, "System3");

    scheduler.run(world, 0.016, Phase::Update);
    REQUIRE(total == 111);
}

TEST_CASE("Scheduler all phases", "[scene][systems]") {
    Scheduler scheduler;
    World world;
    std::vector<Phase> executed_phases;

    auto add_tracker = [&](Phase phase) {
        scheduler.add(phase, [&, phase](World&, double) {
            executed_phases.push_back(phase);
        });
    };

    add_tracker(Phase::PreUpdate);
    add_tracker(Phase::FixedUpdate);
    add_tracker(Phase::Update);
    add_tracker(Phase::PostUpdate);
    add_tracker(Phase::PreRender);
    add_tracker(Phase::Render);
    add_tracker(Phase::PostRender);

    // Run all phases in order
    scheduler.run(world, 0.016, Phase::PreUpdate);
    scheduler.run(world, 0.016, Phase::FixedUpdate);
    scheduler.run(world, 0.016, Phase::Update);
    scheduler.run(world, 0.016, Phase::PostUpdate);
    scheduler.run(world, 0.016, Phase::PreRender);
    scheduler.run(world, 0.016, Phase::Render);
    scheduler.run(world, 0.016, Phase::PostRender);

    REQUIRE(executed_phases.size() == 7);
    REQUIRE(executed_phases[0] == Phase::PreUpdate);
    REQUIRE(executed_phases[1] == Phase::FixedUpdate);
    REQUIRE(executed_phases[2] == Phase::Update);
    REQUIRE(executed_phases[3] == Phase::PostUpdate);
    REQUIRE(executed_phases[4] == Phase::PreRender);
    REQUIRE(executed_phases[5] == Phase::Render);
    REQUIRE(executed_phases[6] == Phase::PostRender);
}
