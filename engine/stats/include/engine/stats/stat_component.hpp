#pragma once

#include <engine/stats/stat_definition.hpp>
#include <engine/stats/stat_modifier.hpp>
#include <engine/scene/entity.hpp>
#include <unordered_map>
#include <vector>
#include <string>
#include <functional>

namespace engine::stats {

// ============================================================================
// StatsComponent - ECS component holding all stats for an entity
// ============================================================================

struct StatsComponent {
    // Base values (before modifiers)
    std::unordered_map<StatType, float> base_values;

    // Cached final values (after modifiers)
    std::unordered_map<StatType, float> final_values;

    // Current resource values (Health, Stamina, Mana - can be less than max)
    std::unordered_map<StatType, float> current_resources;

    // Active modifiers
    std::unordered_map<StatType, std::vector<StatModifier>> modifiers;

    // Dirty flag for recalculation
    bool needs_recalculation = true;

    // ========================================================================
    // Base Value Access
    // ========================================================================

    // Get base value (before modifiers)
    float get_base(StatType stat) const;

    // Set base value
    void set_base(StatType stat, float value);

    // Add to base value
    void add_base(StatType stat, float delta);

    // ========================================================================
    // Final Value Access (after modifiers)
    // ========================================================================

    // Get final calculated value
    float get(StatType stat) const;

    // Get value as integer
    int get_int(StatType stat) const;

    // Check if stat exists
    bool has(StatType stat) const;

    // ========================================================================
    // Resource Management (Health, Stamina, Mana)
    // ========================================================================

    // Get current resource value
    float get_current(StatType resource) const;

    // Set current resource value (clamped to 0..max)
    void set_current(StatType resource, float value);

    // Modify current resource (positive = heal/restore, negative = damage/consume)
    // Returns actual amount changed
    float modify_current(StatType resource, float delta);

    // Get percentage of current vs max (0.0 to 1.0)
    float get_percent(StatType resource) const;

    // Set percentage of max
    void set_percent(StatType resource, float percent);

    // Check if resource is depleted
    bool is_depleted(StatType resource) const;

    // Check if resource is full
    bool is_full(StatType resource) const;

    // Restore resource to max
    void fill(StatType resource);

    // Deplete resource to zero
    void deplete(StatType resource);

    // ========================================================================
    // Modifier Management
    // ========================================================================

    // Add a modifier
    void add_modifier(const StatModifier& mod);

    // Remove modifier by ID
    bool remove_modifier(const core::UUID& id);

    // Remove all modifiers from a source
    int remove_modifiers_by_source(const std::string& source_id);

    // Remove all modifiers of a specific type
    int remove_modifiers_by_type(ModifierSource source);

    // Remove all modifiers for a stat
    void clear_modifiers(StatType stat);

    // Clear all modifiers
    void clear_all_modifiers();

    // Get all modifiers for a stat
    const std::vector<StatModifier>& get_modifiers(StatType stat) const;

    // Get all modifiers from a source
    std::vector<StatModifier> get_modifiers_by_source(const std::string& source_id) const;

    // Check if has modifier from source
    bool has_modifier_from(const std::string& source_id) const;

    // ========================================================================
    // Calculation
    // ========================================================================

    // Force recalculation of all final values
    void recalculate();

    // Recalculate a single stat
    void recalculate_stat(StatType stat);

    // Mark as needing recalculation
    void mark_dirty() { needs_recalculation = true; }

    // ========================================================================
    // Initialization
    // ========================================================================

    // Initialize with default values from stat registry
    void initialize_defaults();

    // Initialize from a preset/template
    void initialize_from_preset(const std::string& preset_name);

    // Copy base values from another component
    void copy_base_from(const StatsComponent& other);

private:
    static std::vector<StatModifier> s_empty_modifiers;
};

// ============================================================================
// StatPreset - Template for initializing stats
// ============================================================================

struct StatPreset {
    std::string preset_id;
    std::string display_name;
    std::unordered_map<StatType, float> base_values;
    std::vector<StatModifier> default_modifiers;
};

// ============================================================================
// StatPresetRegistry - Registry of stat presets
// ============================================================================

class StatPresetRegistry {
public:
    static StatPresetRegistry& instance();

    // Delete copy/move
    StatPresetRegistry(const StatPresetRegistry&) = delete;
    StatPresetRegistry& operator=(const StatPresetRegistry&) = delete;

    // Registration
    void register_preset(const StatPreset& preset);
    void load_presets(const std::string& path);

    // Lookup
    const StatPreset* get_preset(const std::string& id) const;
    std::vector<std::string> get_all_preset_ids() const;

private:
    StatPresetRegistry() = default;
    ~StatPresetRegistry() = default;

    std::unordered_map<std::string, StatPreset> m_presets;
};

inline StatPresetRegistry& stat_presets() { return StatPresetRegistry::instance(); }

// ============================================================================
// Component Registration
// ============================================================================

void register_stats_components();

} // namespace engine::stats
