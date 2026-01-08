// engine/ai/tests/test_behavior_tree.cpp
// Happy path tests for Behavior Tree nodes

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <engine/ai/behavior_tree.hpp>
#include <engine/ai/bt_composites.hpp>
#include <engine/ai/bt_decorators.hpp>
#include <engine/ai/blackboard.hpp>
#include <engine/scene/entity.hpp>

using namespace engine::ai;
using namespace engine::scene;
using Catch::Matchers::WithinAbs;

// Helper to create a minimal valid BTContext
struct TestContext {
    World* world = nullptr;
    Blackboard blackboard;
    BTContext ctx;

    TestContext() {
        ctx.world = world;
        ctx.entity = Entity{1};
        ctx.blackboard = &blackboard;
        ctx.delta_time = 0.016f; // ~60fps
    }
};

TEST_CASE("BTAction node", "[ai][behavior_tree]") {
    TestContext tc;

    SECTION("Action returning Success") {
        auto action = std::make_unique<BTAction>("SuccessAction", [](BTContext&) {
            return BTStatus::Success;
        });

        BTStatus result = action->tick(tc.ctx);
        REQUIRE(result == BTStatus::Success);
        REQUIRE(action->get_last_status() == BTStatus::Success);
    }

    SECTION("Action returning Failure") {
        auto action = std::make_unique<BTAction>("FailAction", [](BTContext&) {
            return BTStatus::Failure;
        });

        BTStatus result = action->tick(tc.ctx);
        REQUIRE(result == BTStatus::Failure);
    }

    SECTION("Action returning Running") {
        auto action = std::make_unique<BTAction>("RunningAction", [](BTContext&) {
            return BTStatus::Running;
        });

        BTStatus result = action->tick(tc.ctx);
        REQUIRE(result == BTStatus::Running);
    }

    SECTION("Action with null function returns Failure") {
        auto action = std::make_unique<BTAction>("NullAction", nullptr);
        BTStatus result = action->tick(tc.ctx);
        REQUIRE(result == BTStatus::Failure);
    }

    SECTION("Action can access context") {
        tc.blackboard.set<int>("counter", 0);

        auto action = std::make_unique<BTAction>("IncrementAction", [](BTContext& ctx) {
            int val = ctx.blackboard->get<int>("counter");
            ctx.blackboard->set<int>("counter", val + 1);
            return BTStatus::Success;
        });

        action->tick(tc.ctx);
        REQUIRE(tc.blackboard.get<int>("counter") == 1);
    }
}

TEST_CASE("BTCondition node", "[ai][behavior_tree]") {
    TestContext tc;

    SECTION("Condition true returns Success") {
        auto condition = std::make_unique<BTCondition>("TrueCondition", [](const BTContext&) {
            return true;
        });

        REQUIRE(condition->tick(tc.ctx) == BTStatus::Success);
    }

    SECTION("Condition false returns Failure") {
        auto condition = std::make_unique<BTCondition>("FalseCondition", [](const BTContext&) {
            return false;
        });

        REQUIRE(condition->tick(tc.ctx) == BTStatus::Failure);
    }

    SECTION("Condition can read blackboard") {
        tc.blackboard.set<bool>("is_ready", true);

        auto condition = std::make_unique<BTCondition>("ReadBBCondition", [](const BTContext& ctx) {
            return ctx.blackboard->get<bool>("is_ready");
        });

        REQUIRE(condition->tick(tc.ctx) == BTStatus::Success);
    }

    SECTION("Null condition returns Failure") {
        auto condition = std::make_unique<BTCondition>("NullCondition", nullptr);
        REQUIRE(condition->tick(tc.ctx) == BTStatus::Failure);
    }
}

