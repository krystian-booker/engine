#include <engine/combat/hit_reaction.hpp>
#include <engine/render/animation_state_machine.hpp>
#include <engine/stats/stat_component.hpp>
#include <engine/core/log.hpp>
#include <engine/core/game_events.hpp>

namespace engine::combat {

using namespace engine::core;

// ============================================================================
// HitReactionSystem Singleton
// ============================================================================

HitReactionSystem& HitReactionSystem::instance() {
    static HitReactionSystem s_instance;
    return s_instance;
}

// ============================================================================
// Hit Processing
// ============================================================================

HitReactionType HitReactionSystem::process_hit(scene::World& world, const DamageInfo& damage) {
    if (damage.target == scene::NullEntity) {
        return HitReactionType::None;
    }

    // Check if target has HitReactionComponent
    auto* comp = world.try_get<HitReactionComponent>(damage.target);
    if (!comp) {
        return HitReactionType::None;
    }

    // Check cooldown
    if (comp->cooldown_remaining > 0.0f) {
        return HitReactionType::None;
    }

    // Calculate damage as percentage of max health
    float damage_percent = 0.0f;
    auto* stats = world.try_get<stats::StatsComponent>(damage.target);
    if (stats) {
        float max_health = stats->get(stats::StatType::Health);
        if (max_health > 0.0f) {
            damage_percent = damage.final_damage / max_health;
        }
    } else {
        // Default to medium reaction if no stats
        damage_percent = 0.1f;
    }

    // Check if stagger was caused by poise break
    HitReactionType reaction_type;
    if (damage.caused_stagger) {
        reaction_type = HitReactionType::Stagger;
    } else {
        reaction_type = determine_type(damage_percent, comp->config, comp->super_armor_stacks);
    }

    if (reaction_type == HitReactionType::None) {
        return HitReactionType::None;
    }

    // Calculate hit direction
    Vec3 direction = damage.knockback;
    if (glm::length(direction) < 0.001f) {
        direction = damage.hit_normal * -1.0f;  // Use inverse hit normal
    }
    if (glm::length(direction) > 0.001f) {
        direction = glm::normalize(direction);
    }

    // Start the reaction
    start_reaction(world, damage.target, reaction_type, *comp, direction);

    // Broadcast event
    HitReactionEvent event;
    event.entity = damage.target;
    event.type = reaction_type;
    event.hit_direction = direction;
    event.damage_percent = damage_percent;
    game_events().broadcast(event);

    return reaction_type;
}

HitReactionType HitReactionSystem::determine_type(float damage_percent,
                                                    const HitReactionConfig& config,
                                                    int super_armor_stacks) {
    HitReactionType base_type;

    if (damage_percent >= config.heavy_threshold) {
        base_type = HitReactionType::Stagger;
    } else if (damage_percent >= config.medium_threshold) {
        base_type = HitReactionType::Heavy;
    } else if (damage_percent >= config.light_threshold) {
        base_type = HitReactionType::Medium;
    } else {
        base_type = HitReactionType::Light;
    }

    // Super armor reduces reaction severity
    if (super_armor_stacks > 0) {
        int reduction = super_armor_stacks;
        while (reduction > 0 && base_type != HitReactionType::None) {
            switch (base_type) {
                case HitReactionType::Stagger:
                    base_type = HitReactionType::Heavy;
                    break;
                case HitReactionType::Heavy:
                    base_type = HitReactionType::Medium;
                    break;
                case HitReactionType::Medium:
                    base_type = HitReactionType::Light;
                    break;
                case HitReactionType::Light:
                    base_type = HitReactionType::None;
                    break;
                default:
                    break;
            }
            reduction--;
        }
    }

    return base_type;
}

float HitReactionSystem::get_reaction_duration(HitReactionType type, const HitReactionConfig& config) const {
    switch (type) {
        case HitReactionType::Light:   return config.light_duration;
        case HitReactionType::Medium:  return config.medium_duration;
        case HitReactionType::Heavy:   return config.heavy_duration;
        case HitReactionType::Stagger: return config.stagger_duration;
        default:                       return 0.0f;
    }
}

void HitReactionSystem::start_reaction(scene::World& world, scene::Entity entity,
                                         HitReactionType type, HitReactionComponent& comp,
                                         const Vec3& direction) {
    // Set component state
    comp.is_reacting = true;
    comp.current_reaction = type;
    comp.reaction_timer = get_reaction_duration(type, comp.config);
    comp.cooldown_remaining = comp.config.cooldown;
    comp.hit_direction = direction;

    // Trigger animation
    auto* animator = world.try_get<render::AnimatorComponent>(entity);
    if (animator && animator->state_machine) {
        std::string anim_name;
        switch (type) {
            case HitReactionType::Light:   anim_name = comp.config.light_hit_anim; break;
            case HitReactionType::Medium:  anim_name = comp.config.medium_hit_anim; break;
            case HitReactionType::Heavy:   anim_name = comp.config.heavy_hit_anim; break;
            case HitReactionType::Stagger: anim_name = comp.config.stagger_anim; break;
            default: break;
        }

        if (!anim_name.empty()) {
            // Set trigger parameter to play the reaction animation
            // The animation state machine should have a trigger for hit reactions
            animator->state_machine->set_trigger("hit_reaction");
            animator->state_machine->set_int("hit_type", static_cast<int>(type));

            log(LogLevel::Debug, "[HitReaction] Entity {} playing {} ({})",
                static_cast<uint32_t>(entity), anim_name, get_reaction_type_name(type));
        }
    }
}

void HitReactionSystem::end_reaction(scene::World& /*world*/, scene::Entity entity,
                                       HitReactionComponent& comp) {
    comp.is_reacting = false;
    comp.current_reaction = HitReactionType::None;
    comp.reaction_timer = 0.0f;

    log(LogLevel::Debug, "[HitReaction] Entity {} reaction ended", static_cast<uint32_t>(entity));
}

// ============================================================================
// Update
// ============================================================================

void HitReactionSystem::update(scene::World& world, float dt) {
    auto view = world.view<HitReactionComponent>();

    for (auto entity : view) {
        auto& comp = view.get<HitReactionComponent>(entity);

        // Update cooldown
        if (comp.cooldown_remaining > 0.0f) {
            comp.cooldown_remaining -= dt;
        }

        // Update reaction timer
        if (comp.is_reacting) {
            comp.reaction_timer -= dt;
            if (comp.reaction_timer <= 0.0f) {
                end_reaction(world, entity, comp);
            }
        }
    }
}

// ============================================================================
// Query Methods
// ============================================================================

bool HitReactionSystem::is_reacting(scene::World& world, scene::Entity entity) const {
    auto* comp = world.try_get<HitReactionComponent>(entity);
    return comp && comp->is_reacting;
}

HitReactionType HitReactionSystem::get_current_reaction(scene::World& world, scene::Entity entity) const {
    auto* comp = world.try_get<HitReactionComponent>(entity);
    return comp ? comp->current_reaction : HitReactionType::None;
}

float HitReactionSystem::get_reaction_progress(scene::World& world, scene::Entity entity) const {
    auto* comp = world.try_get<HitReactionComponent>(entity);
    if (!comp || !comp->is_reacting) {
        return 0.0f;
    }

    float total_duration = get_reaction_duration(comp->current_reaction, comp->config);
    if (total_duration <= 0.0f) {
        return 0.0f;
    }

    return 1.0f - (comp->reaction_timer / total_duration);
}

// ============================================================================
// Control Methods
// ============================================================================

void HitReactionSystem::cancel_reaction(scene::World& world, scene::Entity entity) {
    auto* comp = world.try_get<HitReactionComponent>(entity);
    if (comp && comp->is_reacting) {
        end_reaction(world, entity, *comp);
    }
}

void HitReactionSystem::force_reaction(scene::World& world, scene::Entity entity,
                                         HitReactionType type, const Vec3& direction) {
    auto* comp = world.try_get<HitReactionComponent>(entity);
    if (!comp) {
        return;
    }

    // Cancel any current reaction
    if (comp->is_reacting) {
        end_reaction(world, entity, *comp);
    }

    // Force the new reaction regardless of cooldown
    comp->cooldown_remaining = 0.0f;
    start_reaction(world, entity, type, *comp, direction);
}

// ============================================================================
// Super Armor
// ============================================================================

void HitReactionSystem::add_super_armor(scene::World& world, scene::Entity entity, int stacks) {
    auto* comp = world.try_get<HitReactionComponent>(entity);
    if (comp) {
        comp->super_armor_stacks += stacks;
    }
}

void HitReactionSystem::remove_super_armor(scene::World& world, scene::Entity entity, int stacks) {
    auto* comp = world.try_get<HitReactionComponent>(entity);
    if (comp) {
        comp->super_armor_stacks = std::max(0, comp->super_armor_stacks - stacks);
    }
}

void HitReactionSystem::clear_super_armor(scene::World& world, scene::Entity entity) {
    auto* comp = world.try_get<HitReactionComponent>(entity);
    if (comp) {
        comp->super_armor_stacks = 0;
    }
}

// ============================================================================
// ECS System Function
// ============================================================================

void hit_reaction_system(scene::World& world, double dt) {
    hit_reactions().update(world, static_cast<float>(dt));
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string get_reaction_type_name(HitReactionType type) {
    switch (type) {
        case HitReactionType::None:    return "None";
        case HitReactionType::Light:   return "Light";
        case HitReactionType::Medium:  return "Medium";
        case HitReactionType::Heavy:   return "Heavy";
        case HitReactionType::Stagger: return "Stagger";
        default:                       return "Unknown";
    }
}

} // namespace engine::combat
