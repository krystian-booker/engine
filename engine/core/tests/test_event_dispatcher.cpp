#include <catch2/catch_test_macros.hpp>
#include <engine/core/event_dispatcher.hpp>
#include <string>
#include <atomic>
#include <thread>

using namespace engine::core;

// Test event types
struct TestEvent {
    int value = 0;
};

struct AnotherEvent {
    std::string message;
};

TEST_CASE("EventDispatcher subscription and dispatch", "[core][events]") {
    EventDispatcher dispatcher;

    SECTION("Subscribe and receive event") {
        int received_value = 0;
        auto conn = dispatcher.subscribe<TestEvent>([&](const TestEvent& e) {
            received_value = e.value;
        });

        dispatcher.dispatch(TestEvent{42});
        REQUIRE(received_value == 42);
    }

    SECTION("Multiple subscribers receive event") {
        int sum = 0;
        auto conn1 = dispatcher.subscribe<TestEvent>([&](const TestEvent& e) {
            sum += e.value;
        });
        auto conn2 = dispatcher.subscribe<TestEvent>([&](const TestEvent& e) {
            sum += e.value * 2;
        });

        dispatcher.dispatch(TestEvent{10});
        REQUIRE(sum == 30); // 10 + 20
    }

    SECTION("Different event types are isolated") {
        int test_count = 0;
        int another_count = 0;

        auto conn1 = dispatcher.subscribe<TestEvent>([&](const TestEvent&) {
            test_count++;
        });
        auto conn2 = dispatcher.subscribe<AnotherEvent>([&](const AnotherEvent&) {
            another_count++;
        });

        dispatcher.dispatch(TestEvent{});
        REQUIRE(test_count == 1);
        REQUIRE(another_count == 0);

        dispatcher.dispatch(AnotherEvent{});
        REQUIRE(test_count == 1);
        REQUIRE(another_count == 1);
    }
}

TEST_CASE("ScopedConnection RAII behavior", "[core][events]") {
    EventDispatcher dispatcher;

    SECTION("Connection disconnects on destruction") {
        int received_count = 0;
        {
            auto conn = dispatcher.subscribe<TestEvent>([&](const TestEvent&) {
                received_count++;
            });
            dispatcher.dispatch(TestEvent{});
            REQUIRE(received_count == 1);
        }
        // Connection destroyed, should not receive
        dispatcher.dispatch(TestEvent{});
        REQUIRE(received_count == 1);
    }

    SECTION("Manual disconnect") {
        int received_count = 0;
        auto conn = dispatcher.subscribe<TestEvent>([&](const TestEvent&) {
            received_count++;
        });

        dispatcher.dispatch(TestEvent{});
        REQUIRE(received_count == 1);

        conn.disconnect();

        dispatcher.dispatch(TestEvent{});
        REQUIRE(received_count == 1);
    }

    SECTION("Move connection") {
        int received_count = 0;
        ScopedConnection conn2;
        {
            auto conn1 = dispatcher.subscribe<TestEvent>([&](const TestEvent&) {
                received_count++;
            });
            conn2 = std::move(conn1);
        }
        // conn1 destroyed but conn2 holds the connection
        dispatcher.dispatch(TestEvent{});
        REQUIRE(received_count == 1);
    }

    SECTION("Connection connected() status") {
        ScopedConnection conn;
        REQUIRE_FALSE(conn.connected());

        conn = dispatcher.subscribe<TestEvent>([](const TestEvent&) {});
        REQUIRE(conn.connected());

        conn.disconnect();
        REQUIRE_FALSE(conn.connected());
    }
}

