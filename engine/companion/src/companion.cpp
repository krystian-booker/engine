#include <engine/companion/companion.hpp>
#include <engine/reflect/type_registry.hpp>

namespace engine::companion {

const char* companion_state_to_string(CompanionState state) {
    switch (state) {
        case CompanionState::Following:   return "Following";
        case CompanionState::Waiting:     return "Waiting";
        case CompanionState::Attacking:   return "Attacking";
        case CompanionState::Defending:   return "Defending";
        case CompanionState::Moving:      return "Moving";
        case CompanionState::Interacting: return "Interacting";
        case CompanionState::Dead:        return "Dead";
        case CompanionState::Custom:      return "Custom";
        default:                          return "Unknown";
    }
}

void register_companion_components() {
    using namespace reflect;

    // Register CompanionState enum
    TypeRegistry::instance().register_enum<CompanionState>("CompanionState", {
        {CompanionState::Following, "Following"},
        {CompanionState::Waiting, "Waiting"},
        {CompanionState::Attacking, "Attacking"},
        {CompanionState::Defending, "Defending"},
        {CompanionState::Moving, "Moving"},
        {CompanionState::Interacting, "Interacting"},
        {CompanionState::Dead, "Dead"},
        {CompanionState::Custom, "Custom"}
    });

    // Register CompanionCommand enum
    TypeRegistry::instance().register_enum<CompanionCommand>("CompanionCommand", {
        {CompanionCommand::Follow, "Follow"},
        {CompanionCommand::Wait, "Wait"},
        {CompanionCommand::Attack, "Attack"},
        {CompanionCommand::Defend, "Defend"},
        {CompanionCommand::Move, "Move"},
        {CompanionCommand::Interact, "Interact"},
        {CompanionCommand::Dismiss, "Dismiss"},
        {CompanionCommand::Revive, "Revive"}
    });

    // Register CombatBehavior enum
    TypeRegistry::instance().register_enum<CombatBehavior>("CombatBehavior", {
        {CombatBehavior::Aggressive, "Aggressive"},
        {CombatBehavior::Defensive, "Defensive"},
        {CombatBehavior::Passive, "Passive"},
        {CombatBehavior::Support, "Support"}
    });

    // Register CompanionComponent
    TypeRegistry::instance().register_component<CompanionComponent>(
        "CompanionComponent",
        TypeMeta().set_display_name("Companion").set_category(TypeCategory::Component)
    );
}

} // namespace engine::companion
