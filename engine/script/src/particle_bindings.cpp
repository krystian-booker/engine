#include <engine/script/bindings.hpp>
#include <engine/script/script_context.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/render_components.hpp>
#include <engine/core/log.hpp>

namespace engine::script {

void register_particle_bindings(sol::state& lua) {
    using namespace engine::scene;
    using namespace engine::core;

    // Create Particles table
    auto particles = lua.create_named_table("Particles");

    // Play particle emitter (enable emission)
    particles.set_function("play", [](uint32_t entity_id) {
        auto* world = get_current_script_world();
        if (!world) {
            core::log(core::LogLevel::Warn, "Particles.play called without world context");
            return;
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* emitter = world->try_get<ParticleEmitter>(entity);
        if (emitter) {
            emitter->enabled = true;
        }
    });

    // Stop particle emitter (disable emission)
    particles.set_function("stop", [](uint32_t entity_id) {
        auto* world = get_current_script_world();
        if (!world) {
            core::log(core::LogLevel::Warn, "Particles.stop called without world context");
            return;
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* emitter = world->try_get<ParticleEmitter>(entity);
        if (emitter) {
            emitter->enabled = false;
        }
    });

    // Check if particle emitter is playing
    particles.set_function("is_playing", [](uint32_t entity_id) -> bool {
        auto* world = get_current_script_world();
        if (!world) return false;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return false;

        auto* emitter = world->try_get<ParticleEmitter>(entity);
        return emitter && emitter->enabled;
    });

    // Check if entity has particle emitter
    particles.set_function("has", [](uint32_t entity_id) -> bool {
        auto* world = get_current_script_world();
        if (!world) return false;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return false;

        return world->has<ParticleEmitter>(entity);
    });

    // Set emission rate
    particles.set_function("set_emission_rate", [](uint32_t entity_id, float rate) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* emitter = world->try_get<ParticleEmitter>(entity);
        if (emitter) {
            emitter->emission_rate = rate;
        }
    });

    // Get emission rate
    particles.set_function("get_emission_rate", [](uint32_t entity_id) -> float {
        auto* world = get_current_script_world();
        if (!world) return 0.0f;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return 0.0f;

        auto* emitter = world->try_get<ParticleEmitter>(entity);
        return emitter ? emitter->emission_rate : 0.0f;
    });

    // Set particle lifetime
    particles.set_function("set_lifetime", [](uint32_t entity_id, float lifetime) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* emitter = world->try_get<ParticleEmitter>(entity);
        if (emitter) {
            emitter->lifetime = lifetime;
        }
    });

    // Set particle start color
    particles.set_function("set_start_color", [](uint32_t entity_id, const Vec4& color) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* emitter = world->try_get<ParticleEmitter>(entity);
        if (emitter) {
            emitter->start_color = color;
        }
    });

    // Set particle end color
    particles.set_function("set_end_color", [](uint32_t entity_id, const Vec4& color) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* emitter = world->try_get<ParticleEmitter>(entity);
        if (emitter) {
            emitter->end_color = color;
        }
    });

    // Set particle start size
    particles.set_function("set_start_size", [](uint32_t entity_id, float size) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* emitter = world->try_get<ParticleEmitter>(entity);
        if (emitter) {
            emitter->start_size = size;
        }
    });

    // Set particle end size
    particles.set_function("set_end_size", [](uint32_t entity_id, float size) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* emitter = world->try_get<ParticleEmitter>(entity);
        if (emitter) {
            emitter->end_size = size;
        }
    });

    // Set initial speed
    particles.set_function("set_initial_speed", [](uint32_t entity_id, float speed) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* emitter = world->try_get<ParticleEmitter>(entity);
        if (emitter) {
            emitter->initial_speed = speed;
        }
    });

    // Set gravity
    particles.set_function("set_gravity", [](uint32_t entity_id, const Vec3& gravity) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* emitter = world->try_get<ParticleEmitter>(entity);
        if (emitter) {
            emitter->gravity = gravity;
        }
    });

    // Set max particles
    particles.set_function("set_max_particles", [](uint32_t entity_id, uint32_t max) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* emitter = world->try_get<ParticleEmitter>(entity);
        if (emitter) {
            emitter->max_particles = max;
        }
    });
}

} // namespace engine::script
