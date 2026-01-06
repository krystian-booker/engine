#pragma once

#include <engine/effects/effect_instance.hpp>
#include <engine/effects/effect_component.hpp>
#include <engine/scene/entity.hpp>
#include <engine/scene/world.hpp>
#include <functional>
#include <string>
#include <vector>

namespace engine::effects {

// ============================================================================
// Effect Manager - Central management of all status effects
// ============================================================================

class EffectManager {
public:
    static EffectManager& instance();

    // Delete copy/move
    EffectManager(const EffectManager&) = delete;
    EffectManager& operator=(const EffectManager&) = delete;

    // ========================================================================
    // Application
    // ========================================================================

    // Apply an effect to a target
    ApplyResultInfo apply(scene::World& world,
                          scene::Entity target,
                          const std::string& effect_id,
                          scene::Entity source = scene::NullEntity);

    // Apply with custom parameters
    ApplyResultInfo apply(scene::World& world,
                          scene::Entity target,
                          const std::string& effect_id,
                          scene::Entity source,
                          float duration_override,
                          int stack_override = 1);

    // Apply effect instance directly
    ApplyResultInfo apply(scene::World& world,
                          scene::Entity target,
                          EffectInstance instance);

    // Try to apply (respects chance-based application)
    ApplyResultInfo try_apply(scene::World& world,
                              scene::Entity target,
                              const std::string& effect_id,
                              float chance,
                              scene::Entity source = scene::NullEntity);

    // ========================================================================
    // Removal
    // ========================================================================

    // Remove specific effect by ID
    bool remove(scene::World& world,
                scene::Entity target,
                const std::string& effect_id,
                RemovalReason reason = RemovalReason::Cancelled);

    // Remove by instance ID
    bool remove_by_instance_id(scene::World& world,
                               scene::Entity target,
                               const core::UUID& instance_id,
                               RemovalReason reason = RemovalReason::Cancelled);

    // Remove all effects from source
    int remove_from_source(scene::World& world,
                           scene::Entity target,
                           scene::Entity source,
                           RemovalReason reason = RemovalReason::SourceDeath);

    // Remove all effects with tag
    int remove_with_tag(scene::World& world,
                        scene::Entity target,
                        const std::string& tag,
                        RemovalReason reason = RemovalReason::Cancelled);

    // Remove all buffs
    int remove_buffs(scene::World& world,
                     scene::Entity target,
                     RemovalReason reason = RemovalReason::Purged);

    // Remove all debuffs
    int remove_debuffs(scene::World& world,
                       scene::Entity target,
                       RemovalReason reason = RemovalReason::Dispelled);

    // Remove all effects
    int remove_all(scene::World& world,
                   scene::Entity target,
                   RemovalReason reason = RemovalReason::Cancelled);

    // Dispel (removes dispellable effects by priority)
    int dispel(scene::World& world,
               scene::Entity target,
               int count = 1,
               bool debuffs_only = true);

    // Purge (removes purgeable buffs)
    int purge(scene::World& world,
              scene::Entity target,
              int count = 1);

    // ========================================================================
    // Stack Management
    // ========================================================================

    // Add stacks to an existing effect
    bool add_stacks(scene::World& world,
                    scene::Entity target,
                    const std::string& effect_id,
                    int count = 1);

    // Remove stacks from an effect
    bool remove_stacks(scene::World& world,
                       scene::Entity target,
                       const std::string& effect_id,
                       int count = 1);

    // Set stack count directly
    bool set_stacks(scene::World& world,
                    scene::Entity target,
                    const std::string& effect_id,
                    int count);

    // ========================================================================
    // Duration Management
    // ========================================================================

    // Refresh duration to max
    bool refresh(scene::World& world,
                 scene::Entity target,
                 const std::string& effect_id);

    // Extend duration by amount
    bool extend(scene::World& world,
                scene::Entity target,
                const std::string& effect_id,
                float amount);

    // Reduce duration
    bool reduce_duration(scene::World& world,
                         scene::Entity target,
                         const std::string& effect_id,
                         float amount);

    // ========================================================================
    // Queries
    // ========================================================================

    // Check if entity has effect
    bool has_effect(scene::World& world,
                    scene::Entity entity,
                    const std::string& effect_id) const;

    // Get effect instance
    EffectInstance* get_effect(scene::World& world,
                               scene::Entity entity,
                               const std::string& effect_id);

    // Get all effects on entity
    std::vector<EffectInstance*> get_all_effects(scene::World& world,
                                                  scene::Entity entity);

    // Count effects
    int count_effects(scene::World& world, scene::Entity entity) const;
    int count_buffs(scene::World& world, scene::Entity entity) const;
    int count_debuffs(scene::World& world, scene::Entity entity) const;

    // ========================================================================
    // Immunity
    // ========================================================================

    // Grant immunity to effect
    void grant_immunity(scene::World& world,
                        scene::Entity entity,
                        const std::string& effect_id);

    // Revoke immunity
    void revoke_immunity(scene::World& world,
                         scene::Entity entity,
                         const std::string& effect_id);

    // Check immunity
    bool is_immune(scene::World& world,
                   scene::Entity entity,
                   const std::string& effect_id) const;

    // ========================================================================
    // Callbacks
    // ========================================================================

    using EffectCallback = std::function<void(scene::World&, scene::Entity, const EffectInstance&)>;
    using TickCallback = std::function<void(scene::World&, scene::Entity, EffectInstance&)>;

    void set_on_apply(EffectCallback callback);
    void set_on_remove(EffectCallback callback);
    void set_on_expire(EffectCallback callback);
    void set_on_tick(TickCallback callback);
    void set_on_stack_change(EffectCallback callback);

    // ========================================================================
    // Update
    // ========================================================================

    // Update all effects (call once per frame)
    void update(scene::World& world, float dt);

private:
    EffectManager() = default;
    ~EffectManager() = default;

    // Internal helpers
    void apply_stat_modifiers(scene::World& world, scene::Entity entity, EffectInstance& instance);
    void remove_stat_modifiers(scene::World& world, scene::Entity entity, EffectInstance& instance);
    void process_tick(scene::World& world, scene::Entity entity, EffectInstance& instance);
    void handle_removal(scene::World& world, scene::Entity entity, EffectInstance& instance, RemovalReason reason);
    bool check_immunity(const ActiveEffectsComponent& comp, const EffectDefinition& def) const;

    // Callbacks
    EffectCallback m_on_apply;
    EffectCallback m_on_remove;
    EffectCallback m_on_expire;
    TickCallback m_on_tick;
    EffectCallback m_on_stack_change;
};

// ============================================================================
// Global Access
// ============================================================================

inline EffectManager& effects() { return EffectManager::instance(); }

// ============================================================================
// ECS System
// ============================================================================

// Update all effects (call in Update phase)
void effect_system(scene::World& world, double dt);

// Update auras (call after effect_system)
void aura_system(scene::World& world, double dt);

} // namespace engine::effects
