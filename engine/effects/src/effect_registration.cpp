#include <engine/effects/effect_component.hpp>
#include <engine/effects/effect_instance.hpp>
#include <engine/reflect/type_registry.hpp>

namespace engine::effects {

void register_effects_components() {
    auto& registry = reflect::TypeRegistry::instance();

    // Register EffectCategory enum
    registry.register_enum<EffectCategory>("EffectCategory")
        .value("Buff", EffectCategory::Buff)
        .value("Debuff", EffectCategory::Debuff)
        .value("Neutral", EffectCategory::Neutral)
        .value("Passive", EffectCategory::Passive)
        .value("Aura", EffectCategory::Aura);

    // Register StackBehavior enum
    registry.register_enum<StackBehavior>("StackBehavior")
        .value("None", StackBehavior::None)
        .value("Refresh", StackBehavior::Refresh)
        .value("RefreshExtend", StackBehavior::RefreshExtend)
        .value("Intensity", StackBehavior::Intensity)
        .value("IntensityRefresh", StackBehavior::IntensityRefresh)
        .value("Independent", StackBehavior::Independent);

    // Register EffectState enum
    registry.register_enum<EffectState>("EffectState")
        .value("Pending", EffectState::Pending)
        .value("Active", EffectState::Active)
        .value("Paused", EffectState::Paused)
        .value("Expiring", EffectState::Expiring)
        .value("Expired", EffectState::Expired)
        .value("Removed", EffectState::Removed)
        .value("Blocked", EffectState::Blocked);

    // Register RemovalReason enum
    registry.register_enum<RemovalReason>("RemovalReason")
        .value("Expired", RemovalReason::Expired)
        .value("Dispelled", RemovalReason::Dispelled)
        .value("Purged", RemovalReason::Purged)
        .value("Replaced", RemovalReason::Replaced)
        .value("Cancelled", RemovalReason::Cancelled)
        .value("Death", RemovalReason::Death)
        .value("SourceDeath", RemovalReason::SourceDeath)
        .value("StacksDepleted", RemovalReason::StacksDepleted)
        .value("GameLogic", RemovalReason::GameLogic);

    // Register EffectInstance
    registry.register_type<EffectInstance>("EffectInstance")
        .property("definition_id", &EffectInstance::definition_id)
        .property("duration", &EffectInstance::duration)
        .property("remaining", &EffectInstance::remaining)
        .property("elapsed", &EffectInstance::elapsed)
        .property("stacks", &EffectInstance::stacks)
        .property("intensity", &EffectInstance::intensity);

    // Register ActiveEffectsComponent
    registry.register_component<ActiveEffectsComponent>("ActiveEffectsComponent")
        .property("max_effects", &ActiveEffectsComponent::max_effects);

    // Register EffectSourceComponent
    registry.register_component<EffectSourceComponent>("EffectSourceComponent")
        .property("duration_multiplier", &EffectSourceComponent::duration_multiplier)
        .property("damage_multiplier", &EffectSourceComponent::damage_multiplier)
        .property("heal_multiplier", &EffectSourceComponent::heal_multiplier)
        .property("bonus_stacks", &EffectSourceComponent::bonus_stacks);

    // Register EffectAuraComponent
    registry.register_component<EffectAuraComponent>("EffectAuraComponent")
        .property("effect_id", &EffectAuraComponent::effect_id)
        .property("radius", &EffectAuraComponent::radius)
        .property("apply_interval", &EffectAuraComponent::apply_interval)
        .property("affects_self", &EffectAuraComponent::affects_self)
        .property("affects_allies", &EffectAuraComponent::affects_allies)
        .property("affects_enemies", &EffectAuraComponent::affects_enemies)
        .property("faction", &EffectAuraComponent::faction)
        .property("max_targets", &EffectAuraComponent::max_targets);
}

} // namespace engine::effects