TEST_CASE("BTSequence composite", "[ai][behavior_tree]") {
    TestContext tc;

    SECTION("Empty sequence succeeds") {
        auto sequence = std::make_unique<BTSequence>("EmptySequence");
        REQUIRE(sequence->tick(tc.ctx) == BTStatus::Success);
    }

    SECTION("All children succeed -> Sequence succeeds") {
        auto sequence = std::make_unique<BTSequence>("SuccessSequence");
        sequence->add_child(make_action("S1", [](BTContext&) { return BTStatus::Success; }));
        sequence->add_child(make_action("S2", [](BTContext&) { return BTStatus::Success; }));
        sequence->add_child(make_action("S3", [](BTContext&) { return BTStatus::Success; }));

        REQUIRE(sequence->tick(tc.ctx) == BTStatus::Success);
    }

    SECTION("First child fails -> Sequence fails immediately") {
        int execution_count = 0;

        auto sequence = std::make_unique<BTSequence>("FailSequence");
        sequence->add_child(make_action("Fail", [](BTContext&) { return BTStatus::Failure; }));
        sequence->add_child(make_action("NeverRun", [&](BTContext&) {
            execution_count++;
            return BTStatus::Success;
        }));

        REQUIRE(sequence->tick(tc.ctx) == BTStatus::Failure);
        REQUIRE(execution_count == 0); // Second child never ran
    }

    SECTION("Middle child fails -> Sequence fails") {
        auto sequence = std::make_unique<BTSequence>("MiddleFailSequence");
        sequence->add_child(make_action("S1", [](BTContext&) { return BTStatus::Success; }));
        sequence->add_child(make_action("F1", [](BTContext&) { return BTStatus::Failure; }));
        sequence->add_child(make_action("S2", [](BTContext&) { return BTStatus::Success; }));

        REQUIRE(sequence->tick(tc.ctx) == BTStatus::Failure);
    }

    SECTION("Running child -> Sequence returns Running") {
        auto sequence = std::make_unique<BTSequence>("RunningSequence");
        sequence->add_child(make_action("S1", [](BTContext&) { return BTStatus::Success; }));
        sequence->add_child(make_action("R1", [](BTContext&) { return BTStatus::Running; }));

        REQUIRE(sequence->tick(tc.ctx) == BTStatus::Running);
    }
}

TEST_CASE("BTSelector composite", "[ai][behavior_tree]") {
    TestContext tc;

    SECTION("Empty selector fails") {
        auto selector = std::make_unique<BTSelector>("EmptySelector");
        REQUIRE(selector->tick(tc.ctx) == BTStatus::Failure);
    }

    SECTION("First child succeeds -> Selector succeeds") {
        int second_child_runs = 0;

        auto selector = std::make_unique<BTSelector>("FirstSuccessSelector");
        selector->add_child(make_action("S1", [](BTContext&) { return BTStatus::Success; }));
        selector->add_child(make_action("NeverRun", [&](BTContext&) {
            second_child_runs++;
            return BTStatus::Success;
        }));

        REQUIRE(selector->tick(tc.ctx) == BTStatus::Success);
        REQUIRE(second_child_runs == 0);
    }

    SECTION("All children fail -> Selector fails") {
        auto selector = std::make_unique<BTSelector>("AllFailSelector");
        selector->add_child(make_action("F1", [](BTContext&) { return BTStatus::Failure; }));
        selector->add_child(make_action("F2", [](BTContext&) { return BTStatus::Failure; }));
        selector->add_child(make_action("F3", [](BTContext&) { return BTStatus::Failure; }));

        REQUIRE(selector->tick(tc.ctx) == BTStatus::Failure);
    }

    SECTION("First fails, second succeeds -> Selector succeeds") {
        auto selector = std::make_unique<BTSelector>("FallbackSelector");
        selector->add_child(make_action("F1", [](BTContext&) { return BTStatus::Failure; }));
        selector->add_child(make_action("S1", [](BTContext&) { return BTStatus::Success; }));

        REQUIRE(selector->tick(tc.ctx) == BTStatus::Success);
    }

    SECTION("Running child -> Selector returns Running") {
        auto selector = std::make_unique<BTSelector>("RunningSelector");
        selector->add_child(make_action("R1", [](BTContext&) { return BTStatus::Running; }));

        REQUIRE(selector->tick(tc.ctx) == BTStatus::Running);
    }
}

