#include <engine/gameplay/water_volume.hpp>
#include <engine/gameplay/character_movement.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/physics/physics_world.hpp>
#include <engine/physics/character_controller.hpp>
#include <engine/stats/stat_component.hpp>
#include <engine/reflect/type_registry.hpp>
#include <engine/core/log.hpp>
#include <algorithm>
#include <cmath>

namespace engine::gameplay {

// ============================================================================
// Water Query
// ============================================================================

WaterQueryResult query_water_at(scene::World& world, const Vec3& position) {
    WaterQueryResult result;

    auto view = world.view<WaterVolumeComponent, scene::WorldTransform>();

    for (auto entity : view) {
        auto& water = view.get<WaterVolumeComponent>(entity);
        auto& transform = view.get<scene::WorldTransform>(entity);

        Vec3 water_pos = transform.position();

        // Check if position is within water volume bounds
        bool in_bounds = false;

        if (water.use_collider_bounds) {
            // TODO: Use physics collider for precise bounds
            // For now, use half_extents as fallback
            Vec3 local_pos = position - water_pos;
            in_bounds = std::abs(local_pos.x) <= water.half_extents.x &&
                       local_pos.y <= water.water_height &&
                       local_pos.y >= water_pos.y - water.half_extents.y &&
                       std::abs(local_pos.z) <= water.half_extents.z;
        } else {
            // Simple box check
            Vec3 local_pos = position - water_pos;
            in_bounds = std::abs(local_pos.x) <= water.half_extents.x &&
                       local_pos.y <= water.water_height &&
                       local_pos.y >= water_pos.y - water.half_extents.y &&
                       std::abs(local_pos.z) <= water.half_extents.z;
        }

        if (in_bounds) {
            float actual_water_height = water_pos.y + water.water_height;

            result.in_water = true;
            result.water_height = actual_water_height;
            result.depth = actual_water_height - position.y;
            result.current = water.get_current_at(position);
            result.buoyancy = water.buoyancy;
            result.drag = water.drag;
            result.water_entity = entity;
            result.is_swimmable = water.is_swimmable;
            result.causes_damage = water.causes_damage;
            result.damage_per_second = water.damage_per_second;

            return result;  // Return first water volume found
        }
    }

    return result;  // Not in water
}

// ============================================================================
// Water Detection System
// ============================================================================

void water_detection_system(scene::World& world, double dt) {
    auto view = world.view<CharacterMovementComponent, scene::WorldTransform>();

    for (auto entity : view) {
        auto& movement = view.get<CharacterMovementComponent>(entity);
        auto& transform = view.get<scene::WorldTransform>(entity);

        Vec3 position = transform.position();

        // Query water at character position
        auto water_info = query_water_at(world, position);

        bool was_in_water = movement.is_in_water();
        bool is_in_water = water_info.in_water && water_info.is_swimmable;

        // Update water tracking
        scene::Entity previous_water = movement.current_water_volume;

        if (is_in_water) {
            movement.current_water_volume = water_info.water_entity;
            movement.water_surface_height = water_info.water_height;
            movement.water_depth = water_info.depth;

            // Dispatch enter water event
            if (!was_in_water) {
                core::events().dispatch(EnteredWaterEvent{
                    entity,
                    water_info.water_entity,
                    water_info.water_height
                });

                // Transition to appropriate water state
                if (water_info.depth > movement.water_settings.submerge_depth) {
                    movement.set_state(MovementState::SwimmingUnderwater);
                    core::events().dispatch(SubmergedEvent{entity, water_info.water_entity});
                } else {
                    movement.set_state(MovementState::Treading);
                }
            }

            // Check submerge/surface transitions
            bool was_underwater = movement.previous_state == MovementState::SwimmingUnderwater ||
                                  movement.previous_state == MovementState::Diving;
            bool is_underwater = water_info.depth > movement.water_settings.submerge_depth;

            if (is_underwater && !was_underwater && was_in_water) {
                movement.set_state(MovementState::Diving);
                core::events().dispatch(SubmergedEvent{entity, water_info.water_entity});
            } else if (!is_underwater && was_underwater) {
                movement.set_state(MovementState::Surfacing);
                core::events().dispatch(SurfacedEvent{entity, water_info.water_entity});
            }

        } else {
            movement.current_water_volume = scene::NullEntity;
            movement.water_surface_height = 0.0f;
            movement.water_depth = 0.0f;

            // Dispatch exit water event
            if (was_in_water) {
                core::events().dispatch(ExitedWaterEvent{entity, previous_water});

                // Transition to appropriate land state
                movement.set_state(MovementState::Falling);
            }
        }
    }
}

// ============================================================================
// Breath System
// ============================================================================

void breath_system(scene::World& world, double dt) {
    float delta = static_cast<float>(dt);

    auto view = world.view<CharacterMovementComponent>();

    for (auto entity : view) {
        auto& movement = view.get<CharacterMovementComponent>(entity);

        if (!movement.water_settings.breath_enabled) continue;

        if (movement.is_underwater()) {
            // Deplete breath while underwater
            float previous_breath = movement.current_breath;
            movement.current_breath -= delta;

            // Trigger drowning event when breath first reaches zero
            if (previous_breath > 0.0f && movement.current_breath <= 0.0f) {
                movement.current_breath = 0.0f;
                core::events().dispatch(StartedDrowningEvent{entity});
            }

            // Apply drowning damage
            if (movement.current_breath <= 0.0f) {
                movement.drowning_timer += delta;

                if (movement.drowning_timer >= movement.water_settings.drowning_damage_interval) {
                    movement.drowning_timer = 0.0f;

                    // Apply drowning damage through stats system
                    auto* stats = world.try_get<stats::StatsComponent>(entity);
                    if (stats) {
                        stats->modify_current(stats::StatType::Health,
                                              -movement.water_settings.drowning_damage_rate);

                        // Check for death by drowning
                        if (stats->is_depleted(stats::StatType::Health)) {
                            core::events().dispatch(DrownedEvent{entity});
                        }
                    }
                }

                // Auto-surface if enabled and drowning
                if (movement.water_settings.auto_surface && !movement.wants_dive) {
                    movement.wants_surface = true;
                }
            }

        } else if (movement.is_on_water_surface() || !movement.is_in_water()) {
            // Recover breath when at surface or on land
            float previous_breath = movement.current_breath;
            float max_breath = movement.water_settings.max_breath;

            movement.current_breath = std::min(
                max_breath,
                movement.current_breath + movement.water_settings.breath_recovery_rate * delta
            );
            movement.drowning_timer = 0.0f;

            // Trigger breath restored event when fully recovered
            if (previous_breath < max_breath && movement.current_breath >= max_breath) {
                core::events().dispatch(BreathRestoredEvent{entity, max_breath});
            }
        }
    }
}

// ============================================================================
// Water Movement
// ============================================================================

void apply_water_movement(
    scene::World& world,
    scene::Entity entity,
    const WaterQueryResult& water_info,
    double dt
) {
    float delta = static_cast<float>(dt);

    auto* movement = world.try_get<CharacterMovementComponent>(entity);
    auto* controller = world.try_get<physics::CharacterControllerComponent>(entity);

    if (!movement || !controller || !controller->controller) return;

    Vec3 input = movement->input_direction;
    float input_magnitude = glm::length(input);

    // Determine swim speed based on state and input
    float target_speed = 0.0f;
    bool is_sprinting = movement->wants_sprint;

    if (movement->is_underwater()) {
        target_speed = is_sprinting
            ? movement->water_settings.underwater_sprint_speed
            : movement->water_settings.underwater_speed;
    } else {
        target_speed = is_sprinting
            ? movement->water_settings.swim_sprint_speed
            : movement->water_settings.swim_speed;
    }

    if (input_magnitude < 0.1f) {
        target_speed = 0.0f;
    }

    // Handle vertical movement
    float vertical_input = 0.0f;

    // Dive input (crouch/dive button)
    if (movement->wants_dive && movement->can_dive()) {
        vertical_input = -1.0f;

        if (movement->is_on_water_surface()) {
            movement->set_state(MovementState::Diving);
        }
    }

    // Surface input (jump button)
    if (movement->wants_surface || movement->wants_jump) {
        vertical_input = 1.0f;

        if (movement->is_underwater()) {
            movement->set_state(MovementState::Surfacing);
        }
    }

    // Calculate movement direction
    Vec3 move_dir{0.0f};
    if (input_magnitude > 0.1f) {
        move_dir = glm::normalize(input);
    }

    // Add vertical component for underwater movement
    if (movement->is_underwater()) {
        move_dir.y = vertical_input * movement->water_settings.vertical_swim_speed / target_speed;
        if (glm::length(move_dir) > 0.01f) {
            move_dir = glm::normalize(move_dir);
        }
    }

    // Apply water current
    Vec3 current = water_info.current;

    // Calculate final velocity
    Vec3 velocity = move_dir * target_speed + current;

    // Apply drag
    float drag_factor = 1.0f / (1.0f + water_info.drag * delta);
    velocity *= drag_factor;

    // Apply buoyancy at surface
    if (movement->is_on_water_surface()) {
        float surface_offset = water_info.water_height - world.get<scene::WorldTransform>(entity).position().y;
        if (surface_offset > 0.0f && surface_offset < 1.0f) {
            // Keep character at water surface
            velocity.y += water_info.buoyancy * surface_offset * 5.0f;
        }
    }

    // Update movement state based on input
    if (movement->state == MovementState::Diving &&
        movement->state_time >= movement->water_settings.dive_transition_time) {
        movement->set_state(MovementState::SwimmingUnderwater);
    }

    if (movement->state == MovementState::Surfacing &&
        movement->state_time >= movement->water_settings.surface_transition_time) {
        // Check if actually at surface
        if (water_info.depth <= movement->water_settings.surface_detection_offset) {
            movement->set_state(MovementState::Swimming);
        }
    }

    // Update surface swimming vs treading
    if (movement->is_on_water_surface()) {
        if (input_magnitude > 0.1f) {
            if (movement->state != MovementState::Swimming) {
                movement->set_state(MovementState::Swimming);
            }
        } else if (movement->state == MovementState::Swimming) {
            movement->set_state(MovementState::Treading);
        }
    }

    // Apply to character controller
    controller->controller->set_movement_input(velocity);

    // Update current speed for animation
    movement->current_speed = glm::length(Vec3(velocity.x, 0.0f, velocity.z));

    // Clear one-shot inputs
    movement->wants_dive = false;
    movement->wants_surface = false;
}

// ============================================================================
// Component Registration
// ============================================================================

void register_water_components() {
    using namespace reflect;

    // Register WaterVolumeComponent
    TypeRegistry::instance().register_component<WaterVolumeComponent>(
        "WaterVolumeComponent",
        TypeMeta().set_display_name("Water Volume").set_category(TypeCategory::Component)
    );

    TypeRegistry::instance().register_property<WaterVolumeComponent, &WaterVolumeComponent::water_height>(
        "water_height",
        PropertyMeta().set_display_name("Water Height")
    );

    TypeRegistry::instance().register_property<WaterVolumeComponent, &WaterVolumeComponent::current_strength>(
        "current_strength",
        PropertyMeta().set_display_name("Current Strength")
    );

    TypeRegistry::instance().register_property<WaterVolumeComponent, &WaterVolumeComponent::buoyancy>(
        "buoyancy",
        PropertyMeta().set_display_name("Buoyancy").set_range(0.0f, 5.0f)
    );

    TypeRegistry::instance().register_property<WaterVolumeComponent, &WaterVolumeComponent::drag>(
        "drag",
        PropertyMeta().set_display_name("Drag").set_range(0.0f, 10.0f)
    );

    TypeRegistry::instance().register_property<WaterVolumeComponent, &WaterVolumeComponent::is_swimmable>(
        "is_swimmable",
        PropertyMeta().set_display_name("Is Swimmable")
    );

    TypeRegistry::instance().register_property<WaterVolumeComponent, &WaterVolumeComponent::causes_damage>(
        "causes_damage",
        PropertyMeta().set_display_name("Causes Damage")
    );

    TypeRegistry::instance().register_property<WaterVolumeComponent, &WaterVolumeComponent::damage_per_second>(
        "damage_per_second",
        PropertyMeta().set_display_name("Damage Per Second").set_range(0.0f, 1000.0f)
    );
}

} // namespace engine::gameplay
