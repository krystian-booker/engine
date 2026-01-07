#include <engine/effects/effect_component.hpp>
#include <engine/effects/effect_instance.hpp>
#include <engine/reflect/type_registry.hpp>

namespace engine::effects {

void register_effects_components() {
    auto& registry = reflect::TypeRegistry::instance();

    // Register EffectCategory enum
    // Register EffectCategory enum
    registry.register_enum<EffectCategory>("EffectCategory", {
        {EffectCategory::Buff, "Buff"},
        {EffectCategory::Debuff, "Debuff"},
        {EffectCategory::Neutral, "Neutral"},
        {EffectCategory::Passive, "Passive"},
        {EffectCategory::Aura, "Aura"}
    });

    // Register StackBehavior enum
    registry.register_enum<StackBehavior>("StackBehavior", {
        {StackBehavior::None, "None"},
        {StackBehavior::Refresh, "Refresh"},
        {StackBehavior::RefreshExtend, "RefreshExtend"},
        {StackBehavior::Intensity, "Intensity"},
        {StackBehavior::IntensityRefresh, "IntensityRefresh"},
        {StackBehavior::Independent, "Independent"}
    });

    // Register EffectState enum
    registry.register_enum<EffectState>("EffectState", {
        {EffectState::Pending, "Pending"},
        {EffectState::Active, "Active"},
        {EffectState::Paused, "Paused"},
        {EffectState::Expiring, "Expiring"},
        {EffectState::Expired, "Expired"},
        {EffectState::Removed, "Removed"},
        {EffectState::Blocked, "Blocked"}
    });

    // Register RemovalReason enum
    registry.register_enum<RemovalReason>("RemovalReason", {
        {RemovalReason::Expired, "Expired"},
        {RemovalReason::Dispelled, "Dispelled"},
        {RemovalReason::Purged, "Purged"},
        {RemovalReason::Replaced, "Replaced"},
        {RemovalReason::Cancelled, "Cancelled"},
        {RemovalReason::Death, "Death"},
        {RemovalReason::SourceDeath, "SourceDeath"},
        {RemovalReason::StacksDepleted, "StacksDepleted"},
        {RemovalReason::GameLogic, "GameLogic"}
    });

    // Register EffectInstance
    registry.register_type<EffectInstance>("EffectInstance");
    registry.register_property<EffectInstance, &EffectInstance::definition_id>("definition_id");
    registry.register_property<EffectInstance, &EffectInstance::duration>("duration");
    registry.register_property<EffectInstance, &EffectInstance::remaining>("remaining");
    registry.register_property<EffectInstance, &EffectInstance::elapsed>("elapsed");
    registry.register_property<EffectInstance, &EffectInstance::stacks>("stacks");
    registry.register_property<EffectInstance, &EffectInstance::intensity>("intensity");

    // Register ActiveEffectsComponent
    registry.register_component<ActiveEffectsComponent>("ActiveEffectsComponent");
    registry.register_property<ActiveEffectsComponent, &ActiveEffectsComponent::max_effects>("max_effects");

    // Register EffectSourceComponent
    registry.register_component<EffectSourceComponent>("EffectSourceComponent");
    registry.register_property<EffectSourceComponent, &EffectSourceComponent::duration_multiplier>("duration_multiplier");
    registry.register_property<EffectSourceComponent, &EffectSourceComponent::damage_multiplier>("damage_multiplier");
    registry.register_property<EffectSourceComponent, &EffectSourceComponent::heal_multiplier>("heal_multiplier");
    registry.register_property<EffectSourceComponent, &EffectSourceComponent::bonus_stacks>("bonus_stacks");

    // Register EffectAuraComponent
    registry.register_component<EffectAuraComponent>("EffectAuraComponent");
    registry.register_property<EffectAuraComponent, &EffectAuraComponent::effect_id>("effect_id");
    registry.register_property<EffectAuraComponent, &EffectAuraComponent::radius>("radius");
    registry.register_property<EffectAuraComponent, &EffectAuraComponent::apply_interval>("apply_interval");
    registry.register_property<EffectAuraComponent, &EffectAuraComponent::affects_self>("affects_self");
    registry.register_property<EffectAuraComponent, &EffectAuraComponent::affects_allies>("affects_allies");
    registry.register_property<EffectAuraComponent, &EffectAuraComponent::affects_enemies>("affects_enemies");
    registry.register_property<EffectAuraComponent, &EffectAuraComponent::faction>("faction");
    registry.register_property<EffectAuraComponent, &EffectAuraComponent::max_targets>("max_targets");
}

} // namespace engine::effects
