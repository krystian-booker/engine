#include <engine/script/bindings.hpp>
#include <engine/script/script_context.hpp>
#include <engine/render/animation_state_machine.hpp>
#include <engine/scene/world.hpp>
#include <engine/core/log.hpp>
#include <unordered_map>

namespace engine::script {

// Storage for Lua animation event callbacks
static std::unordered_map<uint32_t, sol::function> s_animation_event_callbacks;

void register_animation_bindings(sol::state& lua) {
    using namespace engine::render;
    using namespace engine::core;

    // Create Animator table
    auto anim = lua.create_named_table("Animator");

    // --- State Machine Control ---

    // Play animation state
    anim.set_function("play", [](uint32_t entity_id, const std::string& state_name) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        if (comp && comp->state_machine) {
            comp->state_machine->set_state(state_name);
            comp->state_machine->start();
        }
    });

    // Stop animation
    anim.set_function("stop", [](uint32_t entity_id) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        if (comp && comp->state_machine) {
            comp->state_machine->stop();
        }
    });

    // Start state machine (from default state)
    anim.set_function("start", [](uint32_t entity_id) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        if (comp && comp->state_machine) {
            comp->state_machine->start();
        }
    });

    // Reset to default state
    anim.set_function("reset", [](uint32_t entity_id) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        if (comp && comp->state_machine) {
            comp->state_machine->reset();
        }
    });

    // Check if running
    anim.set_function("is_playing", [](uint32_t entity_id) -> bool {
        auto* world = get_current_script_world();
        if (!world) return false;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return false;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        return comp && comp->state_machine && comp->state_machine->is_running();
    });

    // Check if entity has animator
    anim.set_function("has", [](uint32_t entity_id) -> bool {
        auto* world = get_current_script_world();
        if (!world) return false;

        auto entity = static_cast<entt::entity>(entity_id);
        return world->registry().valid(entity) && world->has<AnimatorComponent>(entity);
    });

    // --- Parameters ---

    anim.set_function("set_float", [](uint32_t entity_id, const std::string& name, float value) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        if (comp && comp->state_machine) {
            comp->state_machine->set_float(name, value);
        }
    });

    anim.set_function("get_float", [](uint32_t entity_id, const std::string& name) -> float {
        auto* world = get_current_script_world();
        if (!world) return 0.0f;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return 0.0f;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        return (comp && comp->state_machine) ? comp->state_machine->get_float(name) : 0.0f;
    });

    anim.set_function("set_bool", [](uint32_t entity_id, const std::string& name, bool value) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        if (comp && comp->state_machine) {
            comp->state_machine->set_bool(name, value);
        }
    });

    anim.set_function("get_bool", [](uint32_t entity_id, const std::string& name) -> bool {
        auto* world = get_current_script_world();
        if (!world) return false;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return false;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        return (comp && comp->state_machine) ? comp->state_machine->get_bool(name) : false;
    });

    anim.set_function("set_int", [](uint32_t entity_id, const std::string& name, int value) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        if (comp && comp->state_machine) {
            comp->state_machine->set_int(name, value);
        }
    });

    anim.set_function("get_int", [](uint32_t entity_id, const std::string& name) -> int {
        auto* world = get_current_script_world();
        if (!world) return 0;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return 0;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        return (comp && comp->state_machine) ? comp->state_machine->get_int(name) : 0;
    });

    anim.set_function("set_trigger", [](uint32_t entity_id, const std::string& name) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        if (comp && comp->state_machine) {
            comp->state_machine->set_trigger(name);
        }
    });

    anim.set_function("reset_trigger", [](uint32_t entity_id, const std::string& name) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        if (comp && comp->state_machine) {
            comp->state_machine->reset_trigger(name);
        }
    });

    // Check if parameter exists
    anim.set_function("has_parameter", [](uint32_t entity_id, const std::string& name) -> bool {
        auto* world = get_current_script_world();
        if (!world) return false;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return false;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        return (comp && comp->state_machine) ? comp->state_machine->has_parameter(name) : false;
    });

    // --- State Queries ---

    anim.set_function("get_current_state", [](uint32_t entity_id) -> std::string {
        auto* world = get_current_script_world();
        if (!world) return "";

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return "";

        auto* comp = world->try_get<AnimatorComponent>(entity);
        return (comp && comp->state_machine) ? comp->state_machine->get_current_state_name() : "";
    });

    anim.set_function("is_in_transition", [](uint32_t entity_id) -> bool {
        auto* world = get_current_script_world();
        if (!world) return false;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return false;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        return (comp && comp->state_machine) ? comp->state_machine->is_in_transition() : false;
    });

    anim.set_function("get_transition_progress", [](uint32_t entity_id) -> float {
        auto* world = get_current_script_world();
        if (!world) return 0.0f;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return 0.0f;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        return (comp && comp->state_machine) ? comp->state_machine->get_transition_progress() : 0.0f;
    });

    anim.set_function("get_normalized_time", [](uint32_t entity_id) -> float {
        auto* world = get_current_script_world();
        if (!world) return 0.0f;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return 0.0f;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        return (comp && comp->state_machine) ? comp->state_machine->get_current_normalized_time() : 0.0f;
    });

    // --- Root Motion ---

    anim.set_function("set_apply_root_motion", [](uint32_t entity_id, bool apply) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        if (comp) {
            comp->apply_root_motion = apply;
        }
    });

    anim.set_function("get_apply_root_motion", [](uint32_t entity_id) -> bool {
        auto* world = get_current_script_world();
        if (!world) return false;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return false;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        return comp ? comp->apply_root_motion : false;
    });

    anim.set_function("get_root_motion_delta", [](uint32_t entity_id) -> std::tuple<Vec3, Quat> {
        auto* world = get_current_script_world();
        if (!world) return {Vec3{0.0f}, Quat{1.0f, 0.0f, 0.0f, 0.0f}};

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return {Vec3{0.0f}, Quat{1.0f, 0.0f, 0.0f, 0.0f}};

        auto* comp = world->try_get<AnimatorComponent>(entity);
        if (comp && comp->state_machine) {
            const auto& rm = comp->state_machine->get_root_motion();
            return {rm.translation_delta, rm.rotation_delta};
        }
        return {Vec3{0.0f}, Quat{1.0f, 0.0f, 0.0f, 0.0f}};
    });

    // --- Layer Control ---

    anim.set_function("set_layer_weight", [](uint32_t entity_id, const std::string& layer, float weight) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        if (comp && comp->state_machine) {
            comp->state_machine->set_layer_weight(layer, weight);
        }
    });

    // --- Event Callback ---

    anim.set_function("on_event", [](uint32_t entity_id, sol::function callback) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        if (comp && comp->state_machine) {
            // Store the callback
            s_animation_event_callbacks[entity_id] = callback;

            // Set up the C++ callback to call the Lua function
            comp->state_machine->set_event_callback(
                [entity_id](const std::string& state, const std::string& event) {
                    auto it = s_animation_event_callbacks.find(entity_id);
                    if (it != s_animation_event_callbacks.end() && it->second.valid()) {
                        try {
                            it->second(state, event);
                        } catch (const sol::error& e) {
                            core::log(core::LogLevel::Error, "Lua animation event error: {}", e.what());
                        }
                    }
                }
            );
        }
    });

    // Clear event callback
    anim.set_function("clear_event_callback", [](uint32_t entity_id) {
        s_animation_event_callbacks.erase(entity_id);

        auto* world = get_current_script_world();
        if (!world) return;

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) return;

        auto* comp = world->try_get<AnimatorComponent>(entity);
        if (comp && comp->state_machine) {
            comp->state_machine->set_event_callback(nullptr);
        }
    });
}

} // namespace engine::script
