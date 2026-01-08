#include <catch2/catch_test_macros.hpp>
#include <engine/physics/layers.hpp>

using namespace engine::physics;

TEST_CASE("Layer constants", "[physics][layers]") {
    REQUIRE(layers::STATIC == 0);
    REQUIRE(layers::DYNAMIC == 1);
    REQUIRE(layers::PLAYER == 2);
    REQUIRE(layers::ENEMY == 3);
    REQUIRE(layers::TRIGGER == 4);
    REQUIRE(layers::DEBRIS == 5);
    REQUIRE(layers::PROJECTILE == 6);
    REQUIRE(layers::USER_START == 8);
    REQUIRE(layers::MAX_LAYERS == 16);
}

TEST_CASE("CollisionFilter default", "[physics][layers]") {
    CollisionFilter filter;

    SECTION("Everything collides by default") {
        REQUIRE(filter.should_collide(layers::STATIC, layers::DYNAMIC) == true);
        REQUIRE(filter.should_collide(layers::PLAYER, layers::ENEMY) == true);
        REQUIRE(filter.should_collide(layers::TRIGGER, layers::DEBRIS) == true);
    }

    SECTION("Same layer collides with itself") {
        REQUIRE(filter.should_collide(layers::DYNAMIC, layers::DYNAMIC) == true);
        REQUIRE(filter.should_collide(layers::PLAYER, layers::PLAYER) == true);
    }
}

TEST_CASE("CollisionFilter set_collision", "[physics][layers]") {
    CollisionFilter filter;

    SECTION("Disable collision between two layers") {
        filter.set_collision(layers::PLAYER, layers::DEBRIS, false);

        REQUIRE(filter.should_collide(layers::PLAYER, layers::DEBRIS) == false);
        REQUIRE(filter.should_collide(layers::DEBRIS, layers::PLAYER) == false);
    }

    SECTION("Re-enable collision") {
        filter.set_collision(layers::PLAYER, layers::ENEMY, false);
        filter.set_collision(layers::PLAYER, layers::ENEMY, true);

        REQUIRE(filter.should_collide(layers::PLAYER, layers::ENEMY) == true);
    }

    SECTION("Other layers unaffected") {
        filter.set_collision(layers::PLAYER, layers::ENEMY, false);

        REQUIRE(filter.should_collide(layers::PLAYER, layers::STATIC) == true);
        REQUIRE(filter.should_collide(layers::ENEMY, layers::DYNAMIC) == true);
    }
}

TEST_CASE("CollisionFilter set_layer_collisions", "[physics][layers]") {
    CollisionFilter filter;

    SECTION("Disable all collisions for a layer") {
        filter.set_layer_collisions(layers::DEBRIS, false);

        REQUIRE(filter.should_collide(layers::DEBRIS, layers::STATIC) == false);
        REQUIRE(filter.should_collide(layers::DEBRIS, layers::DYNAMIC) == false);
        REQUIRE(filter.should_collide(layers::DEBRIS, layers::PLAYER) == false);
        REQUIRE(filter.should_collide(layers::STATIC, layers::DEBRIS) == false);
    }

    SECTION("Re-enable all collisions for a layer") {
        filter.set_layer_collisions(layers::DEBRIS, false);
        filter.set_layer_collisions(layers::DEBRIS, true);

        REQUIRE(filter.should_collide(layers::DEBRIS, layers::STATIC) == true);
        REQUIRE(filter.should_collide(layers::DEBRIS, layers::DYNAMIC) == true);
    }
}

TEST_CASE("CollisionFilter boundary checks", "[physics][layers]") {
    CollisionFilter filter;

    SECTION("Invalid layer returns false") {
        REQUIRE(filter.should_collide(layers::MAX_LAYERS, 0) == false);
        REQUIRE(filter.should_collide(0, layers::MAX_LAYERS) == false);
        REQUIRE(filter.should_collide(100, 200) == false);
    }

    SECTION("Setting invalid layer is safe") {
        // Should not crash
        filter.set_collision(layers::MAX_LAYERS, 0, false);
        filter.set_layer_collisions(layers::MAX_LAYERS, false);
    }
}

TEST_CASE("CollisionFilter typical game setup", "[physics][layers]") {
    CollisionFilter filter;

    // Typical game collision setup:
    // - Triggers don't physically collide with anything
    // - Debris doesn't collide with other debris
    // - Projectiles don't collide with their shooter (player)

    filter.set_layer_collisions(layers::TRIGGER, false);
    filter.set_collision(layers::DEBRIS, layers::DEBRIS, false);
    filter.set_collision(layers::PROJECTILE, layers::PLAYER, false);

    SECTION("Triggers don't collide") {
        REQUIRE(filter.should_collide(layers::TRIGGER, layers::PLAYER) == false);
        REQUIRE(filter.should_collide(layers::TRIGGER, layers::ENEMY) == false);
        REQUIRE(filter.should_collide(layers::TRIGGER, layers::STATIC) == false);
    }

    SECTION("Debris doesn't collide with debris") {
        REQUIRE(filter.should_collide(layers::DEBRIS, layers::DEBRIS) == false);
        REQUIRE(filter.should_collide(layers::DEBRIS, layers::STATIC) == true);
    }

    SECTION("Player projectiles pass through player") {
        REQUIRE(filter.should_collide(layers::PROJECTILE, layers::PLAYER) == false);
        REQUIRE(filter.should_collide(layers::PROJECTILE, layers::ENEMY) == true);
    }
}
