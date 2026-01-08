#include <engine/stats/stat_component.hpp>
#include <engine/stats/stat_calculator.hpp>
#include <engine/data/json_loader.hpp>
#include <engine/core/log.hpp>
#include <algorithm>
#include <cmath>

namespace engine::stats {

// ============================================================================
// JSON Deserialization
// ============================================================================

namespace {

// Deserialize a single StatPreset from JSON
std::optional<StatPreset> deserialize_preset(const nlohmann::json& j, std::string& error) {
    using namespace data::json_helpers;

    // Required: preset_id
    if (!require_string(j, "preset_id", error)) {
        return std::nullopt;
    }

    StatPreset preset;
    preset.preset_id = j["preset_id"].get<std::string>();
    preset.display_name = get_string(j, "display_name", preset.preset_id);

    auto& stat_reg = stat_registry();

    // Base values: array of {stat: "StatName", value: float}
    if (j.contains("base_values") && j["base_values"].is_array()) {
        for (const auto& val : j["base_values"]) {
            if (!val.is_object() || !val.contains("stat") || !val.contains("value")) continue;

            std::string stat_name = val["stat"].get<std::string>();
            StatType stat_type = stat_reg.get_type_by_name(stat_name);
            if (stat_type != StatType::Count) {
                preset.base_values[stat_type] = val["value"].get<float>();
            } else {
                core::log(core::LogLevel::Warn, "[Stats] Unknown stat '{}' in preset '{}'",
                          stat_name, preset.preset_id);
            }
        }
    }

    // Default modifiers: array of {stat: "StatName", type: "flat"|"percent"|"multiplicative", value: float}
    if (j.contains("default_modifiers") && j["default_modifiers"].is_array()) {
        for (const auto& mod_json : j["default_modifiers"]) {
            if (!mod_json.is_object()) continue;
            if (!mod_json.contains("stat") || !mod_json.contains("value")) continue;

            StatModifier mod;
            mod.id = core::UUID::generate();
            std::string stat_name = mod_json["stat"].get<std::string>();
            mod.stat = stat_reg.get_type_by_name(stat_name);
            if (mod.stat == StatType::Count) {
                core::log(core::LogLevel::Warn, "[Stats] Unknown stat '{}' in preset '{}' modifier",
                          stat_name, preset.preset_id);
                continue;
            }
            mod.type = get_enum<ModifierType>(mod_json, "type", ModifierType::Flat);
            mod.value = mod_json["value"].get<float>();
            mod.source = ModifierSource::Base;
            mod.source_id = "preset:" + preset.preset_id;
            preset.default_modifiers.push_back(mod);
        }
    }

    return preset;
}

} // anonymous namespace

// Static empty vector for returning when no modifiers exist
std::vector<StatModifier> StatsComponent::s_empty_modifiers;

// ============================================================================
// Base Value Access
// ============================================================================

float StatsComponent::get_base(StatType stat) const {
    auto it = base_values.find(stat);
    if (it != base_values.end()) {
        return it->second;
    }

    // Return default from registry
    const StatDefinition* def = stat_registry().get_definition(stat);
    return def ? def->default_value : 0.0f;
}

void StatsComponent::set_base(StatType stat, float value) {
    const StatDefinition* def = stat_registry().get_definition(stat);
    if (def) {
        value = std::clamp(value, def->min_value, def->max_value);
    }

    base_values[stat] = value;
    needs_recalculation = true;
}

void StatsComponent::add_base(StatType stat, float delta) {
    set_base(stat, get_base(stat) + delta);
}

// ============================================================================
// Final Value Access
// ============================================================================

float StatsComponent::get(StatType stat) const {
    // Ensure recalculation if needed
    if (needs_recalculation) {
        const_cast<StatsComponent*>(this)->recalculate();
    }

    auto it = final_values.find(stat);
    if (it != final_values.end()) {
        return it->second;
    }

    // Not calculated yet, return base
    return get_base(stat);
}

int StatsComponent::get_int(StatType stat) const {
    return static_cast<int>(std::round(get(stat)));
}

bool StatsComponent::has(StatType stat) const {
    return base_values.find(stat) != base_values.end() ||
           final_values.find(stat) != final_values.end();
}

// ============================================================================
// Resource Management
// ============================================================================

float StatsComponent::get_current(StatType resource) const {
    auto it = current_resources.find(resource);
    if (it != current_resources.end()) {
        return it->second;
    }

    // If not set, return max (full)
    StatType max_stat = get_max_stat(resource);
    if (max_stat != StatType::Count) {
        return get(max_stat);
    }

    return get(resource);
}

void StatsComponent::set_current(StatType resource, float value) {
    StatType max_stat = get_max_stat(resource);
    float max_value = (max_stat != StatType::Count) ? get(max_stat) : 999999.0f;

    current_resources[resource] = std::clamp(value, 0.0f, max_value);
}

float StatsComponent::modify_current(StatType resource, float delta) {
    float old_value = get_current(resource);
    float new_value = old_value + delta;

    set_current(resource, new_value);

    return get_current(resource) - old_value;
}

float StatsComponent::get_percent(StatType resource) const {
    StatType max_stat = get_max_stat(resource);
    if (max_stat == StatType::Count) {
        return 1.0f;
    }

    float max_value = get(max_stat);
    if (max_value <= 0.0f) {
        return 0.0f;
    }

    return get_current(resource) / max_value;
}

void StatsComponent::set_percent(StatType resource, float percent) {
    StatType max_stat = get_max_stat(resource);
    if (max_stat == StatType::Count) return;

    float max_value = get(max_stat);
    set_current(resource, max_value * std::clamp(percent, 0.0f, 1.0f));
}

bool StatsComponent::is_depleted(StatType resource) const {
    return get_current(resource) <= 0.0f;
}

bool StatsComponent::is_full(StatType resource) const {
    StatType max_stat = get_max_stat(resource);
    if (max_stat == StatType::Count) return true;

    return get_current(resource) >= get(max_stat);
}

void StatsComponent::fill(StatType resource) {
    set_percent(resource, 1.0f);
}

void StatsComponent::deplete(StatType resource) {
    set_current(resource, 0.0f);
}

// ============================================================================
// Modifier Management
// ============================================================================

void StatsComponent::add_modifier(const StatModifier& mod) {
    modifiers[mod.stat].push_back(mod);

    // Sort by priority
    auto& mods = modifiers[mod.stat];
    std::sort(mods.begin(), mods.end(), [](const StatModifier& a, const StatModifier& b) {
        return a.priority < b.priority;
    });

    needs_recalculation = true;
}

bool StatsComponent::remove_modifier(const core::UUID& id) {
    for (auto& [stat, mods] : modifiers) {
        auto it = std::find_if(mods.begin(), mods.end(),
                               [&id](const StatModifier& m) { return m.id == id; });
        if (it != mods.end()) {
            mods.erase(it);
            needs_recalculation = true;
            return true;
        }
    }
    return false;
}

int StatsComponent::remove_modifiers_by_source(const std::string& source_id) {
    int count = 0;
    for (auto& [stat, mods] : modifiers) {
        auto new_end = std::remove_if(mods.begin(), mods.end(),
                                       [&source_id](const StatModifier& m) {
                                           return m.source_id == source_id;
                                       });
        count += static_cast<int>(std::distance(new_end, mods.end()));
        mods.erase(new_end, mods.end());
    }

    if (count > 0) {
        needs_recalculation = true;
    }
    return count;
}

int StatsComponent::remove_modifiers_by_type(ModifierSource source) {
    int count = 0;
    for (auto& [stat, mods] : modifiers) {
        auto new_end = std::remove_if(mods.begin(), mods.end(),
                                       [source](const StatModifier& m) {
                                           return m.source == source;
                                       });
        count += static_cast<int>(std::distance(new_end, mods.end()));
        mods.erase(new_end, mods.end());
    }

    if (count > 0) {
        needs_recalculation = true;
    }
    return count;
}

void StatsComponent::clear_modifiers(StatType stat) {
    auto it = modifiers.find(stat);
    if (it != modifiers.end()) {
        it->second.clear();
        needs_recalculation = true;
    }
}

void StatsComponent::clear_all_modifiers() {
    modifiers.clear();
    needs_recalculation = true;
}

const std::vector<StatModifier>& StatsComponent::get_modifiers(StatType stat) const {
    auto it = modifiers.find(stat);
    return it != modifiers.end() ? it->second : s_empty_modifiers;
}

std::vector<StatModifier> StatsComponent::get_modifiers_by_source(const std::string& source_id) const {
    std::vector<StatModifier> result;
    for (const auto& [stat, mods] : modifiers) {
        for (const auto& mod : mods) {
            if (mod.source_id == source_id) {
                result.push_back(mod);
            }
        }
    }
    return result;
}

bool StatsComponent::has_modifier_from(const std::string& source_id) const {
    for (const auto& [stat, mods] : modifiers) {
        for (const auto& mod : mods) {
            if (mod.source_id == source_id) {
                return true;
            }
        }
    }
    return false;
}

// ============================================================================
// Calculation
// ============================================================================

void StatsComponent::recalculate() {
    // Store old values for event comparison
    std::unordered_map<StatType, float> old_final = final_values;

    // Clear and recalculate all
    final_values.clear();

    // Collect all stats that need calculation
    std::set<StatType> all_stats;
    for (const auto& [stat, value] : base_values) {
        all_stats.insert(stat);
    }
    for (const auto& [stat, mods] : modifiers) {
        all_stats.insert(stat);
    }

    // Calculate each stat
    for (StatType stat : all_stats) {
        recalculate_stat(stat);
    }

    // Clamp resources to their new max values
    for (auto& [resource, current] : current_resources) {
        StatType max_stat = get_max_stat(resource);
        if (max_stat != StatType::Count) {
            float max_value = get(max_stat);
            current = std::min(current, max_value);
        }
    }

    needs_recalculation = false;
}

void StatsComponent::recalculate_stat(StatType stat) {
    float base = get_base(stat);

    // Build modifier stack
    ModifierStack stack;
    auto it = modifiers.find(stat);
    if (it != modifiers.end()) {
        for (const auto& mod : it->second) {
            stack.add(mod);
        }
    }

    // Calculate final value
    float final_value = calculate_stat_value(base, stack);

    // Clamp to definition limits
    const StatDefinition* def = stat_registry().get_definition(stat);
    if (def) {
        final_value = std::clamp(final_value, def->min_value, def->max_value);
    }

    final_values[stat] = final_value;
}

// ============================================================================
// Initialization
// ============================================================================

void StatsComponent::initialize_defaults() {
    for (StatType stat : stat_registry().get_all_registered_stats()) {
        const StatDefinition* def = stat_registry().get_definition(stat);
        if (def) {
            base_values[stat] = def->default_value;
        }
    }

    // Initialize resources to max
    current_resources[StatType::Health] = get(StatType::MaxHealth);
    current_resources[StatType::Stamina] = get(StatType::MaxStamina);
    current_resources[StatType::Mana] = get(StatType::MaxMana);
    current_resources[StatType::Poise] = get(StatType::Poise);

    needs_recalculation = true;
}

void StatsComponent::initialize_from_preset(const std::string& preset_name) {
    const StatPreset* preset = stat_presets().get_preset(preset_name);
    if (!preset) {
        initialize_defaults();
        return;
    }

    // Set base values from preset
    base_values = preset->base_values;

    // Apply default modifiers
    for (const auto& mod : preset->default_modifiers) {
        add_modifier(mod);
    }

    // Initialize resources
    recalculate();
    current_resources[StatType::Health] = get(StatType::MaxHealth);
    current_resources[StatType::Stamina] = get(StatType::MaxStamina);
    current_resources[StatType::Mana] = get(StatType::MaxMana);
    current_resources[StatType::Poise] = get(StatType::Poise);
}

void StatsComponent::copy_base_from(const StatsComponent& other) {
    base_values = other.base_values;
    needs_recalculation = true;
}

// ============================================================================
// StatPresetRegistry Implementation
// ============================================================================

StatPresetRegistry& StatPresetRegistry::instance() {
    static StatPresetRegistry s_instance;
    return s_instance;
}

void StatPresetRegistry::register_preset(const StatPreset& preset) {
    m_presets[preset.preset_id] = preset;
}

void StatPresetRegistry::load_presets(const std::string& path) {
    core::log(core::LogLevel::Info, "[Stats] Loading presets from: {}", path);

    auto result = data::load_json_array<StatPreset>(path, deserialize_preset, "presets");

    // Log warnings
    for (const auto& warn : result.warnings) {
        core::log(core::LogLevel::Warn, "[Stats] {}", warn);
    }

    // Log errors
    for (const auto& err : result.errors) {
        core::log(core::LogLevel::Error, "[Stats] {}", err);
    }

    // Register successfully loaded presets
    for (const auto& preset : result.items) {
        register_preset(preset);
    }

    core::log(core::LogLevel::Info, "[Stats] Loaded {} presets ({} errors)",
              result.loaded_count(), result.error_count());
}

const StatPreset* StatPresetRegistry::get_preset(const std::string& id) const {
    auto it = m_presets.find(id);
    return it != m_presets.end() ? &it->second : nullptr;
}

std::vector<std::string> StatPresetRegistry::get_all_preset_ids() const {
    std::vector<std::string> result;
    result.reserve(m_presets.size());
    for (const auto& [id, preset] : m_presets) {
        result.push_back(id);
    }
    return result;
}

} // namespace engine::stats
