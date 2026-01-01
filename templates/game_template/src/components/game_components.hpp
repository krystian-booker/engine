#pragma once

#include <engine/core/math.hpp>

// Custom game components
// These are registered with the reflection system in MyGame::register_components()

// Player controller component - controls player movement
struct PlayerController {
    float move_speed = 5.0f;
    float jump_force = 10.0f;
    bool is_grounded = true;

    engine::core::Vec3 velocity{0.0f, 0.0f, 0.0f};
};

// Health component - tracks entity health
struct Health {
    float current = 100.0f;
    float max = 100.0f;

    Health() = default;
    Health(float current_, float max_) : current(current_), max(max_) {}

    bool is_alive() const { return current > 0.0f; }
    float percent() const { return max > 0.0f ? current / max : 0.0f; }

    void take_damage(float amount) {
        current -= amount;
        if (current < 0.0f) current = 0.0f;
    }

    void heal(float amount) {
        current += amount;
        if (current > max) current = max;
    }
};

// Collectible component - items that can be picked up
struct Collectible {
    enum class Type {
        HealthPack,
        Coin,
        PowerUp
    };

    Type type = Type::Coin;
    float value = 1.0f;
    bool collected = false;
};
