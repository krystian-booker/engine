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
                core::events().dispatch(event);
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
    core::events().dispatch(event);
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
        core::events().dispatch(event);
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
    core::events().dispatch(start_event);

    if (m_on_phase_changed) {
        m_on_phase_changed(entity, old_phase, AttackPhase::Startup);
    }

    AttackPhaseChangedEvent phase_event;
    phase_event.entity = entity;
    phase_event.old_phase = old_phase;
    phase_event.new_phase = AttackPhase::Startup;
    phase_event.attack_name = attack.name;
    core::events().dispatch(phase_event);

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
    core::events().dispatch(end_event);

    AttackPhaseChangedEvent phase_event;
    phase_event.entity = entity;
    phase_event.old_phase = old_phase;
    phase_event.new_phase = AttackPhase::Canceled;
    phase_event.attack_name = attack_name;
    core::events().dispatch(phase_event);
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
                core::events().dispatch(end_event);
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
        core::events().dispatch(event);
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
    TypeRegistry::instance().register_component<HitboxComponent>("HitboxComponent",
        TypeMeta().set_display_name("Hitbox").set_category(TypeCategory::Component));

    TypeRegistry::instance().register_property<HitboxComponent, &HitboxComponent::active>("active",
        PropertyMeta().set_display_name("Active"));

    TypeRegistry::instance().register_property<HitboxComponent, &HitboxComponent::hitbox_id>("hitbox_id",
        PropertyMeta().set_display_name("Hitbox ID"));

    TypeRegistry::instance().register_property<HitboxComponent, &HitboxComponent::base_damage>("base_damage",
        PropertyMeta().set_display_name("Base Damage").set_range(0.0f, 1000.0f)); // Assuming max, no explicit max in old code but set_range requires it? No, set_range(min, max). Old used .min(). Use reasonably high max.

    TypeRegistry::instance().register_property<HitboxComponent, &HitboxComponent::damage_type>("damage_type",
        PropertyMeta().set_display_name("Damage Type"));

    TypeRegistry::instance().register_property<HitboxComponent, &HitboxComponent::knockback_force>("knockback_force",
        PropertyMeta().set_display_name("Knockback Force").set_range(0.0f, 1000.0f));

    TypeRegistry::instance().register_property<HitboxComponent, &HitboxComponent::radius>("radius",
        PropertyMeta().set_display_name("Radius").set_range(0.01f, 100.0f));

    // HurtboxComponent
    TypeRegistry::instance().register_component<HurtboxComponent>("HurtboxComponent",
        TypeMeta().set_display_name("Hurtbox").set_category(TypeCategory::Component));

    TypeRegistry::instance().register_property<HurtboxComponent, &HurtboxComponent::enabled>("enabled",
        PropertyMeta().set_display_name("Enabled"));

    TypeRegistry::instance().register_property<HurtboxComponent, &HurtboxComponent::hurtbox_type>("hurtbox_type",
        PropertyMeta().set_display_name("Hurtbox Type"));

    TypeRegistry::instance().register_property<HurtboxComponent, &HurtboxComponent::damage_multiplier>("damage_multiplier",
        PropertyMeta().set_display_name("Damage Multiplier").set_range(0.0f, 100.0f));

    TypeRegistry::instance().register_property<HurtboxComponent, &HurtboxComponent::radius>("radius",
        PropertyMeta().set_display_name("Radius").set_range(0.01f, 100.0f));

    // DamageReceiverComponent
    TypeRegistry::instance().register_component<DamageReceiverComponent>("DamageReceiverComponent",
        TypeMeta().set_display_name("Damage Receiver").set_category(TypeCategory::Component));

    TypeRegistry::instance().register_property<DamageReceiverComponent, &DamageReceiverComponent::max_poise>("max_poise",
        PropertyMeta().set_display_name("Max Poise").set_range(0.0f, 1000.0f));

    TypeRegistry::instance().register_property<DamageReceiverComponent, &DamageReceiverComponent::block_damage_reduction>("block_damage_reduction",
        PropertyMeta().set_display_name("Block Reduction").set_range(0.0f, 1.0f));

    // IFrameComponent
    TypeRegistry::instance().register_component<IFrameComponent>("IFrameComponent",
        TypeMeta().set_display_name("I-Frames").set_category(TypeCategory::Component));

    TypeRegistry::instance().register_property<IFrameComponent, &IFrameComponent::is_invincible>("is_invincible",
        PropertyMeta().set_display_name("Is Invincible").set_read_only(true));

    TypeRegistry::instance().register_property<IFrameComponent, &IFrameComponent::flash_enabled>("flash_enabled",
        PropertyMeta().set_display_name("Flash Enabled"));

    TypeRegistry::instance().register_property<IFrameComponent, &IFrameComponent::flash_interval>("flash_interval",
        PropertyMeta().set_display_name("Flash Interval").set_range(0.01f, 5.0f));

    // AttackPhaseComponent
    TypeRegistry::instance().register_component<AttackPhaseComponent>("AttackPhaseComponent",
        TypeMeta().set_display_name("Attack Phase").set_category(TypeCategory::Component));

    TypeRegistry::instance().register_property<AttackPhaseComponent, &AttackPhaseComponent::current_attack>("current_attack",
        PropertyMeta().set_display_name("Current Attack").set_read_only(true));

    TypeRegistry::instance().register_property<AttackPhaseComponent, &AttackPhaseComponent::combo_count>("combo_count",
        PropertyMeta().set_display_name("Combo Count").set_read_only(true));

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