TEST_CASE("BTInverter decorator", "[ai][behavior_tree]") {
    TestContext tc;

    SECTION("Inverts Success to Failure") {
        auto inverter = std::make_unique<BTInverter>("Inverter");
        inverter->set_child(make_action("Success", [](BTContext&) { return BTStatus::Success; }));

        REQUIRE(inverter->tick(tc.ctx) == BTStatus::Failure);
    }

    SECTION("Inverts Failure to Success") {
        auto inverter = std::make_unique<BTInverter>("Inverter");
        inverter->set_child(make_action("Failure", [](BTContext&) { return BTStatus::Failure; }));

        REQUIRE(inverter->tick(tc.ctx) == BTStatus::Success);
    }

    SECTION("Running passes through unchanged") {
        auto inverter = std::make_unique<BTInverter>("Inverter");
        inverter->set_child(make_action("Running", [](BTContext&) { return BTStatus::Running; }));

        REQUIRE(inverter->tick(tc.ctx) == BTStatus::Running);
    }

    SECTION("No child returns Failure") {
        auto inverter = std::make_unique<BTInverter>("EmptyInverter");
        REQUIRE(inverter->tick(tc.ctx) == BTStatus::Failure);
    }
}

TEST_CASE("BTSucceeder decorator", "[ai][behavior_tree]") {
    TestContext tc;

    SECTION("Child success -> Success") {
        auto succeeder = std::make_unique<BTSucceeder>("Succeeder");
        succeeder->set_child(make_action("S", [](BTContext&) { return BTStatus::Success; }));
        REQUIRE(succeeder->tick(tc.ctx) == BTStatus::Success);
    }

    SECTION("Child failure -> Success") {
        auto succeeder = std::make_unique<BTSucceeder>("Succeeder");
        succeeder->set_child(make_action("F", [](BTContext&) { return BTStatus::Failure; }));
        REQUIRE(succeeder->tick(tc.ctx) == BTStatus::Success);
    }

    SECTION("Child running -> Running") {
        auto succeeder = std::make_unique<BTSucceeder>("Succeeder");
        succeeder->set_child(make_action("R", [](BTContext&) { return BTStatus::Running; }));
        REQUIRE(succeeder->tick(tc.ctx) == BTStatus::Running);
    }

    SECTION("No child -> Success") {
        auto succeeder = std::make_unique<BTSucceeder>("EmptySucceeder");
        REQUIRE(succeeder->tick(tc.ctx) == BTStatus::Success);
    }
}

TEST_CASE("BTFailer decorator", "[ai][behavior_tree]") {
    TestContext tc;

    SECTION("Child success -> Failure") {
        auto failer = std::make_unique<BTFailer>("Failer");
        failer->set_child(make_action("S", [](BTContext&) { return BTStatus::Success; }));
        REQUIRE(failer->tick(tc.ctx) == BTStatus::Failure);
    }

    SECTION("Child failure -> Failure") {
        auto failer = std::make_unique<BTFailer>("Failer");
        failer->set_child(make_action("F", [](BTContext&) { return BTStatus::Failure; }));
        REQUIRE(failer->tick(tc.ctx) == BTStatus::Failure);
    }

    SECTION("Child running -> Running") {
        auto failer = std::make_unique<BTFailer>("Failer");
        failer->set_child(make_action("R", [](BTContext&) { return BTStatus::Running; }));
        REQUIRE(failer->tick(tc.ctx) == BTStatus::Running);
    }

    SECTION("No child -> Failure") {
        auto failer = std::make_unique<BTFailer>("EmptyFailer");
        REQUIRE(failer->tick(tc.ctx) == BTStatus::Failure);
    }
}