TEST_CASE("EventDispatcher queue and flush", "[core][events]") {
    EventDispatcher dispatcher;

    SECTION("Queued events dispatch on flush") {
        int received_value = 0;
        auto conn = dispatcher.subscribe<TestEvent>([&](const TestEvent& e) {
            received_value = e.value;
        });

        dispatcher.queue(TestEvent{99});
        REQUIRE(received_value == 0); // Not yet dispatched

        dispatcher.flush();
        REQUIRE(received_value == 99);
    }

    SECTION("Multiple queued events") {
        std::vector<int> values;
        auto conn = dispatcher.subscribe<TestEvent>([&](const TestEvent& e) {
            values.push_back(e.value);
        });

        dispatcher.queue(TestEvent{1});
        dispatcher.queue(TestEvent{2});
        dispatcher.queue(TestEvent{3});

        REQUIRE(values.empty());
        REQUIRE(dispatcher.queued_event_count() == 3);

        dispatcher.flush();

        REQUIRE(values.size() == 3);
        REQUIRE(values[0] == 1);
        REQUIRE(values[1] == 2);
        REQUIRE(values[2] == 3);
        REQUIRE(dispatcher.queued_event_count() == 0);
    }

    SECTION("Clear queue without dispatching") {
        int received_count = 0;
        auto conn = dispatcher.subscribe<TestEvent>([&](const TestEvent&) {
            received_count++;
        });

        dispatcher.queue(TestEvent{});
        dispatcher.queue(TestEvent{});
        REQUIRE(dispatcher.has_queued_events());

        dispatcher.clear_queue();
        REQUIRE_FALSE(dispatcher.has_queued_events());

        dispatcher.flush();
        REQUIRE(received_count == 0);
    }
}

TEST_CASE("EventDispatcher handler management", "[core][events]") {
    EventDispatcher dispatcher;

    SECTION("Handler count tracking") {
        REQUIRE(dispatcher.handler_count<TestEvent>() == 0);

        auto conn1 = dispatcher.subscribe<TestEvent>([](const TestEvent&) {});
        REQUIRE(dispatcher.handler_count<TestEvent>() == 1);

        auto conn2 = dispatcher.subscribe<TestEvent>([](const TestEvent&) {});
        REQUIRE(dispatcher.handler_count<TestEvent>() == 2);

        conn1.disconnect();
        REQUIRE(dispatcher.handler_count<TestEvent>() == 1);
    }

    SECTION("Clear handlers for type") {
        auto conn1 = dispatcher.subscribe<TestEvent>([](const TestEvent&) {});
        auto conn2 = dispatcher.subscribe<TestEvent>([](const TestEvent&) {});
        auto conn3 = dispatcher.subscribe<AnotherEvent>([](const AnotherEvent&) {});

        REQUIRE(dispatcher.handler_count<TestEvent>() == 2);
        REQUIRE(dispatcher.handler_count<AnotherEvent>() == 1);

        dispatcher.clear_handlers<TestEvent>();

        REQUIRE(dispatcher.handler_count<TestEvent>() == 0);
        REQUIRE(dispatcher.handler_count<AnotherEvent>() == 1);
    }

    SECTION("Clear all handlers") {
        auto conn1 = dispatcher.subscribe<TestEvent>([](const TestEvent&) {});
        auto conn2 = dispatcher.subscribe<AnotherEvent>([](const AnotherEvent&) {});

        dispatcher.clear_all_handlers();

        REQUIRE(dispatcher.handler_count<TestEvent>() == 0);
        REQUIRE(dispatcher.handler_count<AnotherEvent>() == 0);
    }
}

TEST_CASE("EventDispatcher thread safety", "[core][events]") {
    EventDispatcher dispatcher;
    std::atomic<int> counter{0};

    auto conn = dispatcher.subscribe<TestEvent>([&](const TestEvent& e) {
        counter += e.value;
    });

    SECTION("Concurrent queue from multiple threads") {
        constexpr int num_threads = 4;
        constexpr int events_per_thread = 100;

        std::vector<std::thread> threads;
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&dispatcher]() {
                for (int i = 0; i < events_per_thread; ++i) {
                    dispatcher.queue(TestEvent{1});
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        REQUIRE(dispatcher.queued_event_count() == num_threads * events_per_thread);

        dispatcher.flush();
        REQUIRE(counter == num_threads * events_per_thread);
    }
}
