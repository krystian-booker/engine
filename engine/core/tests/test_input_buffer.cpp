#include <catch2/catch_test_macros.hpp>
#include <engine/core/input_buffer.hpp>

using namespace engine::core;

TEST_CASE("InputBuffer buffering and consumption", "[core][input]") {
    InputBuffer buffer;

    SECTION("Buffer action and check existence") {
        REQUIRE_FALSE(buffer.has("attack"));

        buffer.buffer("attack");

        REQUIRE(buffer.has("attack"));
        REQUIRE(buffer.count() == 1);
    }

    SECTION("Consume removes buffered action") {
        buffer.buffer("attack");
        REQUIRE(buffer.has("attack"));

        bool consumed = buffer.consume("attack");

        REQUIRE(consumed);
        REQUIRE_FALSE(buffer.has("attack"));
        REQUIRE(buffer.count() == 0);
    }

    SECTION("Has returns false after consumption") {
        buffer.buffer("dodge");

        REQUIRE(buffer.has("dodge"));
        buffer.consume("dodge");
        REQUIRE_FALSE(buffer.has("dodge"));
    }

    SECTION("Multiple actions can be buffered simultaneously") {
        buffer.buffer("attack");
        buffer.buffer("dodge");
        buffer.buffer("jump");

        REQUIRE(buffer.has("attack"));
        REQUIRE(buffer.has("dodge"));
        REQUIRE(buffer.has("jump"));
        REQUIRE(buffer.count() == 3);
    }

    SECTION("Consume returns false for non-existent action") {
        buffer.buffer("attack");

        bool consumed = buffer.consume("dodge");

        REQUIRE_FALSE(consumed);
        REQUIRE(buffer.has("attack"));
    }

    SECTION("Peek returns oldest action without consuming") {
        buffer.buffer("first");
        buffer.buffer("second");

        REQUIRE(buffer.peek() == "first");
        REQUIRE(buffer.count() == 2);
        REQUIRE(buffer.peek() == "first");  // Still there
    }

    SECTION("Consume oldest removes and returns first action") {
        buffer.buffer("first");
        buffer.buffer("second");

        std::string oldest = buffer.consume_oldest();

        REQUIRE(oldest == "first");
        REQUIRE(buffer.count() == 1);
        REQUIRE(buffer.peek() == "second");
    }
}

TEST_CASE("InputBuffer expiration", "[core][input]") {
    InputBuffer::Config config;
    config.default_buffer_duration = 0.1f;  // Short duration for testing
    InputBuffer buffer(config);

    SECTION("Buffered action expires after duration") {
        buffer.buffer("attack");
        REQUIRE(buffer.has("attack"));

        // Advance time past expiration
        buffer.update(0.15f);

        REQUIRE_FALSE(buffer.has("attack"));
        REQUIRE(buffer.empty());
    }

    SECTION("Custom duration is respected") {
        buffer.buffer("attack", 0.5f);  // Longer custom duration

        buffer.update(0.3f);
        REQUIRE(buffer.has("attack"));  // Still alive

        buffer.update(0.3f);
        REQUIRE_FALSE(buffer.has("attack"));  // Now expired
    }

    SECTION("Update advances expiration timers") {
        buffer.buffer("attack");

        buffer.update(0.05f);  // Half duration
        REQUIRE(buffer.has("attack"));

        buffer.update(0.05f);  // At duration
        REQUIRE_FALSE(buffer.has("attack"));
    }

    SECTION("Multiple actions expire independently") {
        buffer.buffer("attack", 0.2f);
        buffer.buffer("dodge", 0.1f);

        buffer.update(0.15f);

        REQUIRE(buffer.has("attack"));  // Still alive
        REQUIRE_FALSE(buffer.has("dodge"));  // Expired
    }
}

TEST_CASE("InputBuffer clear operations", "[core][input]") {
    InputBuffer buffer;

    SECTION("Clear removes all buffered actions") {
        buffer.buffer("attack");
        buffer.buffer("dodge");
        buffer.buffer("jump");

        buffer.clear_all();

        REQUIRE(buffer.empty());
        REQUIRE(buffer.count() == 0);
    }

    SECTION("Clear specific action leaves others") {
        buffer.buffer("attack");
        buffer.buffer("dodge");
        buffer.buffer("jump");

        buffer.clear("dodge");

        REQUIRE(buffer.has("attack"));
        REQUIRE_FALSE(buffer.has("dodge"));
        REQUIRE(buffer.has("jump"));
        REQUIRE(buffer.count() == 2);
    }

    SECTION("Clear non-existent action is safe") {
        buffer.buffer("attack");

        buffer.clear("non_existent");

        REQUIRE(buffer.has("attack"));
        REQUIRE(buffer.count() == 1);
    }
}

TEST_CASE("InputBuffer configuration", "[core][input]") {
    SECTION("Max buffered inputs is enforced") {
        InputBuffer::Config config;
        config.max_buffered_inputs = 3;
        InputBuffer buffer(config);

        buffer.buffer("action1");
        buffer.buffer("action2");
        buffer.buffer("action3");
        buffer.buffer("action4");  // Should evict oldest

        REQUIRE(buffer.count() == 3);
        REQUIRE_FALSE(buffer.has("action1"));  // Evicted
        REQUIRE(buffer.has("action2"));
        REQUIRE(buffer.has("action3"));
        REQUIRE(buffer.has("action4"));
    }

    SECTION("Duplicate handling when allow_duplicates is false") {
        InputBuffer::Config config;
        config.allow_duplicates = false;
        InputBuffer buffer(config);

        buffer.buffer("attack", 0.5f);
        buffer.buffer("attack", 0.5f);  // Should refresh, not add

        REQUIRE(buffer.count() == 1);
    }

    SECTION("Duplicate handling when allow_duplicates is true") {
        InputBuffer::Config config;
        config.allow_duplicates = true;
        InputBuffer buffer(config);

        buffer.buffer("attack");
        buffer.buffer("attack");

        REQUIRE(buffer.count() == 2);
    }

    SECTION("Duplicate refresh extends duration") {
        InputBuffer::Config config;
        config.allow_duplicates = false;
        config.default_buffer_duration = 0.1f;
        InputBuffer buffer(config);

        buffer.buffer("attack");
        buffer.update(0.08f);  // Almost expired

        buffer.buffer("attack");  // Refresh
        buffer.update(0.08f);  // Would have expired without refresh

        REQUIRE(buffer.has("attack"));  // Still alive due to refresh
    }
}

TEST_CASE("InputBuffer get_all", "[core][input]") {
    InputBuffer buffer;

    SECTION("Returns empty vector when buffer is empty") {
        auto all = buffer.get_all();
        REQUIRE(all.empty());
    }

    SECTION("Returns all buffered actions in order") {
        buffer.buffer("attack");
        buffer.buffer("dodge");
        buffer.buffer("jump");

        auto all = buffer.get_all();

        REQUIRE(all.size() == 3);
        REQUIRE(all[0] == "attack");
        REQUIRE(all[1] == "dodge");
        REQUIRE(all[2] == "jump");
    }
}

TEST_CASE("InputBuffer empty state", "[core][input]") {
    InputBuffer buffer;

    SECTION("New buffer is empty") {
        REQUIRE(buffer.empty());
        REQUIRE(buffer.count() == 0);
    }

    SECTION("Peek on empty buffer returns empty string") {
        REQUIRE(buffer.peek().empty());
    }

    SECTION("Consume oldest on empty buffer returns empty string") {
        REQUIRE(buffer.consume_oldest().empty());
    }
}
