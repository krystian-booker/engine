#include <engine/effects/effect_definition.hpp>
#include <algorithm>

namespace engine::effects {

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
    // TODO: Load from JSON file
    (void)path;
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
