#include <engine/stats/stat_component.hpp>
#include <engine/stats/stat_modifier.hpp>
#include <engine/reflect/type_registry.hpp>

namespace engine::stats {

void register_stats_components() {
    auto& registry = reflect::TypeRegistry::instance();

    // Register StatType enum
    registry.register_enum<StatType>("StatType", {
        {StatType::Health, "Health"},
        {StatType::MaxHealth, "MaxHealth"},
        {StatType::HealthRegen, "HealthRegen"},
        {StatType::Stamina, "Stamina"},
        {StatType::MaxStamina, "MaxStamina"},
        {StatType::StaminaRegen, "StaminaRegen"},
        {StatType::Mana, "Mana"},
        {StatType::MaxMana, "MaxMana"},
        {StatType::ManaRegen, "ManaRegen"},
        {StatType::Strength, "Strength"},
        {StatType::Dexterity, "Dexterity"},
        {StatType::Intelligence, "Intelligence"},
        {StatType::Vitality, "Vitality"},
        {StatType::Luck, "Luck"},
        {StatType::Endurance, "Endurance"},
        {StatType::Agility, "Agility"},
        {StatType::Wisdom, "Wisdom"},
        {StatType::Charisma, "Charisma"},
        {StatType::PhysicalDamage, "PhysicalDamage"},
        {StatType::MagicDamage, "MagicDamage"},
        {StatType::PhysicalDefense, "PhysicalDefense"},
        {StatType::MagicDefense, "MagicDefense"},
        {StatType::CritChance, "CritChance"},
        {StatType::CritDamage, "CritDamage"},
        {StatType::ArmorPenetration, "ArmorPenetration"},
        {StatType::MagicPenetration, "MagicPenetration"},
        {StatType::MoveSpeed, "MoveSpeed"},
        {StatType::AttackSpeed, "AttackSpeed"},
        {StatType::CastSpeed, "CastSpeed"},
        {StatType::CooldownReduction, "CooldownReduction"},
        {StatType::DodgeChance, "DodgeChance"},
        {StatType::BlockChance, "BlockChance"},
        {StatType::BlockAmount, "BlockAmount"},
        {StatType::Poise, "Poise"},
        {StatType::PoiseRegen, "PoiseRegen"},
        {StatType::FireResistance, "FireResistance"},
        {StatType::IceResistance, "IceResistance"},
        {StatType::LightningResistance, "LightningResistance"},
        {StatType::PoisonResistance, "PoisonResistance"},
        {StatType::BleedResistance, "BleedResistance"},
        {StatType::ExperienceGain, "ExperienceGain"},
        {StatType::GoldFind, "GoldFind"},
        {StatType::ItemFind, "ItemFind"},
        {StatType::CarryCapacity, "CarryCapacity"}
    });

    // Register ModifierType enum
    registry.register_enum<ModifierType>("ModifierType", {
        {ModifierType::Flat, "Flat"},
        {ModifierType::PercentAdd, "PercentAdd"},
        {ModifierType::PercentMult, "PercentMult"},
        {ModifierType::Override, "Override"}
    });

    // Register ModifierSource enum
    registry.register_enum<ModifierSource>("ModifierSource", {
        {ModifierSource::Equipment, "Equipment"},
        {ModifierSource::Effect, "Effect"},
        {ModifierSource::Skill, "Skill"},
        {ModifierSource::Aura, "Aura"},
        {ModifierSource::Environment, "Environment"},
        {ModifierSource::Temporary, "Temporary"},
        {ModifierSource::Permanent, "Permanent"},
        {ModifierSource::Debug, "Debug"},
        {ModifierSource::Custom, "Custom"}
    });

    // Register StatModifier struct
    // Register StatModifier struct
    registry.register_component<StatModifier>("StatModifier");
    registry.register_property<StatModifier, &StatModifier::stat>("stat");
    registry.register_property<StatModifier, &StatModifier::type>("type");
    registry.register_property<StatModifier, &StatModifier::source>("source");
    registry.register_property<StatModifier, &StatModifier::value>("value");
    registry.register_property<StatModifier, &StatModifier::priority>("priority");
    registry.register_property<StatModifier, &StatModifier::source_id>("source_id");
    registry.register_property<StatModifier, &StatModifier::source_name>("source_name");
    registry.register_property<StatModifier, &StatModifier::duration>("duration");
    registry.register_property<StatModifier, &StatModifier::elapsed>("elapsed");
    registry.register_property<StatModifier, &StatModifier::is_hidden>("is_hidden");
    registry.register_property<StatModifier, &StatModifier::is_stackable>("is_stackable");

    // Register StatsComponent
    // Note: Maps are serialized separately due to complexity
    registry.register_component<StatsComponent>("StatsComponent");
    registry.register_property<StatsComponent, &StatsComponent::needs_recalculation>("needs_recalculation");

    // Register StatPreset
    registry.register_type<StatPreset>("StatPreset");
    registry.register_property<StatPreset, &StatPreset::preset_id>("preset_id");
    registry.register_property<StatPreset, &StatPreset::display_name>("display_name");
}

} // namespace engine::stats
