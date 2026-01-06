#include <engine/ai/perception.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/core/event_dispatcher.hpp>
#include <engine/core/log.hpp>
#include <engine/reflect/type_registry.hpp>
#include <cmath>
#include <algorithm>

namespace engine::ai {

namespace {

Vec3 get_entity_position(scene::World& world, scene::Entity entity) {
    auto* world_transform = world.try_get<scene::WorldTransform>(entity);
    if (world_transform) {
        return world_transform->get_position();
    }
    auto* local_transform = world.try_get<scene::LocalTransform>(entity);
    if (local_transform) {
        return local_transform->position;
    }
    return Vec3(0.0f);
}

Vec3 get_entity_forward(scene::World& world, scene::Entity entity) {
    auto* world_transform = world.try_get<scene::WorldTransform>(entity);
    if (world_transform) {
        return -Vec3(world_transform->matrix[2][0],
                     world_transform->matrix[2][1],
                     world_transform->matrix[2][2]);
    }
    auto* local_transform = world.try_get<scene::LocalTransform>(entity);
    if (local_transform) {
        return local_transform->forward();
    }
    return Vec3(0.0f, 0.0f, -1.0f);
}

bool is_faction_hostile(const AIPerceptionComponent& perception, const std::string& target_faction) {
    for (const auto& hostile : perception.hostile_factions) {
        if (hostile == target_faction) {
            return true;
        }
    }
    return false;
}

} // anonymous namespace

// ============================================================================
// Perception System Implementation
// ============================================================================

PerceptionSystem::PerceptionSystem() {
    m_los_check = [this](scene::World& world, const Vec3& from, const Vec3& to,
                         uint32_t layer_mask, scene::Entity exclude) {
        return default_los_check(world, from, to, layer_mask, exclude);
    };
}

PerceptionSystem& PerceptionSystem::instance() {
    static PerceptionSystem s_instance;
    return s_instance;
}

void PerceptionSystem::update(scene::World& world, float dt) {
    auto view = world.view<AIPerceptionComponent>();

    for (auto entity : view) {
        auto& perception = view.get<AIPerceptionComponent>(entity);

        if (!perception.enabled) continue;

        check_perception(world, entity, dt);
    }
}

void PerceptionSystem::check_perception(scene::World& world, scene::Entity entity, float dt) {
    auto* perception = world.try_get<AIPerceptionComponent>(entity);
    if (!perception || !perception->enabled) return;

    Vec3 position = get_entity_position(world, entity);
    Vec3 forward = get_entity_forward(world, entity);

    // Update sight
    if (perception->sight_enabled) {
        update_sight(world, entity, *perception, position, forward, dt);
    }

    // Update hearing
    if (perception->hearing_enabled) {
        update_hearing(world, entity, *perception, position, dt);
    }

    // Update awareness levels and cleanup old perceptions
    update_awareness(*perception, dt);
    cleanup_perceptions(*perception, dt);
}

void PerceptionSystem::emit_noise(scene::World& world, const Vec3& position, float radius,
                                   float loudness, const std::string& type, scene::Entity source) {
    // Find all AI entities within range
    auto view = world.view<AIPerceptionComponent>();

    for (auto entity : view) {
        if (entity == source) continue;

        auto& perception = view.get<AIPerceptionComponent>(entity);
        if (!perception.enabled || !perception.hearing_enabled) continue;

        Vec3 listener_pos = get_entity_position(world, entity);
        float distance = glm::length(position - listener_pos);

        if (distance > radius * perception.hearing_range / 10.0f) continue;

        // Calculate effective loudness
        float effective_loudness = loudness * (1.0f - distance / (radius * m_hearing_multiplier));
        if (effective_loudness <= 0.0f) continue;

        // Find or create perception entry
        PerceivedEntity* pe = nullptr;
        if (source != scene::NullEntity) {
            for (auto& existing : perception.perceived_entities) {
                if (existing.entity == source) {
                    pe = &existing;
                    break;
                }
            }
        }

        if (!pe && source != scene::NullEntity) {
            PerceivedEntity new_pe;
            new_pe.entity = source;
            new_pe.sense = PerceptionSense::Hearing;
            new_pe.is_hostile = true; // Assume hostile for noise
            perception.perceived_entities.push_back(new_pe);
            pe = &perception.perceived_entities.back();
        }

        if (pe) {
            pe->last_known_position = position;
            pe->stimulation = std::max(pe->stimulation, effective_loudness);
            pe->time_since_sensed = 0.0f;
            pe->currently_perceived = true;
        }

        // Emit event
        NoiseHeardEvent event;
        event.listener = entity;
        event.noise_position = position;
        event.loudness = effective_loudness;
        event.noise_type = type;
        event.noise_source = source;
        core::EventDispatcher::instance().dispatch(event);
    }
}

void PerceptionSystem::alert_to_target(scene::World& world, scene::Entity ai, scene::Entity target) {
    auto* perception = world.try_get<AIPerceptionComponent>(ai);
    if (!perception) return;

    Vec3 target_pos = get_entity_position(world, target);

    // Find or create perception entry
    PerceivedEntity* pe = nullptr;
    for (auto& existing : perception->perceived_entities) {
        if (existing.entity == target) {
            pe = &existing;
            break;
        }
    }

    if (!pe) {
        PerceivedEntity new_pe;
        new_pe.entity = target;
        new_pe.sense = PerceptionSense::Damage; // Alerted, not seen
        perception->perceived_entities.push_back(new_pe);
        pe = &perception->perceived_entities.back();
    }

    // Instant full awareness
    pe->awareness = 1.0f;
    pe->stimulation = 1.0f;
    pe->last_known_position = target_pos;
    pe->currently_perceived = true;
    pe->is_hostile = true;
    pe->time_since_sensed = 0.0f;

    // Emit alert event
    AIAlertedEvent event;
    event.entity = ai;
    event.cause = target;
    event.alert_position = target_pos;
    core::EventDispatcher::instance().dispatch(event);
}

bool PerceptionSystem::can_see(scene::World& world, scene::Entity observer, scene::Entity target) {
    auto* perception = world.try_get<AIPerceptionComponent>(observer);
    if (!perception || !perception->sight_enabled) return false;

    Vec3 observer_pos = get_entity_position(world, observer);
    Vec3 observer_forward = get_entity_forward(world, observer);
    Vec3 target_pos = get_entity_position(world, target);

    // Check range
    float distance = glm::length(target_pos - observer_pos);
    if (distance > perception->sight_range * m_sight_multiplier) return false;

    // Check FOV
    if (!is_in_fov(observer_pos, observer_forward, target_pos,
                   perception->sight_angle, perception->sight_range)) {
        return false;
    }

    // Check line of sight
    if (perception->requires_line_of_sight) {
        return m_los_check(world, observer_pos, target_pos,
                           perception->sight_layer_mask, observer);
    }

    return true;
}

float PerceptionSystem::get_distance(scene::World& world, scene::Entity from, scene::Entity to) {
    Vec3 from_pos = get_entity_position(world, from);
    Vec3 to_pos = get_entity_position(world, to);
    return glm::length(to_pos - from_pos);
}

void PerceptionSystem::set_los_check(PerceptionLOSCheck check) {
    m_los_check = std::move(check);
}

void PerceptionSystem::update_sight(scene::World& world, scene::Entity entity,
                                     AIPerceptionComponent& perception, const Vec3& position,
                                     const Vec3& forward, float dt) {
    // Check all potential targets (entities with transform)
    auto view = world.view<scene::LocalTransform>();

    for (auto target : view) {
        if (target == entity) continue;

        Vec3 target_pos = get_entity_position(world, target);
        float distance = glm::length(target_pos - position);

        // Check range
        if (distance > perception.sight_range * m_sight_multiplier) continue;

        // Check FOV
        float stimulation = 1.0f;
        bool in_main_fov = is_in_fov(position, forward, target_pos,
                                      perception.sight_angle, perception.sight_range);
        bool in_peripheral = false;

        if (!in_main_fov && perception.peripheral_enabled) {
            in_peripheral = is_in_fov(position, forward, target_pos,
                                       perception.peripheral_angle, perception.sight_range);
            if (in_peripheral) {
                stimulation = perception.peripheral_stimulation;
            }
        }

        if (!in_main_fov && !in_peripheral) continue;

        // Check line of sight
        bool can_see = true;
        if (perception.requires_line_of_sight) {
            can_see = m_los_check(world, position, target_pos,
                                  perception.sight_layer_mask, entity);
        }

        if (!can_see) continue;

        // Find or create perception entry
        PerceivedEntity* pe = nullptr;
        for (auto& existing : perception.perceived_entities) {
            if (existing.entity == target) {
                pe = &existing;
                break;
            }
        }

        if (!pe) {
            PerceivedEntity new_pe;
            new_pe.entity = target;
            new_pe.time_first_sensed = 0.0f;
            perception.perceived_entities.push_back(new_pe);
            pe = &perception.perceived_entities.back();

            // Emit gained event
            PerceptionGainedEvent event;
            event.perceiver = entity;
            event.perceived = target;
            event.sense = PerceptionSense::Sight;
            core::EventDispatcher::instance().dispatch(event);
        }

        // Update perception
        pe->sense = PerceptionSense::Sight;
        pe->currently_perceived = true;
        pe->stimulation = stimulation;
        pe->last_known_position = target_pos;
        pe->time_since_sensed = 0.0f;

        // Instant awareness at close range
        if (distance <= perception.instant_awareness_distance) {
            pe->awareness = 1.0f;
        }

        // Determine hostility (simplified - real implementation would check factions)
        pe->is_hostile = true; // For demo purposes
    }
}

void PerceptionSystem::update_hearing(scene::World& world, scene::Entity entity,
                                       AIPerceptionComponent& perception, const Vec3& position, float dt) {
    // Process noise emitters
    auto view = world.view<AINoiseEmitterComponent>();

    for (auto noise_entity : view) {
        if (noise_entity == entity) continue;

        auto& emitter = view.get<AINoiseEmitterComponent>(noise_entity);
        if (!emitter.enabled) continue;

        // Only process triggered or continuous noises
        if (!emitter.is_continuous && !emitter.trigger_noise) continue;

        Vec3 noise_pos = emitter.noise_position.value_or(get_entity_position(world, noise_entity));
        float distance = glm::length(noise_pos - position);

        if (distance > emitter.noise_radius * m_hearing_multiplier) continue;

        // Calculate loudness
        float loudness = emitter.loudness * (1.0f - distance / (emitter.noise_radius * m_hearing_multiplier));
        if (loudness <= 0.0f) continue;

        // Handle as noise
        emit_noise(world, noise_pos, emitter.noise_radius, loudness, emitter.noise_type, noise_entity);

        // Clear one-shot trigger
        if (!emitter.is_continuous) {
            emitter.trigger_noise = false;
        }
    }
}

void PerceptionSystem::update_awareness(AIPerceptionComponent& perception, float dt) {
    for (auto& pe : perception.perceived_entities) {
        float old_awareness = pe.awareness;

        if (pe.currently_perceived) {
            // Increase awareness
            pe.awareness += perception.awareness_gain_rate * pe.stimulation * dt;
            pe.awareness = std::min(pe.awareness, 1.0f);
        } else {
            // Decay awareness
            pe.awareness -= perception.awareness_decay_rate * dt;
            pe.awareness = std::max(pe.awareness, 0.0f);
        }

        // Check if crossed threshold
        bool was_alert = old_awareness >= perception.awareness_threshold;
        bool is_alert = pe.awareness >= perception.awareness_threshold;

        if (!was_alert && is_alert) {
            AwarenessChangedEvent event;
            event.perceiver = scene::NullEntity; // Would need entity reference
            event.perceived = pe.entity;
            event.old_awareness = old_awareness;
            event.new_awareness = pe.awareness;
            event.became_alert = true;
            core::EventDispatcher::instance().dispatch(event);
        }

        // Clear current perception flag for next frame
        pe.currently_perceived = false;
    }
}

void PerceptionSystem::cleanup_perceptions(AIPerceptionComponent& perception, float dt) {
    // Update time since sensed and remove old perceptions
    perception.perceived_entities.erase(
        std::remove_if(perception.perceived_entities.begin(), perception.perceived_entities.end(),
            [&perception, dt](PerceivedEntity& pe) {
                pe.time_since_sensed += dt;

                // Remove if forgotten
                return pe.awareness <= 0.0f &&
                       pe.time_since_sensed >= perception.memory_duration;
            }),
        perception.perceived_entities.end()
    );
}

bool PerceptionSystem::default_los_check(scene::World& world, const Vec3& from, const Vec3& to,
                                          uint32_t layer_mask, scene::Entity exclude) {
    // Default: no occlusion (would use physics raycast in real implementation)
    return true;
}

bool PerceptionSystem::is_in_fov(const Vec3& observer_pos, const Vec3& forward,
                                  const Vec3& target_pos, float angle, float range) {
    Vec3 to_target = target_pos - observer_pos;
    float distance = glm::length(to_target);

    if (distance > range || distance < 0.001f) return false;

    to_target = glm::normalize(to_target);
    float dot = glm::dot(forward, to_target);
    float view_angle = glm::degrees(std::acos(glm::clamp(dot, -1.0f, 1.0f)));

    return view_angle <= angle * 0.5f;
}

// ============================================================================
// ECS Systems
// ============================================================================

void perception_system(scene::World& world, double dt) {
    PerceptionSystem::instance().update(world, static_cast<float>(dt));
}

void noise_emitter_system(scene::World& world, double dt) {
    // Handled within perception_system for efficiency
}

// ============================================================================
// Component Registration
// ============================================================================

void register_perception_components() {
    using namespace reflect;

    // AIPerceptionComponent
    TypeRegistry::instance().register_component<AIPerceptionComponent>("AIPerceptionComponent")
        .display_name("AI Perception")
        .category("AI");

    TypeRegistry::instance().register_property<AIPerceptionComponent>("enabled",
        [](const AIPerceptionComponent& c) { return c.enabled; },
        [](AIPerceptionComponent& c, bool v) { c.enabled = v; })
        .display_name("Enabled");

    TypeRegistry::instance().register_property<AIPerceptionComponent>("sight_range",
        [](const AIPerceptionComponent& c) { return c.sight_range; },
        [](AIPerceptionComponent& c, float v) { c.sight_range = v; })
        .display_name("Sight Range").min(1.0f);

    TypeRegistry::instance().register_property<AIPerceptionComponent>("sight_angle",
        [](const AIPerceptionComponent& c) { return c.sight_angle; },
        [](AIPerceptionComponent& c, float v) { c.sight_angle = v; })
        .display_name("Sight Angle").min(10.0f).max(360.0f);

    TypeRegistry::instance().register_property<AIPerceptionComponent>("hearing_range",
        [](const AIPerceptionComponent& c) { return c.hearing_range; },
        [](AIPerceptionComponent& c, float v) { c.hearing_range = v; })
        .display_name("Hearing Range").min(0.0f);

    // AINoiseEmitterComponent
    TypeRegistry::instance().register_component<AINoiseEmitterComponent>("AINoiseEmitterComponent")
        .display_name("AI Noise Emitter")
        .category("AI");

    TypeRegistry::instance().register_property<AINoiseEmitterComponent>("noise_radius",
        [](const AINoiseEmitterComponent& c) { return c.noise_radius; },
        [](AINoiseEmitterComponent& c, float v) { c.noise_radius = v; })
        .display_name("Noise Radius").min(0.0f);

    TypeRegistry::instance().register_property<AINoiseEmitterComponent>("loudness",
        [](const AINoiseEmitterComponent& c) { return c.loudness; },
        [](AINoiseEmitterComponent& c, float v) { c.loudness = v; })
        .display_name("Loudness").min(0.0f).max(2.0f);

    core::log(core::LogLevel::Info, "Perception components registered");
}

} // namespace engine::ai
