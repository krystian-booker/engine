#include <engine/stats/stat_component.hpp>
#include <engine/stats/stat_modifier.hpp>
#include <engine/reflect/type_registry.hpp>

namespace engine::stats {

void register_stats_components() {
    auto& registry = reflect::TypeRegistry::instance();

    // Register StatType enum
    registry.register_enum<StatType>("StatType")
        .value("Health", StatType::Health)
        .value("MaxHealth", StatType::MaxHealth)
        .value("HealthRegen", StatType::HealthRegen)
        .value("Stamina", StatType::Stamina)
        .value("MaxStamina", StatType::MaxStamina)
        .value("StaminaRegen", StatType::StaminaRegen)
        .value("Mana", StatType::Mana)
        .value("MaxMana", StatType::MaxMana)
        .value("ManaRegen", StatType::ManaRegen)
        .value("Strength", StatType::Strength)
        .value("Dexterity", StatType::Dexterity)
        .value("Intelligence", StatType::Intelligence)
        .value("Vitality", StatType::Vitality)
        .value("Luck", StatType::Luck)
        .value("Endurance", StatType::Endurance)
        .value("Agility", StatType::Agility)
        .value("Wisdom", StatType::Wisdom)
        .value("Charisma", StatType::Charisma)
        .value("PhysicalDamage", StatType::PhysicalDamage)
        .value("MagicDamage", StatType::MagicDamage)
        .value("PhysicalDefense", StatType::PhysicalDefense)
        .value("MagicDefense", StatType::MagicDefense)
        .value("CritChance", StatType::CritChance)
        .value("CritDamage", StatType::CritDamage)
        .value("ArmorPenetration", StatType::ArmorPenetration)
        .value("MagicPenetration", StatType::MagicPenetration)
        .value("MoveSpeed", StatType::MoveSpeed)
        .value("AttackSpeed", StatType::AttackSpeed)
        .value("CastSpeed", StatType::CastSpeed)
        .value("CooldownReduction", StatType::CooldownReduction)
        .value("DodgeChance", StatType::DodgeChance)
        .value("BlockChance", StatType::BlockChance)
        .value("BlockAmount", StatType::BlockAmount)
        .value("Poise", StatType::Poise)
        .value("PoiseRegen", StatType::PoiseRegen)
        .value("FireResistance", StatType::FireResistance)
        .value("IceResistance", StatType::IceResistance)
        .value("LightningResistance", StatType::LightningResistance)
        .value("PoisonResistance", StatType::PoisonResistance)
        .value("BleedResistance", StatType::BleedResistance)
        .value("ExperienceGain", StatType::ExperienceGain)
        .value("GoldFind", StatType::GoldFind)
        .value("ItemFind", StatType::ItemFind)
        .value("CarryCapacity", StatType::CarryCapacity);

    // Register ModifierType enum
    registry.register_enum<ModifierType>("ModifierType")
        .value("Flat", ModifierType::Flat)
        .value("PercentAdd", ModifierType::PercentAdd)
        .value("PercentMult", ModifierType::PercentMult)
        .value("Override", ModifierType::Override);

    // Register ModifierSource enum
    registry.register_enum<ModifierSource>("ModifierSource")
        .value("Equipment", ModifierSource::Equipment)
        .value("Effect", ModifierSource::Effect)
        .value("Skill", ModifierSource::Skill)
        .value("Aura", ModifierSource::Aura)
        .value("Environment", ModifierSource::Environment)
        .value("Temporary", ModifierSource::Temporary)
        .value("Permanent", ModifierSource::Permanent)
        .value("Debug", ModifierSource::Debug)
        .value("Custom", ModifierSource::Custom);

    // Register StatModifier struct
    registry.register_component<StatModifier>("StatModifier")
        .property("stat", &StatModifier::stat)
        .property("type", &StatModifier::type)
        .property("source", &StatModifier::source)
        .property("value", &StatModifier::value)
        .property("priority", &StatModifier::priority)
        .property("source_id", &StatModifier::source_id)
        .property("source_name", &StatModifier::source_name)
        .property("duration", &StatModifier::duration)
        .property("elapsed", &StatModifier::elapsed)
        .property("is_hidden", &StatModifier::is_hidden)
        .property("is_stackable", &StatModifier::is_stackable);

    // Register StatsComponent
    // Note: Maps are serialized separately due to complexity
    registry.register_component<StatsComponent>("StatsComponent")
        .property("needs_recalculation", &StatsComponent::needs_recalculation);

    // Register StatPreset
    registry.register_type<StatPreset>("StatPreset")
        .property("preset_id", &StatPreset::preset_id)
        .property("display_name", &StatPreset::display_name);
}

} // namespace engine::stats
