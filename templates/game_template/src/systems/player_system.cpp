#include "player_system.hpp"
#include "../components/game_components.hpp"

#include <engine/scene/world.hpp>
#include <engine/scene/components.hpp>
#include <engine/core/input.hpp>

void player_movement_system(engine::scene::World& world, double dt) {
    using namespace engine::scene;
    using namespace engine::core;

    float delta = static_cast<float>(dt);

    // Process all entities that have both PlayerController and LocalTransform
    auto view = world.view<PlayerController, LocalTransform>();

    for (auto entity : view) {
        auto& controller = view.get<PlayerController>(entity);
        auto& transform = view.get<LocalTransform>(entity);

        // Get input (simplified - in real game, use Input system)
        Vec3 input_dir{0.0f, 0.0f, 0.0f};

        // Example: Check keyboard input
        // if (Input::key_held(Key::W)) input_dir.z -= 1.0f;
        // if (Input::key_held(Key::S)) input_dir.z += 1.0f;
        // if (Input::key_held(Key::A)) input_dir.x -= 1.0f;
        // if (Input::key_held(Key::D)) input_dir.x += 1.0f;

        // Normalize input direction
        float len = std::sqrt(input_dir.x * input_dir.x +
                             input_dir.y * input_dir.y +
                             input_dir.z * input_dir.z);
        if (len > 0.001f) {
            input_dir.x /= len;
            input_dir.y /= len;
            input_dir.z /= len;
        }

        // Apply movement
        controller.velocity.x = input_dir.x * controller.move_speed;
        controller.velocity.z = input_dir.z * controller.move_speed;

        // Apply gravity if not grounded
        if (!controller.is_grounded) {
            controller.velocity.y -= 9.81f * delta;
        }

        // Update position
        transform.position.x += controller.velocity.x * delta;
        transform.position.y += controller.velocity.y * delta;
        transform.position.z += controller.velocity.z * delta;

        // Simple ground check (y = 0 is ground)
        if (transform.position.y < 0.0f) {
            transform.position.y = 0.0f;
            controller.velocity.y = 0.0f;
            controller.is_grounded = true;
        }
    }
}

void health_system(engine::scene::World& world, double /*dt*/) {
    using namespace engine::scene;

    // Process all entities with Health component
    auto view = world.view<Health>();

    for (auto entity : view) {
        auto& health = view.get<Health>(entity);

        // Check for death
        if (!health.is_alive()) {
            // Entity is dead - could trigger death animation, respawn, etc.
            // For now, just log or handle death
        }

        // Could add health regeneration, poison damage, etc.
    }
}
