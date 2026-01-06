#include <engine/effects/effect_component.hpp>
#include <algorithm>

namespace engine::effects {

// ============================================================================
// ActiveEffectsComponent Implementation
// ============================================================================

bool ActiveEffectsComponent::has_effect(const std::string& effect_id) const {
    return std::any_of(effects.begin(), effects.end(),
                       [&effect_id](const EffectInstance& e) {
                           return e.definition_id == effect_id && e.is_active();
                       });
}

bool ActiveEffectsComponent::has_effect_with_tag(const std::string& tag) const {
    for (const auto& effect : effects) {
        if (!effect.is_active()) continue;
        const EffectDefinition* def = effect.get_definition();
        if (def && def->has_tag(tag)) {
            return true;
        }
    }
    return false;
}

bool ActiveEffectsComponent::has_buff() const {
    return std::any_of(effects.begin(), effects.end(),
                       [](const EffectInstance& e) {
                           if (!e.is_active()) return false;
                           const EffectDefinition* def = e.get_definition();
                           return def && def->is_buff();
                       });
}

bool ActiveEffectsComponent::has_debuff() const {
    return std::any_of(effects.begin(), effects.end(),
                       [](const EffectInstance& e) {
                           if (!e.is_active()) return false;
                           const EffectDefinition* def = e.get_definition();
                           return def && def->is_debuff();
                       });
}

EffectInstance* ActiveEffectsComponent::get_effect(const std::string& effect_id) {
    auto it = std::find_if(effects.begin(), effects.end(),
                           [&effect_id](const EffectInstance& e) {
                               return e.definition_id == effect_id && e.is_active();
                           });
    return it != effects.end() ? &(*it) : nullptr;
}

const EffectInstance* ActiveEffectsComponent::get_effect(const std::string& effect_id) const {
    auto it = std::find_if(effects.begin(), effects.end(),
                           [&effect_id](const EffectInstance& e) {
                               return e.definition_id == effect_id && e.is_active();
                           });
    return it != effects.end() ? &(*it) : nullptr;
}

EffectInstance* ActiveEffectsComponent::get_effect_by_id(const core::UUID& instance_id) {
    auto it = std::find_if(effects.begin(), effects.end(),
                           [&instance_id](const EffectInstance& e) {
                               return e.instance_id == instance_id;
                           });
    return it != effects.end() ? &(*it) : nullptr;
}

std::vector<EffectInstance*> ActiveEffectsComponent::get_effects_by_category(EffectCategory category) {
    std::vector<EffectInstance*> result;
    for (auto& effect : effects) {
        if (!effect.is_active()) continue;
        const EffectDefinition* def = effect.get_definition();
        if (def && def->category == category) {
            result.push_back(&effect);
        }
    }
    return result;
}

std::vector<EffectInstance*> ActiveEffectsComponent::get_effects_with_tag(const std::string& tag) {
    std::vector<EffectInstance*> result;
    for (auto& effect : effects) {
        if (!effect.is_active()) continue;
        const EffectDefinition* def = effect.get_definition();
        if (def && def->has_tag(tag)) {
            result.push_back(&effect);
        }
    }
    return result;
}

std::vector<EffectInstance*> ActiveEffectsComponent::get_buffs() {
    return get_effects_by_category(EffectCategory::Buff);
}

std::vector<EffectInstance*> ActiveEffectsComponent::get_debuffs() {
    return get_effects_by_category(EffectCategory::Debuff);
}

int ActiveEffectsComponent::get_stack_count(const std::string& effect_id) const {
    const EffectInstance* instance = get_effect(effect_id);
    return instance ? instance->stacks : 0;
}

float ActiveEffectsComponent::get_remaining_duration(const std::string& effect_id) const {
    const EffectInstance* instance = get_effect(effect_id);
    return instance ? instance->remaining : 0.0f;
}

int ActiveEffectsComponent::count_buffs() const {
    int count = 0;
    for (const auto& effect : effects) {
        if (!effect.is_active()) continue;
        const EffectDefinition* def = effect.get_definition();
        if (def && def->is_buff()) {
            ++count;
        }
    }
    return count;
}

int ActiveEffectsComponent::count_debuffs() const {
    int count = 0;
    for (const auto& effect : effects) {
        if (!effect.is_active()) continue;
        const EffectDefinition* def = effect.get_definition();
        if (def && def->is_debuff()) {
            ++count;
        }
    }
    return count;
}

// ============================================================================
// Immunity
// ============================================================================

bool ActiveEffectsComponent::is_immune_to(const std::string& effect_id) const {
    // Direct immunity
    if (immunities.find(effect_id) != immunities.end()) {
        return true;
    }

    // Check category immunity
    const EffectDefinition* def = effect_registry().get(effect_id);
    if (def) {
        if (category_immunities.find(def->category) != category_immunities.end()) {
            return true;
        }

        // Check tag immunity
        for (const auto& tag : def->tags) {
            if (tag_immunities.find(tag) != tag_immunities.end()) {
                return true;
            }
        }
    }

    // Check if any active effect grants immunity
    for (const auto& effect : effects) {
        if (!effect.is_active()) continue;
        const EffectDefinition* effect_def = effect.get_definition();
        if (!effect_def) continue;

        for (const auto& immune_id : effect_def->grants_immunity) {
            if (immune_id == effect_id) {
                return true;
            }
        }
    }

    return false;
}

bool ActiveEffectsComponent::is_immune_to_category(EffectCategory category) const {
    return category_immunities.find(category) != category_immunities.end();
}

bool ActiveEffectsComponent::is_immune_to_tags(const std::vector<std::string>& tags) const {
    for (const auto& tag : tags) {
        if (tag_immunities.find(tag) != tag_immunities.end()) {
            return true;
        }
    }
    return false;
}

void ActiveEffectsComponent::add_immunity(const std::string& effect_id) {
    immunities.insert(effect_id);
}

void ActiveEffectsComponent::add_category_immunity(EffectCategory category) {
    category_immunities.insert(category);
}

void ActiveEffectsComponent::add_tag_immunity(const std::string& tag) {
    tag_immunities.insert(tag);
}

void ActiveEffectsComponent::remove_immunity(const std::string& effect_id) {
    immunities.erase(effect_id);
}

void ActiveEffectsComponent::remove_category_immunity(EffectCategory category) {
    category_immunities.erase(category);
}

void ActiveEffectsComponent::remove_tag_immunity(const std::string& tag) {
    tag_immunities.erase(tag);
}

void ActiveEffectsComponent::clear_immunities() {
    immunities.clear();
    category_immunities.clear();
    tag_immunities.clear();
}

// ============================================================================
// Utility
// ============================================================================

float ActiveEffectsComponent::get_total_modifier(stats::StatType stat, stats::ModifierType type) const {
    float total = 0.0f;

    for (const auto& effect : effects) {
        if (!effect.is_active()) continue;
        const EffectDefinition* def = effect.get_definition();
        if (!def) continue;

        for (const auto& mod : def->stat_modifiers) {
            if (mod.stat == stat && mod.type == type) {
                total += mod.value * effect.intensity;
            }
        }
    }

    return total;
}

void ActiveEffectsComponent::cleanup_expired() {
    effects.erase(
        std::remove_if(effects.begin(), effects.end(),
                       [](const EffectInstance& e) { return e.is_expired(); }),
        effects.end()
    );
}

void ActiveEffectsComponent::sort_by_priority() {
    std::sort(effects.begin(), effects.end(),
              [](const EffectInstance& a, const EffectInstance& b) {
                  const EffectDefinition* def_a = a.get_definition();
                  const EffectDefinition* def_b = b.get_definition();
                  int priority_a = def_a ? def_a->dispel_priority : 0;
                  int priority_b = def_b ? def_b->dispel_priority : 0;
                  return priority_a > priority_b;
              });
}

void ActiveEffectsComponent::sort_by_remaining() {
    std::sort(effects.begin(), effects.end(),
              [](const EffectInstance& a, const EffectInstance& b) {
                  // Permanent effects last
                  if (a.is_permanent() && !b.is_permanent()) return false;
                  if (!a.is_permanent() && b.is_permanent()) return true;
                  return a.remaining < b.remaining;
              });
}

} // namespace engine::effects
