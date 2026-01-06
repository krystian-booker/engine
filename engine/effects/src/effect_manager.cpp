#include <engine/effects/effect_manager.hpp>
#include <engine/effects/effect_events.hpp>
#include <engine/stats/stat_component.hpp>
#include <engine/core/game_events.hpp>
#include <algorithm>
#include <random>

namespace engine::effects {

// ============================================================================
// EffectManager Implementation
// ============================================================================

EffectManager& EffectManager::instance() {
    static EffectManager s_instance;
    return s_instance;
}

// ============================================================================
// Application
// ============================================================================

ApplyResultInfo EffectManager::apply(scene::World& world,
                                      scene::Entity target,
                                      const std::string& effect_id,
                                      scene::Entity source) {
    return apply(world, target, effect_id, source, -1.0f, 1);
}

ApplyResultInfo EffectManager::apply(scene::World& world,
                                      scene::Entity target,
                                      const std::string& effect_id,
                                      scene::Entity source,
                                      float duration_override,
                                      int stack_override) {
    ApplyResultInfo result;
    result.result = ApplyResult::Failed;

    // Validate target
    if (!world.valid(target)) {
        result.result = ApplyResult::TargetInvalid;
        return result;
    }

    // Get definition
    const EffectDefinition* def = effect_registry().get(effect_id);
    if (!def) {
        result.result = ApplyResult::DefinitionNotFound;
        return result;
    }

    // Ensure target has ActiveEffectsComponent
    if (!world.has<ActiveEffectsComponent>(target)) {
        world.emplace<ActiveEffectsComponent>(target);
    }

    auto& comp = world.get<ActiveEffectsComponent>(target);

    // Check immunity
    if (check_immunity(comp, *def)) {
        result.result = ApplyResult::Blocked;
        result.blocked_by = "immunity";

        // Emit blocked event
        core::game_events().emit(EffectBlockedEvent{
            target, source, effect_id, "immunity"
        });

        return result;
    }

    // Check blocked_by list
    for (const auto& blocker : def->blocked_by) {
        if (comp.has_effect(blocker)) {
            result.result = ApplyResult::Blocked;
            result.blocked_by = blocker;

            core::game_events().emit(EffectBlockedEvent{
                target, source, effect_id, blocker
            });

            return result;
        }
    }

    // Remove effects that this one replaces
    for (const auto& remove_id : def->removes_effects) {
        remove(world, target, remove_id, RemovalReason::Replaced);
    }

    // Check if effect already exists
    EffectInstance* existing = comp.get_effect(effect_id);

    if (existing) {
        // Handle stacking behavior
        switch (def->stacking) {
            case StackBehavior::None:
                result.result = ApplyResult::AlreadyAtMax;
                result.instance = existing;
                return result;

            case StackBehavior::Refresh:
                existing->refresh_duration();
                result.result = ApplyResult::Refreshed;
                result.instance = existing;
                result.new_duration = existing->duration;
                break;

            case StackBehavior::RefreshExtend:
                existing->extend_duration(def->base_duration);
                result.result = ApplyResult::Extended;
                result.instance = existing;
                result.new_duration = existing->remaining;
                break;

            case StackBehavior::Intensity:
                if (existing->can_add_stack()) {
                    existing->add_stack(stack_override);
                    result.result = ApplyResult::Stacked;
                    result.instance = existing;
                    result.new_stack_count = existing->stacks;
                } else {
                    result.result = ApplyResult::AlreadyAtMax;
                    result.instance = existing;
                }
                break;

            case StackBehavior::IntensityRefresh:
                existing->refresh_duration();
                if (existing->can_add_stack()) {
                    existing->add_stack(stack_override);
                    result.result = ApplyResult::StackedAndRefreshed;
                } else {
                    result.result = ApplyResult::Refreshed;
                }
                result.instance = existing;
                result.new_stack_count = existing->stacks;
                result.new_duration = existing->duration;
                break;

            case StackBehavior::Independent:
                // Fall through to create new instance
                existing = nullptr;
                break;
        }

        if (existing) {
            // Emit refresh/stack event
            if (m_on_stack_change) {
                m_on_stack_change(world, target, *existing);
            }
            return result;
        }
    }

    // Create new instance
    EffectInstance instance = EffectInstance::create(effect_id, target, source);

    // Apply overrides
    if (duration_override > 0.0f) {
        instance.duration = duration_override;
        instance.remaining = duration_override;
    }
    instance.stacks = stack_override;

    // Apply source modifiers if source has EffectSourceComponent
    if (world.valid(source) && world.has<EffectSourceComponent>(source)) {
        const auto& src_comp = world.get<EffectSourceComponent>(source);
        instance.duration_multiplier = src_comp.duration_multiplier;
        instance.damage_multiplier = src_comp.damage_multiplier;
        instance.heal_multiplier = src_comp.heal_multiplier;
        instance.stacks += src_comp.bonus_stacks;
    }

    // Activate
    instance.state = EffectState::Active;

    // Apply stat modifiers
    apply_stat_modifiers(world, target, instance);

    // Add to component
    comp.effects.push_back(std::move(instance));
    result.instance = &comp.effects.back();
    result.result = ApplyResult::Applied;
    result.new_stack_count = result.instance->stacks;
    result.new_duration = result.instance->duration;

    // Emit event
    core::game_events().emit(EffectAppliedEvent{
        target, source, effect_id, result.instance->instance_id,
        result.new_stack_count, result.new_duration, false, false
    });

    if (m_on_apply) {
        m_on_apply(world, target, *result.instance);
    }

    return result;
}

ApplyResultInfo EffectManager::apply(scene::World& world,
                                      scene::Entity target,
                                      EffectInstance instance) {
    ApplyResultInfo result;
    result.result = ApplyResult::Failed;

    if (!world.valid(target)) {
        result.result = ApplyResult::TargetInvalid;
        return result;
    }

    instance.target = target;
    instance.state = EffectState::Active;

    if (!world.has<ActiveEffectsComponent>(target)) {
        world.emplace<ActiveEffectsComponent>(target);
    }

    auto& comp = world.get<ActiveEffectsComponent>(target);

    apply_stat_modifiers(world, target, instance);

    comp.effects.push_back(std::move(instance));
    result.instance = &comp.effects.back();
    result.result = ApplyResult::Applied;

    return result;
}

ApplyResultInfo EffectManager::try_apply(scene::World& world,
                                          scene::Entity target,
                                          const std::string& effect_id,
                                          float chance,
                                          scene::Entity source) {
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(0.0f, 100.0f);

    if (dist(rng) > chance) {
        ApplyResultInfo result;
        result.result = ApplyResult::Failed;
        return result;
    }

    return apply(world, target, effect_id, source);
}

// ============================================================================
// Removal
// ============================================================================

bool EffectManager::remove(scene::World& world,
                           scene::Entity target,
                           const std::string& effect_id,
                           RemovalReason reason) {
    if (!world.valid(target) || !world.has<ActiveEffectsComponent>(target)) {
        return false;
    }

    auto& comp = world.get<ActiveEffectsComponent>(target);

    auto it = std::find_if(comp.effects.begin(), comp.effects.end(),
                           [&effect_id](const EffectInstance& e) {
                               return e.definition_id == effect_id && e.is_active();
                           });

    if (it == comp.effects.end()) {
        return false;
    }

    handle_removal(world, target, *it, reason);
    comp.effects.erase(it);
    return true;
}

bool EffectManager::remove_by_instance_id(scene::World& world,
                                           scene::Entity target,
                                           const core::UUID& instance_id,
                                           RemovalReason reason) {
    if (!world.valid(target) || !world.has<ActiveEffectsComponent>(target)) {
        return false;
    }

    auto& comp = world.get<ActiveEffectsComponent>(target);

    auto it = std::find_if(comp.effects.begin(), comp.effects.end(),
                           [&instance_id](const EffectInstance& e) {
                               return e.instance_id == instance_id;
                           });

    if (it == comp.effects.end()) {
        return false;
    }

    handle_removal(world, target, *it, reason);
    comp.effects.erase(it);
    return true;
}

int EffectManager::remove_from_source(scene::World& world,
                                       scene::Entity target,
                                       scene::Entity source,
                                       RemovalReason reason) {
    if (!world.valid(target) || !world.has<ActiveEffectsComponent>(target)) {
        return 0;
    }

    auto& comp = world.get<ActiveEffectsComponent>(target);
    int count = 0;

    for (auto it = comp.effects.begin(); it != comp.effects.end();) {
        if (it->source == source) {
            handle_removal(world, target, *it, reason);
            it = comp.effects.erase(it);
            ++count;
        } else {
            ++it;
        }
    }

    return count;
}

int EffectManager::remove_with_tag(scene::World& world,
                                    scene::Entity target,
                                    const std::string& tag,
                                    RemovalReason reason) {
    if (!world.valid(target) || !world.has<ActiveEffectsComponent>(target)) {
        return 0;
    }

    auto& comp = world.get<ActiveEffectsComponent>(target);
    int count = 0;

    for (auto it = comp.effects.begin(); it != comp.effects.end();) {
        const EffectDefinition* def = it->get_definition();
        if (def && def->has_tag(tag)) {
            handle_removal(world, target, *it, reason);
            it = comp.effects.erase(it);
            ++count;
        } else {
            ++it;
        }
    }

    return count;
}

int EffectManager::remove_buffs(scene::World& world,
                                 scene::Entity target,
                                 RemovalReason reason) {
    if (!world.valid(target) || !world.has<ActiveEffectsComponent>(target)) {
        return 0;
    }

    auto& comp = world.get<ActiveEffectsComponent>(target);
    int count = 0;

    for (auto it = comp.effects.begin(); it != comp.effects.end();) {
        const EffectDefinition* def = it->get_definition();
        if (def && def->is_buff()) {
            handle_removal(world, target, *it, reason);
            it = comp.effects.erase(it);
            ++count;
        } else {
            ++it;
        }
    }

    return count;
}

int EffectManager::remove_debuffs(scene::World& world,
                                   scene::Entity target,
                                   RemovalReason reason) {
    if (!world.valid(target) || !world.has<ActiveEffectsComponent>(target)) {
        return 0;
    }

    auto& comp = world.get<ActiveEffectsComponent>(target);
    int count = 0;

    for (auto it = comp.effects.begin(); it != comp.effects.end();) {
        const EffectDefinition* def = it->get_definition();
        if (def && def->is_debuff()) {
            handle_removal(world, target, *it, reason);
            it = comp.effects.erase(it);
            ++count;
        } else {
            ++it;
        }
    }

    return count;
}

int EffectManager::remove_all(scene::World& world,
                               scene::Entity target,
                               RemovalReason reason) {
    if (!world.valid(target) || !world.has<ActiveEffectsComponent>(target)) {
        return 0;
    }

    auto& comp = world.get<ActiveEffectsComponent>(target);
    int count = static_cast<int>(comp.effects.size());

    for (auto& effect : comp.effects) {
        handle_removal(world, target, effect, reason);
    }

    comp.effects.clear();
    return count;
}

int EffectManager::dispel(scene::World& world,
                           scene::Entity target,
                           int count,
                           bool debuffs_only) {
    if (!world.valid(target) || !world.has<ActiveEffectsComponent>(target)) {
        return 0;
    }

    auto& comp = world.get<ActiveEffectsComponent>(target);
    comp.sort_by_priority();

    int removed = 0;
    for (auto it = comp.effects.begin(); it != comp.effects.end() && removed < count;) {
        const EffectDefinition* def = it->get_definition();
        if (!def || !def->is_dispellable()) {
            ++it;
            continue;
        }

        if (debuffs_only && !def->is_debuff()) {
            ++it;
            continue;
        }

        handle_removal(world, target, *it, RemovalReason::Dispelled);
        it = comp.effects.erase(it);
        ++removed;
    }

    return removed;
}

int EffectManager::purge(scene::World& world,
                          scene::Entity target,
                          int count) {
    if (!world.valid(target) || !world.has<ActiveEffectsComponent>(target)) {
        return 0;
    }

    auto& comp = world.get<ActiveEffectsComponent>(target);
    comp.sort_by_priority();

    int removed = 0;
    for (auto it = comp.effects.begin(); it != comp.effects.end() && removed < count;) {
        const EffectDefinition* def = it->get_definition();
        if (!def || !has_flag(def->flags, EffectFlags::Purgeable) || !def->is_buff()) {
            ++it;
            continue;
        }

        handle_removal(world, target, *it, RemovalReason::Purged);
        it = comp.effects.erase(it);
        ++removed;
    }

    return removed;
}

// ============================================================================
// Queries
// ============================================================================

bool EffectManager::has_effect(scene::World& world,
                                scene::Entity entity,
                                const std::string& effect_id) const {
    if (!world.valid(entity) || !world.has<ActiveEffectsComponent>(entity)) {
        return false;
    }

    const auto& comp = world.get<ActiveEffectsComponent>(entity);
    return comp.has_effect(effect_id);
}

EffectInstance* EffectManager::get_effect(scene::World& world,
                                           scene::Entity entity,
                                           const std::string& effect_id) {
    if (!world.valid(entity) || !world.has<ActiveEffectsComponent>(entity)) {
        return nullptr;
    }

    auto& comp = world.get<ActiveEffectsComponent>(entity);
    return comp.get_effect(effect_id);
}

std::vector<EffectInstance*> EffectManager::get_all_effects(scene::World& world,
                                                             scene::Entity entity) {
    std::vector<EffectInstance*> result;

    if (!world.valid(entity) || !world.has<ActiveEffectsComponent>(entity)) {
        return result;
    }

    auto& comp = world.get<ActiveEffectsComponent>(entity);
    for (auto& effect : comp.effects) {
        if (effect.is_active()) {
            result.push_back(&effect);
        }
    }

    return result;
}

int EffectManager::count_effects(scene::World& world, scene::Entity entity) const {
    if (!world.valid(entity) || !world.has<ActiveEffectsComponent>(entity)) {
        return 0;
    }
    return world.get<ActiveEffectsComponent>(entity).count();
}

int EffectManager::count_buffs(scene::World& world, scene::Entity entity) const {
    if (!world.valid(entity) || !world.has<ActiveEffectsComponent>(entity)) {
        return 0;
    }
    return world.get<ActiveEffectsComponent>(entity).count_buffs();
}

int EffectManager::count_debuffs(scene::World& world, scene::Entity entity) const {
    if (!world.valid(entity) || !world.has<ActiveEffectsComponent>(entity)) {
        return 0;
    }
    return world.get<ActiveEffectsComponent>(entity).count_debuffs();
}

// ============================================================================
// Callbacks
// ============================================================================

void EffectManager::set_on_apply(EffectCallback callback) {
    m_on_apply = std::move(callback);
}

void EffectManager::set_on_remove(EffectCallback callback) {
    m_on_remove = std::move(callback);
}

void EffectManager::set_on_expire(EffectCallback callback) {
    m_on_expire = std::move(callback);
}

void EffectManager::set_on_tick(TickCallback callback) {
    m_on_tick = std::move(callback);
}

void EffectManager::set_on_stack_change(EffectCallback callback) {
    m_on_stack_change = std::move(callback);
}

// ============================================================================
// Update
// ============================================================================

void EffectManager::update(scene::World& world, float dt) {
    auto view = world.view<ActiveEffectsComponent>();

    for (auto entity : view) {
        auto& comp = view.get<ActiveEffectsComponent>(entity);

        for (auto it = comp.effects.begin(); it != comp.effects.end();) {
            if (!it->update(dt)) {
                // Effect expired
                handle_removal(world, entity, *it, RemovalReason::Expired);
                it = comp.effects.erase(it);
            } else {
                // Process ticks
                while (it->consume_tick()) {
                    process_tick(world, entity, *it);
                }
                ++it;
            }
        }
    }
}

// ============================================================================
// Internal Helpers
// ============================================================================

void EffectManager::apply_stat_modifiers(scene::World& world, scene::Entity entity, EffectInstance& instance) {
    if (!world.has<stats::StatsComponent>(entity)) return;

    const EffectDefinition* def = instance.get_definition();
    if (!def) return;

    auto& stats = world.get<stats::StatsComponent>(entity);

    for (const auto& mod : def->stat_modifiers) {
        stats::StatModifier applied_mod = mod;
        applied_mod.id = core::UUID::generate();
        applied_mod.source_id = "effect:" + instance.definition_id;
        applied_mod.value *= instance.intensity;

        stats.add_modifier(applied_mod);
        instance.applied_modifier_ids.push_back(applied_mod.id);
    }
}

void EffectManager::remove_stat_modifiers(scene::World& world, scene::Entity entity, EffectInstance& instance) {
    if (!world.has<stats::StatsComponent>(entity)) return;

    auto& stats = world.get<stats::StatsComponent>(entity);

    for (const auto& mod_id : instance.applied_modifier_ids) {
        stats.remove_modifier(mod_id);
    }
    instance.applied_modifier_ids.clear();
}

void EffectManager::process_tick(scene::World& world, scene::Entity entity, EffectInstance& instance) {
    const EffectDefinition* def = instance.get_definition();
    if (!def) return;

    float damage = def->damage_per_tick * instance.intensity * instance.damage_multiplier;
    float heal = def->heal_per_tick * instance.intensity * instance.heal_multiplier;

    // Apply damage
    if (damage > 0.0f && world.has<stats::StatsComponent>(entity)) {
        auto& stats = world.get<stats::StatsComponent>(entity);
        float actual_damage = -stats.modify_current(stats::StatType::Health, -damage);

        core::game_events().emit(EffectDamageEvent{
            entity, instance.source, instance.definition_id,
            def->damage_type, actual_damage,
            stats.get_current(stats::StatType::Health),
            stats.is_depleted(stats::StatType::Health)
        });
    }

    // Apply healing
    if (heal > 0.0f && world.has<stats::StatsComponent>(entity)) {
        auto& stats = world.get<stats::StatsComponent>(entity);
        float actual_heal = stats.modify_current(stats::StatType::Health, heal);

        core::game_events().emit(EffectHealEvent{
            entity, instance.source, instance.definition_id,
            actual_heal,
            stats.get_current(stats::StatType::Health),
            stats.get(stats::StatType::MaxHealth)
        });
    }

    // Callback
    if (m_on_tick) {
        m_on_tick(world, entity, instance);
    }
}

void EffectManager::handle_removal(scene::World& world, scene::Entity entity, EffectInstance& instance, RemovalReason reason) {
    remove_stat_modifiers(world, entity, instance);

    instance.state = (reason == RemovalReason::Expired) ? EffectState::Expired : EffectState::Removed;

    // Emit events
    if (reason == RemovalReason::Expired) {
        core::game_events().emit(EffectExpiredEvent{
            entity, instance.definition_id, instance.instance_id,
            instance.stacks, instance.elapsed
        });

        if (m_on_expire) {
            m_on_expire(world, entity, instance);
        }
    }

    core::game_events().emit(EffectRemovedEvent{
        entity, instance.definition_id, instance.instance_id,
        reason, instance.remaining, instance.stacks
    });

    if (m_on_remove) {
        m_on_remove(world, entity, instance);
    }
}

bool EffectManager::check_immunity(const ActiveEffectsComponent& comp, const EffectDefinition& def) const {
    return comp.is_immune_to(def.effect_id) ||
           comp.is_immune_to_category(def.category) ||
           comp.is_immune_to_tags(def.tags);
}

// ============================================================================
// ECS Systems
// ============================================================================

void effect_system(scene::World& world, double dt) {
    effects().update(world, static_cast<float>(dt));
}

void aura_system(scene::World& world, double dt) {
    // TODO: Implement aura detection and application
    (void)world;
    (void)dt;
}

} // namespace engine::effects
