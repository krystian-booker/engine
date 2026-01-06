#pragma once

#include <engine/effects/effect_instance.hpp>
#include <vector>
#include <unordered_set>
#include <functional>

namespace engine::effects {

// ============================================================================
// ActiveEffectsComponent - ECS component for entities with status effects
// ============================================================================

struct ActiveEffectsComponent {
    // Active effect instances
    std::vector<EffectInstance> effects;

    // Immunity list (effect IDs that cannot be applied)
    std::unordered_set<std::string> immunities;

    // Immunity to categories
    std::unordered_set<EffectCategory> category_immunities;

    // Immunity to tags
    std::unordered_set<std::string> tag_immunities;

    // Maximum number of effects (0 = unlimited)
    int max_effects = 0;

    // ========================================================================
    // Queries
    // ========================================================================

    // Check if entity has a specific effect
    bool has_effect(const std::string& effect_id) const;

    // Check if entity has any effect with tag
    bool has_effect_with_tag(const std::string& tag) const;

    // Check if entity has any buff
    bool has_buff() const;

    // Check if entity has any debuff
    bool has_debuff() const;

    // Get instance of a specific effect (nullptr if not found)
    EffectInstance* get_effect(const std::string& effect_id);
    const EffectInstance* get_effect(const std::string& effect_id) const;

    // Get instance by UUID
    EffectInstance* get_effect_by_id(const core::UUID& instance_id);

    // Get all effects of a category
    std::vector<EffectInstance*> get_effects_by_category(EffectCategory category);

    // Get all effects with a tag
    std::vector<EffectInstance*> get_effects_with_tag(const std::string& tag);

    // Get all active buffs
    std::vector<EffectInstance*> get_buffs();

    // Get all active debuffs
    std::vector<EffectInstance*> get_debuffs();

    // Get stack count of an effect
    int get_stack_count(const std::string& effect_id) const;

    // Get remaining duration of an effect
    float get_remaining_duration(const std::string& effect_id) const;

    // Count active effects
    int count() const { return static_cast<int>(effects.size()); }
    int count_buffs() const;
    int count_debuffs() const;

    // ========================================================================
    // Immunity
    // ========================================================================

    // Check if immune to a specific effect
    bool is_immune_to(const std::string& effect_id) const;

    // Check if immune to a category
    bool is_immune_to_category(EffectCategory category) const;

    // Check if immune due to tags
    bool is_immune_to_tags(const std::vector<std::string>& tags) const;

    // Add immunity
    void add_immunity(const std::string& effect_id);
    void add_category_immunity(EffectCategory category);
    void add_tag_immunity(const std::string& tag);

    // Remove immunity
    void remove_immunity(const std::string& effect_id);
    void remove_category_immunity(EffectCategory category);
    void remove_tag_immunity(const std::string& tag);

    // Clear all immunities
    void clear_immunities();

    // ========================================================================
    // Utility
    // ========================================================================

    // Get total stat modifier from all active effects for a stat
    float get_total_modifier(stats::StatType stat, stats::ModifierType type) const;

    // Remove expired effects from list
    void cleanup_expired();

    // Sort effects by priority (for display)
    void sort_by_priority();

    // Sort effects by remaining time
    void sort_by_remaining();
};

// ============================================================================
// EffectSourceComponent - For entities that can apply effects
// ============================================================================

struct EffectSourceComponent {
    // Modifiers applied to effects this entity creates
    float duration_multiplier = 1.0f;
    float damage_multiplier = 1.0f;
    float heal_multiplier = 1.0f;
    int bonus_stacks = 0;

    // Effects that this source always applies (auras)
    std::vector<std::string> passive_effects;

    // Chance modifiers for effect application
    std::unordered_map<std::string, float> apply_chance_modifiers;
};

// ============================================================================
// EffectAuraComponent - For area-based continuous effects
// ============================================================================

struct EffectAuraComponent {
    std::string effect_id;              // Effect to apply
    float radius = 5.0f;                // Aura radius
    float apply_interval = 1.0f;        // How often to reapply/refresh
    float time_since_apply = 0.0f;

    bool affects_self = false;          // Apply to self?
    bool affects_allies = true;         // Apply to allies?
    bool affects_enemies = true;        // Apply to enemies?

    std::string faction;                // For friend/foe detection

    // Maximum targets (0 = unlimited)
    int max_targets = 0;

    // Currently affected entities
    std::vector<scene::Entity> affected_entities;
};

// ============================================================================
// Component Registration
// ============================================================================

void register_effects_components();

} // namespace engine::effects
