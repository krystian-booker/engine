#include <engine/combat/combat.hpp>
#include <engine/combat/hitbox.hpp>
#include <engine/combat/hurtbox.hpp>
#include <engine/combat/damage.hpp>
#include <engine/combat/iframe.hpp>
#include <engine/combat/attack_phases.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/systems.hpp>
#include <engine/scene/transform.hpp>
#include <engine/core/event_dispatcher.hpp>
#include <engine/core/log.hpp>
#include <engine/reflect/type_registry.hpp>

namespace engine::combat {

// ============================================================================
// I-Frame System
// ============================================================================

void iframe_system(scene::World& world, double dt) {
    auto view = world.view<IFrameComponent>();

    for (auto entity : view) {
        auto& iframe = view.get<IFrameComponent>(entity);

        if (iframe.is_invincible) {
            bool ended = iframe.update(static_cast<float>(dt));

            if (ended) {
                // Emit end event
                IFramesEndedEvent event;
                event.entity = entity;
                event.source = iframe.source;
                core::EventDispatcher::instance().dispatch(event);
            }
        }
    }
}

// I-frame utility functions
namespace iframe {

void grant(scene::World& world, scene::Entity entity, float duration, IFrameSource source) {
    auto* iframe = world.try_get<IFrameComponent>(entity);
    if (!iframe) {
        // Add component if missing
        iframe = &world.emplace<IFrameComponent>(entity);
    }

    iframe->grant(duration, source);

    // Emit start event
    IFramesStartedEvent event;
    event.entity = entity;
    event.duration = duration;
    event.source = source;
    core::EventDispatcher::instance().dispatch(event);
}

void grant_default(scene::World& world, scene::Entity entity, IFrameSource source) {
    float duration = 0.3f;
    switch (source) {
        case IFrameSource::Dodge: duration = IFrameComponent::DEFAULT_DODGE_DURATION; break;
        case IFrameSource::Hit: duration = IFrameComponent::DEFAULT_HIT_DURATION; break;
        case IFrameSource::Spawn: duration = IFrameComponent::DEFAULT_SPAWN_DURATION; break;
        default: break;
    }
    grant(world, entity, duration, source);
}

bool is_invincible(scene::World& world, scene::Entity entity) {
    auto* iframe = world.try_get<IFrameComponent>(entity);
    return iframe && iframe->is_invincible;
}

void cancel(scene::World& world, scene::Entity entity) {
    auto* iframe = world.try_get<IFrameComponent>(entity);
    if (iframe) {
        IFrameSource source = iframe->source;
        iframe->cancel();

        IFramesEndedEvent event;
        event.entity = entity;
        event.source = source;
        core::EventDispatcher::instance().dispatch(event);
    }
}

float get_remaining(scene::World& world, scene::Entity entity) {
    auto* iframe = world.try_get<IFrameComponent>(entity);
    return iframe ? iframe->remaining_time : 0.0f;
}

} // namespace iframe

// ============================================================================
// Attack Phase System
// ============================================================================

AttackPhaseManager::AttackPhaseManager() {
}

AttackPhaseManager& AttackPhaseManager::instance() {
    static AttackPhaseManager s_instance;
    return s_instance;
}

bool AttackPhaseManager::start_attack(scene::World& world, scene::Entity entity,
                                       const std::string& attack_name) {
    auto* attack_def = get_attack(attack_name);
    if (!attack_def) {
        core::log(core::LogLevel::Warn, "Attack not found: {}", attack_name);
        return false;
    }
    return start_attack(world, entity, *attack_def);
}

bool AttackPhaseManager::start_attack(scene::World& world, scene::Entity entity,
                                       const AttackDefinition& attack) {
    auto* phase = world.try_get<AttackPhaseComponent>(entity);
    if (!phase) {
        phase = &world.emplace<AttackPhaseComponent>(entity);
    }

    // Check if we can cancel current attack
    if (phase->is_attacking() && !phase->can_cancel()) {
        // Queue the attack for later
        phase->queue_attack(attack.name);
        return false;
    }

    AttackPhase old_phase = phase->current_phase;

    // Deactivate previous attack hitboxes
    if (phase->is_attacking()) {
        deactivate_hitboxes(world, entity, phase->attack_def);
    }

    // Set up new attack
    phase->attack_def = attack;
    phase->current_attack = attack.name;
    phase->current_phase = AttackPhase::Startup;
    phase->phase_time = 0.0f;
    phase->phase_duration = attack.startup_duration;
    phase->queued_attack.clear();

    // Track combo
    if (!attack.next_combo_attack.empty()) {
        phase->combo_count++;
    } else {
        phase->combo_count = 1;
    }

    // Emit events
    AttackStartedEvent start_event;
    start_event.entity = entity;
    start_event.attack_name = attack.name;
    core::EventDispatcher::instance().dispatch(start_event);

    if (m_on_phase_changed) {
        m_on_phase_changed(entity, old_phase, AttackPhase::Startup);
    }

    AttackPhaseChangedEvent phase_event;
    phase_event.entity = entity;
    phase_event.old_phase = old_phase;
    phase_event.new_phase = AttackPhase::Startup;
    phase_event.attack_name = attack.name;
    core::EventDispatcher::instance().dispatch(phase_event);

    return true;
}

void AttackPhaseManager::cancel_attack(scene::World& world, scene::Entity entity) {
    auto* phase = world.try_get<AttackPhaseComponent>(entity);
    if (!phase || !phase->is_attacking()) return;

    AttackPhase old_phase = phase->current_phase;
    std::string attack_name = phase->current_attack;

    // Deactivate hitboxes
    deactivate_hitboxes(world, entity, phase->attack_def);

    phase->current_phase = AttackPhase::Canceled;
    phase->clear();

    // Emit events
    AttackEndedEvent end_event;
    end_event.entity = entity;
    end_event.attack_name = attack_name;
    end_event.was_canceled = true;
    core::EventDispatcher::instance().dispatch(end_event);

    AttackPhaseChangedEvent phase_event;
    phase_event.entity = entity;
    phase_event.old_phase = old_phase;
    phase_event.new_phase = AttackPhase::Canceled;
    phase_event.attack_name = attack_name;
    core::EventDispatcher::instance().dispatch(phase_event);
}

void AttackPhaseManager::process_attack_input(scene::World& world, scene::Entity entity,
                                               const std::string& attack_name) {
    auto* phase = world.try_get<AttackPhaseComponent>(entity);

    // Not attacking - start fresh
    if (!phase || !phase->is_attacking()) {
        start_attack(world, entity, attack_name);
        return;
    }

    // Check if we can combo
    if (phase->can_combo()) {
        // Check if there's a specific combo follow-up
        if (!phase->attack_def.next_combo_attack.empty()) {
            start_attack(world, entity, phase->attack_def.next_combo_attack);
        } else {
            start_attack(world, entity, attack_name);
        }
        return;
    }

    // Queue the attack for input buffering
    phase->queue_attack(attack_name);
}

void AttackPhaseManager::register_attack(const AttackDefinition& attack) {
    m_attacks[attack.name] = attack;
}

void AttackPhaseManager::register_attacks(const std::vector<AttackDefinition>& attacks) {
    for (const auto& attack : attacks) {
        register_attack(attack);
    }
}

const AttackDefinition* AttackPhaseManager::get_attack(const std::string& name) const {
    auto it = m_attacks.find(name);
    return it != m_attacks.end() ? &it->second : nullptr;
}

std::vector<std::string> AttackPhaseManager::get_registered_attacks() const {
    std::vector<std::string> names;
    names.reserve(m_attacks.size());
    for (const auto& [name, _] : m_attacks) {
        names.push_back(name);
    }
    return names;
}

void AttackPhaseManager::advance_phase(scene::World& world, scene::Entity entity,
                                        AttackPhaseComponent& attack) {
    AttackPhase old_phase = attack.current_phase;
    AttackPhase new_phase = old_phase;

    switch (old_phase) {
        case AttackPhase::Startup:
            new_phase = AttackPhase::Active;
            attack.phase_duration = attack.attack_def.active_duration;
            activate_hitboxes(world, entity, attack.attack_def);
            break;

        case AttackPhase::Active:
            new_phase = AttackPhase::Recovery;
            attack.phase_duration = attack.attack_def.recovery_duration;
            attack.combo_window_timer = attack.combo_window_duration;
            deactivate_hitboxes(world, entity, attack.attack_def);
            break;

        case AttackPhase::Recovery:
            // Attack complete
            new_phase = AttackPhase::None;

            // Check for queued attack
            if (!attack.queued_attack.empty()) {
                std::string queued = attack.queued_attack;
                attack.clear();
                start_attack(world, entity, queued);
                return;
            }

            {
                std::string attack_name = attack.current_attack;
                attack.clear();

                AttackEndedEvent end_event;
                end_event.entity = entity;
                end_event.attack_name = attack_name;
                end_event.was_canceled = false;
                core::EventDispatcher::instance().dispatch(end_event);
            }
            break;

        default:
            break;
    }

    attack.current_phase = new_phase;
    attack.phase_time = 0.0f;

    // Emit phase change event
    if (old_phase != new_phase) {
        if (m_on_phase_changed) {
            m_on_phase_changed(entity, old_phase, new_phase);
        }

        AttackPhaseChangedEvent event;
        event.entity = entity;
        event.old_phase = old_phase;
        event.new_phase = new_phase;
        event.attack_name = attack.current_attack;
        core::EventDispatcher::instance().dispatch(event);
    }
}

void AttackPhaseManager::activate_hitboxes(scene::World& world, scene::Entity entity,
                                            const AttackDefinition& attack) {
    // Find hitboxes on this entity and activate those matching the attack's hitbox IDs
    auto* hitbox = world.try_get<HitboxComponent>(entity);
    if (hitbox) {
        for (const auto& id : attack.hitbox_ids) {
            if (hitbox->hitbox_id == id || attack.hitbox_ids.empty()) {
                hitbox->activate();
                break;
            }
        }
    }

    // Also check child entities for hitboxes (for weapon hitboxes)
    // This would need hierarchy traversal - simplified for now
}

void AttackPhaseManager::deactivate_hitboxes(scene::World& world, scene::Entity entity,
                                              const AttackDefinition& attack) {
    auto* hitbox = world.try_get<HitboxComponent>(entity);
    if (hitbox) {
        hitbox->deactivate();
    }
}

// Attack phase system (called every frame)
void attack_phase_system(scene::World& world, double dt) {
    auto& manager = AttackPhaseManager::instance();
    auto& damage_system = DamageSystem::instance();

    // Update hitstop
    damage_system.update_hitstop(static_cast<float>(dt));

    // Get time scale (0 during hitstop)
    float time_scale = damage_system.get_hitstop_time_scale();
    float scaled_dt = static_cast<float>(dt) * time_scale;

    auto view = world.view<AttackPhaseComponent>();

    for (auto entity : view) {
        auto& attack = view.get<AttackPhaseComponent>(entity);

        if (!attack.is_attacking()) continue;

        // Update phase timer
        attack.phase_time += scaled_dt;

        // Update combo window timer
        if (attack.combo_window_timer > 0.0f) {
            attack.combo_window_timer -= scaled_dt;
        }

        // Check phase transition
        if (attack.phase_time >= attack.phase_duration) {
            manager.advance_phase(world, entity, attack);
        }
    }
}

// ============================================================================
// Poise Recovery System
// ============================================================================

void poise_recovery_system(scene::World& world, double dt) {
    auto view = world.view<DamageReceiverComponent>();

    for (auto entity : view) {
        auto& receiver = view.get<DamageReceiverComponent>(entity);
        receiver.recover_poise(static_cast<float>(dt));

        // Update parry window
        if (receiver.parry_window > 0.0f) {
            receiver.parry_window -= static_cast<float>(dt);
            if (receiver.parry_window < 0.0f) {
                receiver.parry_window = 0.0f;
            }
        }
    }
}

// ============================================================================
// Component Registration
// ============================================================================

void register_combat_components() {
    using namespace reflect;

    // HitboxComponent
    TypeRegistry::instance().register_component<HitboxComponent>("HitboxComponent")
        .display_name("Hitbox")
        .category("Combat");

    TypeRegistry::instance().register_property<HitboxComponent>("active",
        [](const HitboxComponent& c) { return c.active; },
        [](HitboxComponent& c, bool v) { c.active = v; })
        .display_name("Active");

    TypeRegistry::instance().register_property<HitboxComponent>("hitbox_id",
        [](const HitboxComponent& c) { return c.hitbox_id; },
        [](HitboxComponent& c, const std::string& v) { c.hitbox_id = v; })
        .display_name("Hitbox ID");

    TypeRegistry::instance().register_property<HitboxComponent>("base_damage",
        [](const HitboxComponent& c) { return c.base_damage; },
        [](HitboxComponent& c, float v) { c.base_damage = v; })
        .display_name("Base Damage").min(0.0f);

    TypeRegistry::instance().register_property<HitboxComponent>("damage_type",
        [](const HitboxComponent& c) { return c.damage_type; },
        [](HitboxComponent& c, const std::string& v) { c.damage_type = v; })
        .display_name("Damage Type");

    TypeRegistry::instance().register_property<HitboxComponent>("knockback_force",
        [](const HitboxComponent& c) { return c.knockback_force; },
        [](HitboxComponent& c, float v) { c.knockback_force = v; })
        .display_name("Knockback Force").min(0.0f);

    TypeRegistry::instance().register_property<HitboxComponent>("radius",
        [](const HitboxComponent& c) { return c.radius; },
        [](HitboxComponent& c, float v) { c.radius = v; })
        .display_name("Radius").min(0.01f);

    // HurtboxComponent
    TypeRegistry::instance().register_component<HurtboxComponent>("HurtboxComponent")
        .display_name("Hurtbox")
        .category("Combat");

    TypeRegistry::instance().register_property<HurtboxComponent>("enabled",
        [](const HurtboxComponent& c) { return c.enabled; },
        [](HurtboxComponent& c, bool v) { c.enabled = v; })
        .display_name("Enabled");

    TypeRegistry::instance().register_property<HurtboxComponent>("hurtbox_type",
        [](const HurtboxComponent& c) { return c.hurtbox_type; },
        [](HurtboxComponent& c, const std::string& v) { c.hurtbox_type = v; })
        .display_name("Hurtbox Type");

    TypeRegistry::instance().register_property<HurtboxComponent>("damage_multiplier",
        [](const HurtboxComponent& c) { return c.damage_multiplier; },
        [](HurtboxComponent& c, float v) { c.damage_multiplier = v; })
        .display_name("Damage Multiplier").min(0.0f);

    TypeRegistry::instance().register_property<HurtboxComponent>("radius",
        [](const HurtboxComponent& c) { return c.radius; },
        [](HurtboxComponent& c, float v) { c.radius = v; })
        .display_name("Radius").min(0.01f);

    // DamageReceiverComponent
    TypeRegistry::instance().register_component<DamageReceiverComponent>("DamageReceiverComponent")
        .display_name("Damage Receiver")
        .category("Combat");

    TypeRegistry::instance().register_property<DamageReceiverComponent>("max_poise",
        [](const DamageReceiverComponent& c) { return c.max_poise; },
        [](DamageReceiverComponent& c, float v) { c.max_poise = v; })
        .display_name("Max Poise").min(0.0f);

    TypeRegistry::instance().register_property<DamageReceiverComponent>("block_damage_reduction",
        [](const DamageReceiverComponent& c) { return c.block_damage_reduction; },
        [](DamageReceiverComponent& c, float v) { c.block_damage_reduction = v; })
        .display_name("Block Reduction").min(0.0f).max(1.0f);

    // IFrameComponent
    TypeRegistry::instance().register_component<IFrameComponent>("IFrameComponent")
        .display_name("I-Frames")
        .category("Combat");

    TypeRegistry::instance().register_property<IFrameComponent>("is_invincible",
        [](const IFrameComponent& c) { return c.is_invincible; },
        [](IFrameComponent& c, bool v) { c.is_invincible = v; })
        .display_name("Is Invincible").read_only();

    TypeRegistry::instance().register_property<IFrameComponent>("flash_enabled",
        [](const IFrameComponent& c) { return c.flash_enabled; },
        [](IFrameComponent& c, bool v) { c.flash_enabled = v; })
        .display_name("Flash Enabled");

    TypeRegistry::instance().register_property<IFrameComponent>("flash_interval",
        [](const IFrameComponent& c) { return c.flash_interval; },
        [](IFrameComponent& c, float v) { c.flash_interval = v; })
        .display_name("Flash Interval").min(0.01f);

    // AttackPhaseComponent
    TypeRegistry::instance().register_component<AttackPhaseComponent>("AttackPhaseComponent")
        .display_name("Attack Phase")
        .category("Combat");

    TypeRegistry::instance().register_property<AttackPhaseComponent>("current_attack",
        [](const AttackPhaseComponent& c) { return c.current_attack; },
        [](AttackPhaseComponent& c, const std::string& v) { c.current_attack = v; })
        .display_name("Current Attack").read_only();

    TypeRegistry::instance().register_property<AttackPhaseComponent>("combo_count",
        [](const AttackPhaseComponent& c) { return c.combo_count; },
        [](AttackPhaseComponent& c, int v) { c.combo_count = v; })
        .display_name("Combo Count").read_only();

    core::log(core::LogLevel::Info, "Combat components registered");
}

// ============================================================================
// System Registration
// ============================================================================

void register_combat_systems(scene::World& world) {
    // Note: Systems are registered via the scheduler in the application
    // This function provides documentation of which systems exist

    // Systems to register:
    // - hitbox_detection_system: Phase::FixedUpdate, priority 100
    // - iframe_system: Phase::FixedUpdate, priority 90
    // - attack_phase_system: Phase::Update, priority 100
    // - poise_recovery_system: Phase::Update, priority 50

    core::log(core::LogLevel::Info, "Combat systems ready for registration");
}

} // namespace engine::combat
