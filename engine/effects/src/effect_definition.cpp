#include <engine/effects/effect_definition.hpp>
#include <engine/data/json_loader.hpp>
#include <engine/stats/stat_definition.hpp>
#include <engine/core/log.hpp>
#include <algorithm>

namespace engine::effects {

// ============================================================================
// JSON Deserialization
// ============================================================================

namespace {

// Parse EffectFlags from JSON array of strings
EffectFlags parse_flags(const nlohmann::json& j) {
    EffectFlags flags = EffectFlags::None;
    if (!j.is_array()) return flags;

    for (const auto& flag : j) {
        if (!flag.is_string()) continue;
        std::string f = flag.get<std::string>();
        if (f == "dispellable") flags = flags | EffectFlags::Dispellable;
        else if (f == "purgeable") flags = flags | EffectFlags::Purgeable;
        else if (f == "hidden") flags = flags | EffectFlags::Hidden;
        else if (f == "persistent") flags = flags | EffectFlags::Persistent;
        else if (f == "unique") flags = flags | EffectFlags::Unique;
        else if (f == "stackable") flags = flags | EffectFlags::Stackable;
        else if (f == "refreshable") flags = flags | EffectFlags::Refreshable;
        else if (f == "no_save") flags = flags | EffectFlags::NoSave;
        else if (f == "inheritable") flags = flags | EffectFlags::Inheritable;
    }
    return flags;
}

// Deserialize a single EffectDefinition from JSON
std::optional<EffectDefinition> deserialize_effect(const nlohmann::json& j, std::string& error) {
    using namespace data::json_helpers;

    // Required: effect_id
    if (!require_string(j, "effect_id", error)) {
        return std::nullopt;
    }

    EffectDefinition def;
    def.effect_id = j["effect_id"].get<std::string>();

    // Basic strings
    def.display_name = get_string(j, "display_name", def.effect_id);
    def.description = get_string(j, "description");
    def.icon_path = get_string(j, "icon_path");

    // Category and flags
    def.category = get_enum<EffectCategory>(j, "category", EffectCategory::Buff);
    if (j.contains("flags") && j["flags"].is_array()) {
        def.flags = parse_flags(j["flags"]);
    } else {
        // Default flags
        def.flags = get_enum<EffectFlags>(j, "flags_raw", EffectFlags::Dispellable | EffectFlags::Stackable);
    }

    // Duration and stacking
    def.base_duration = get_float(j, "base_duration", 10.0f);
    def.max_duration = get_float(j, "max_duration", def.base_duration * 2.0f);
    def.stacking = get_enum<StackBehavior>(j, "stacking", StackBehavior::RefreshExtend);
    def.max_stacks = get_int(j, "max_stacks", 1);

    // Tick behavior
    def.tick_interval = get_float(j, "tick_interval", 0.0f);
    def.tick_on_apply = get_bool(j, "tick_on_apply", false);

    // Damage/healing over time
    def.damage_per_tick = get_float(j, "damage_per_tick", 0.0f);
    def.damage_type = get_string(j, "damage_type", "physical");
    def.heal_per_tick = get_float(j, "heal_per_tick", 0.0f);

    // Priority and scaling
    def.dispel_priority = get_int(j, "dispel_priority", 0);
    def.intensity_per_stack = get_float(j, "intensity_per_stack", 1.0f);
    def.scale_duration_with_stacks = get_bool(j, "scale_duration_with_stacks", false);

    // VFX/SFX
    def.apply_vfx = get_string(j, "apply_vfx");
    def.tick_vfx = get_string(j, "tick_vfx");
    def.expire_vfx = get_string(j, "expire_vfx");
    def.loop_vfx = get_string(j, "loop_vfx");
    def.apply_sfx = get_string(j, "apply_sfx");
    def.tick_sfx = get_string(j, "tick_sfx");
    def.loop_sfx = get_string(j, "loop_sfx");

    // String arrays
    def.tags = get_string_array(j, "tags");
    def.grants_immunity = get_string_array(j, "grants_immunity");
    def.removes_effects = get_string_array(j, "removes_effects");
    def.blocked_by = get_string_array(j, "blocked_by");

    // Stat modifiers: array of {stat: "StatName", type: int, value: float}
    if (j.contains("stat_modifiers") && j["stat_modifiers"].is_array()) {
        auto& stat_reg = stats::stat_registry();
        for (const auto& mod_json : j["stat_modifiers"]) {
            if (!mod_json.is_object()) continue;
            if (!mod_json.contains("stat") || !mod_json.contains("value")) continue;

            stats::StatModifier mod;
            mod.id = core::UUID::generate();
            std::string stat_name = mod_json["stat"].get<std::string>();
            mod.stat = stat_reg.get_type_by_name(stat_name);
            if (mod.stat == stats::StatType::Count) {
                core::log(core::LogLevel::Warn, "[Effects] Unknown stat '{}' in effect '{}'",
                          stat_name, def.effect_id);
                continue;
            }
            mod.type = get_enum<stats::ModifierType>(mod_json, "type", stats::ModifierType::Flat);
            mod.value = mod_json["value"].get<float>();
            mod.source = stats::ModifierSource::Effect;
            mod.source_id = "effect:" + def.effect_id;
            def.stat_modifiers.push_back(mod);
        }
    }

    // Resource per tick: array of {stat: "StatName", value: float}
    if (j.contains("resource_per_tick") && j["resource_per_tick"].is_array()) {
        auto& stat_reg = stats::stat_registry();
        for (const auto& res : j["resource_per_tick"]) {
            if (!res.is_object() || !res.contains("stat") || !res.contains("value")) continue;
            std::string stat_name = res["stat"].get<std::string>();
            stats::StatType stat_type = stat_reg.get_type_by_name(stat_name);
            if (stat_type != stats::StatType::Count) {
                def.resource_per_tick.emplace_back(stat_type, res["value"].get<float>());
            }
        }
    }

    return def;
}

} // anonymous namespace

// ============================================================================
// EffectDefinition Implementation
// ============================================================================

bool EffectDefinition::has_tag(const std::string& tag) const {
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

// ============================================================================
// EffectRegistry Implementation
// ============================================================================

EffectRegistry& EffectRegistry::instance() {
    static EffectRegistry s_instance;
    return s_instance;
}

void EffectRegistry::register_effect(const EffectDefinition& def) {
    m_effects[def.effect_id] = def;
}

void EffectRegistry::load_effects(const std::string& path) {
    core::log(core::LogLevel::Info, "[Effects] Loading effects from: {}", path);

    auto result = data::load_json_array<EffectDefinition>(path, deserialize_effect, "effects");

    // Log warnings
    for (const auto& warn : result.warnings) {
        core::log(core::LogLevel::Warn, "[Effects] {}", warn);
    }

    // Log errors
    for (const auto& err : result.errors) {
        core::log(core::LogLevel::Error, "[Effects] {}", err);
    }

    // Register successfully loaded effects
    for (const auto& effect : result.items) {
        register_effect(effect);
    }

    core::log(core::LogLevel::Info, "[Effects] Loaded {} effects ({} errors)",
              result.loaded_count(), result.error_count());
}

const EffectDefinition* EffectRegistry::get(const std::string& effect_id) const {
    auto it = m_effects.find(effect_id);
    return it != m_effects.end() ? &it->second : nullptr;
}

bool EffectRegistry::exists(const std::string& effect_id) const {
    return m_effects.find(effect_id) != m_effects.end();
}

std::vector<std::string> EffectRegistry::get_all_effect_ids() const {
    std::vector<std::string> result;
    result.reserve(m_effects.size());
    for (const auto& [id, def] : m_effects) {
        result.push_back(id);
    }
    return result;
}

std::vector<std::string> EffectRegistry::get_effects_by_category(EffectCategory category) const {
    std::vector<std::string> result;
    for (const auto& [id, def] : m_effects) {
        if (def.category == category) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<std::string> EffectRegistry::get_effects_by_tag(const std::string& tag) const {
    std::vector<std::string> result;
    for (const auto& [id, def] : m_effects) {
        if (def.has_tag(tag)) {
            result.push_back(id);
        }
    }
    return result;
}

void EffectRegistry::clear() {
    m_effects.clear();
}

// ============================================================================
// EffectBuilder Implementation
// ============================================================================

EffectBuilder& EffectBuilder::id(const std::string& effect_id) {
    m_def.effect_id = effect_id;
    return *this;
}

EffectBuilder& EffectBuilder::name(const std::string& display_name) {
    m_def.display_name = display_name;
    return *this;
}

EffectBuilder& EffectBuilder::description(const std::string& desc) {
    m_def.description = desc;
    return *this;
}

EffectBuilder& EffectBuilder::icon(const std::string& path) {
    m_def.icon_path = path;
    return *this;
}

EffectBuilder& EffectBuilder::category(EffectCategory cat) {
    m_def.category = cat;
    return *this;
}

EffectBuilder& EffectBuilder::buff() {
    m_def.category = EffectCategory::Buff;
    return *this;
}

EffectBuilder& EffectBuilder::debuff() {
    m_def.category = EffectCategory::Debuff;
    return *this;
}

EffectBuilder& EffectBuilder::duration(float seconds) {
    m_def.base_duration = seconds;
    m_def.max_duration = seconds * 2.0f;  // Default max is 2x base
    return *this;
}

EffectBuilder& EffectBuilder::permanent() {
    m_def.base_duration = 0.0f;
    return *this;
}

EffectBuilder& EffectBuilder::stacking(StackBehavior behavior, int max_stacks) {
    m_def.stacking = behavior;
    m_def.max_stacks = max_stacks;
    return *this;
}

EffectBuilder& EffectBuilder::tick(float interval) {
    m_def.tick_interval = interval;
    return *this;
}

EffectBuilder& EffectBuilder::damage_per_tick(float amount, const std::string& type) {
    m_def.damage_per_tick = amount;
    m_def.damage_type = type;
    return *this;
}

EffectBuilder& EffectBuilder::heal_per_tick(float amount) {
    m_def.heal_per_tick = amount;
    return *this;
}

EffectBuilder& EffectBuilder::stat_modifier(stats::StatType stat, stats::ModifierType type, float value) {
    stats::StatModifier mod;
    mod.id = core::UUID::generate();
    mod.stat = stat;
    mod.type = type;
    mod.value = value;
    mod.source = stats::ModifierSource::Effect;
    mod.source_id = "effect:" + m_def.effect_id;
    m_def.stat_modifiers.push_back(mod);
    return *this;
}

EffectBuilder& EffectBuilder::grants_immunity(const std::string& effect_id) {
    m_def.grants_immunity.push_back(effect_id);
    return *this;
}

EffectBuilder& EffectBuilder::removes(const std::string& effect_id) {
    m_def.removes_effects.push_back(effect_id);
    return *this;
}

EffectBuilder& EffectBuilder::blocked_by(const std::string& effect_id) {
    m_def.blocked_by.push_back(effect_id);
    return *this;
}

EffectBuilder& EffectBuilder::tag(const std::string& t) {
    m_def.tags.push_back(t);
    return *this;
}

EffectBuilder& EffectBuilder::dispellable(bool value) {
    if (value) {
        m_def.flags = m_def.flags | EffectFlags::Dispellable;
    } else {
        m_def.flags = static_cast<EffectFlags>(
            static_cast<uint32_t>(m_def.flags) & ~static_cast<uint32_t>(EffectFlags::Dispellable)
        );
    }
    return *this;
}

EffectBuilder& EffectBuilder::hidden(bool value) {
    if (value) {
        m_def.flags = m_def.flags | EffectFlags::Hidden;
    } else {
        m_def.flags = static_cast<EffectFlags>(
            static_cast<uint32_t>(m_def.flags) & ~static_cast<uint32_t>(EffectFlags::Hidden)
        );
    }
    return *this;
}

EffectBuilder& EffectBuilder::vfx(const std::string& apply, const std::string& loop) {
    m_def.apply_vfx = apply;
    m_def.loop_vfx = loop;
    return *this;
}

EffectBuilder& EffectBuilder::sfx(const std::string& apply, const std::string& loop) {
    m_def.apply_sfx = apply;
    m_def.loop_sfx = loop;
    return *this;
}

EffectDefinition EffectBuilder::build() const {
    return m_def;
}

void EffectBuilder::register_effect() const {
    effect_registry().register_effect(m_def);
}

} // namespace engine::effects