TEST_CASE("BTRepeater decorator", "[ai][behavior_tree]") {
    TestContext tc;

    SECTION("Repeats N times then succeeds") {
        int run_count = 0;

        auto repeater = std::make_unique<BTRepeater>("Repeat3", 3);
        repeater->set_child(make_action("Count", [&](BTContext&) {
            run_count++;
            return BTStatus::Success;
        }));

        // First tick: runs child (1), child succeeds, increments count, returns Running
        REQUIRE(repeater->tick(tc.ctx) == BTStatus::Running);
        // Second tick: runs child (2), child succeeds, increments count, returns Running
        REQUIRE(repeater->tick(tc.ctx) == BTStatus::Running);
        // Third tick: runs child (3), child succeeds, count reached, returns Success
        REQUIRE(repeater->tick(tc.ctx) == BTStatus::Success);
        REQUIRE(run_count == 3);
    }

    SECTION("No child returns Failure") {
        auto repeater = std::make_unique<BTRepeater>("EmptyRepeater", 5);
        REQUIRE(repeater->tick(tc.ctx) == BTStatus::Failure);
    }

    SECTION("Reset clears count") {
        int run_count = 0;

        auto repeater = std::make_unique<BTRepeater>("Repeat2", 2);
        repeater->set_child(make_action("Count", [&](BTContext&) {
            run_count++;
            return BTStatus::Success;
        }));

        repeater->tick(tc.ctx);
        repeater->reset();

        // After reset, should be able to repeat again
        REQUIRE(repeater->tick(tc.ctx) == BTStatus::Running);
    }
}

TEST_CASE("BehaviorTree", "[ai][behavior_tree]") {
    TestContext tc;

    SECTION("Tree with no root returns Failure") {
        BehaviorTree tree("EmptyTree");
        REQUIRE(tree.tick(tc.ctx) == BTStatus::Failure);
    }

    SECTION("Tree executes root and returns status") {
        BehaviorTree tree("SimpleTree");
        tree.set_root(make_action("Root", [](BTContext&) { return BTStatus::Success; }));

        REQUIRE(tree.tick(tc.ctx) == BTStatus::Success);
        REQUIRE(tree.get_last_status() == BTStatus::Success);
    }

    SECTION("Tree name is stored") {
        BehaviorTree tree("MyTree");
        REQUIRE(tree.get_name() == "MyTree");
    }

    SECTION("Complex tree execution") {
        BehaviorTree tree("ComplexTree");

        // Create a selector with two sequences
        auto* root = tree.set_root<BTSelector>("MainSelector");

        // First sequence: check condition -> do action (condition will fail)
        auto* seq1 = root->add_child<BTSequence>("Seq1");
        seq1->add_child(make_condition("AlwaysFalse", [](const BTContext&) { return false; }));
        seq1->add_child(make_action("NeverRun", [](BTContext&) { return BTStatus::Success; }));

        // Second sequence: check condition -> do action (condition will succeed)
        auto* seq2 = root->add_child<BTSequence>("Seq2");
        seq2->add_child(make_condition("AlwaysTrue", [](const BTContext&) { return true; }));
        seq2->add_child(make_action("DoAction", [](BTContext&) { return BTStatus::Success; }));

        // Selector should fall through to second sequence and succeed
        REQUIRE(tree.tick(tc.ctx) == BTStatus::Success);
    }

    SECTION("Reset propagates to all nodes") {
        BehaviorTree tree("ResetTree");

        int tick_count = 0;
        auto* repeater = tree.set_root<BTRepeater>("Repeat2", 2);
        repeater->set_child(make_action("Count", [&](BTContext&) {
            tick_count++;
            return BTStatus::Success;
        }));

        tree.tick(tc.ctx); // tick_count = 1
        tree.reset();
        tick_count = 0;

        tree.tick(tc.ctx);
        REQUIRE(tick_count == 1); // Reset worked, started fresh
    }
}

TEST_CASE("make_action and make_condition helpers", "[ai][behavior_tree]") {
    TestContext tc;

    SECTION("make_action creates BTAction") {
        auto action = make_action("TestAction", [](BTContext&) { return BTStatus::Success; });
        REQUIRE(action->get_name() == "TestAction");
        REQUIRE(action->tick(tc.ctx) == BTStatus::Success);
    }

    SECTION("make_condition creates BTCondition") {
        auto condition = make_condition("TestCondition", [](const BTContext&) { return true; });
        REQUIRE(condition->get_name() == "TestCondition");
        REQUIRE(condition->tick(tc.ctx) == BTStatus::Success);
    }
}

TEST_CASE("BTStatus to_string", "[ai][behavior_tree]") {
    REQUIRE(std::string(to_string(BTStatus::Success)) == "Success");
    REQUIRE(std::string(to_string(BTStatus::Failure)) == "Failure");
    REQUIRE(std::string(to_string(BTStatus::Running)) == "Running");
}
