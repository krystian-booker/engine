#include "game.hpp"
#include "components/game_components.hpp"
#include "systems/player_system.hpp"

#include <engine/scene/world.hpp>
#include <engine/scene/components.hpp>
#include <engine/reflect/type_registry.hpp>
#include <engine/core/log.hpp>

void MyGame::register_components() {
    using namespace engine::reflect;

    // Register custom components so they can be serialized/deserialized
    TypeRegistry::instance().register_component<PlayerController>("PlayerController");
    TypeRegistry::instance().register_component<Health>("Health");
    TypeRegistry::instance().register_component<Collectible>("Collectible");
}

bool MyGame::init(engine::plugin::GameContext* ctx) {
    m_ctx = ctx;

    engine::core::log(engine::core::LogLevel::Info, "{{PROJECT_NAME}} initializing...");

    // Create a simple player entity
    auto player = ctx->world->create();
    ctx->world->emplace<engine::scene::LocalTransform>(player);
    ctx->world->emplace<engine::scene::Name>(player, "Player");
    ctx->world->emplace<PlayerController>(player);
    ctx->world->emplace<Health>(player, 100.0f, 100.0f);

    engine::core::log(engine::core::LogLevel::Info, "{{PROJECT_NAME}} initialized!");

    return true;
}

void MyGame::register_systems(engine::plugin::SystemRegistry* reg) {
    using namespace engine::scene;

    // Register game systems
    // Systems run in order of phase, then by priority within each phase
    reg->add(Phase::Update, player_movement_system, "PlayerMovement", 0);
    reg->add(Phase::Update, health_system, "Health", 10);
}

void MyGame::pre_reload(engine::scene::World* /*world*/, void* /*state*/) {
    // Save any state that won't survive hot reload
    // The world state is automatically preserved by the engine
    // Use this for non-ECS state like score, game state machine, etc.

    engine::core::log(engine::core::LogLevel::Info, "Saving game state before reload...");
}

void MyGame::post_reload(engine::scene::World* /*world*/, const void* /*state*/) {
    // Restore state after hot reload
    // Note: pointers to components may have changed!

    engine::core::log(engine::core::LogLevel::Info, "Restoring game state after reload...");
}

void MyGame::shutdown() {
    engine::core::log(engine::core::LogLevel::Info, "{{PROJECT_NAME}} shutting down...");
    m_ctx = nullptr;
}
