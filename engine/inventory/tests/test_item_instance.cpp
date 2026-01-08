#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/inventory/item_instance.hpp>

using namespace engine::inventory;
using namespace engine::stats;
using Catch::Matchers::WithinAbs;

// ============================================================================
// ModifierTier Tests
// ============================================================================

TEST_CASE("ModifierTier enum", "[inventory][instance]") {
    REQUIRE(static_cast<uint8_t>(ModifierTier::Minor) == 0);
    REQUIRE(static_cast<uint8_t>(ModifierTier::Lesser) == 1);
    REQUIRE(static_cast<uint8_t>(ModifierTier::Normal) == 2);
    REQUIRE(static_cast<uint8_t>(ModifierTier::Greater) == 3);
    REQUIRE(static_cast<uint8_t>(ModifierTier::Major) == 4);
}

// ============================================================================
// ItemRandomModifier Tests
// ============================================================================

TEST_CASE("ItemRandomModifier", "[inventory][instance]") {
    ItemRandomModifier mod;
    mod.stat = StatType::Strength;
    mod.modifier_type = ModifierType::Flat;
    mod.value = 10.0f;
    mod.tier = ModifierTier::Greater;
    mod.prefix = "Sturdy";
    mod.suffix = "";

    REQUIRE(mod.stat == StatType::Strength);
    REQUIRE(mod.modifier_type == ModifierType::Flat);
    REQUIRE_THAT(mod.value, WithinAbs(10.0f, 0.001f));
    REQUIRE(mod.tier == ModifierTier::Greater);
    REQUIRE(mod.prefix == "Sturdy");
    REQUIRE(mod.suffix.empty());
}

TEST_CASE("ItemRandomModifier suffix style", "[inventory][instance]") {
    ItemRandomModifier mod;
    mod.stat = StatType::CritChance;
    mod.modifier_type = ModifierType::PercentAdd;
    mod.value = 0.05f;
    mod.tier = ModifierTier::Major;
    mod.prefix = "";
    mod.suffix = "of Precision";

    REQUIRE(mod.stat == StatType::CritChance);
    REQUIRE(mod.modifier_type == ModifierType::PercentAdd);
    REQUIRE_THAT(mod.value, WithinAbs(0.05f, 0.001f));
    REQUIRE(mod.tier == ModifierTier::Major);
    REQUIRE(mod.prefix.empty());
    REQUIRE(mod.suffix == "of Precision");
}

// ============================================================================
// ItemInstance Tests
// ============================================================================

TEST_CASE("ItemInstance defaults", "[inventory][instance]") {
    ItemInstance instance;

    REQUIRE(instance.instance_id.is_null());
    REQUIRE(instance.definition_id.empty());
    REQUIRE(instance.stack_count == 1);
    REQUIRE(instance.item_level == 1);
    REQUIRE(instance.quality == 0);
    REQUIRE(instance.current_durability == -1);
    REQUIRE(instance.max_durability == -1);
    REQUIRE(instance.random_modifiers.empty());
    REQUIRE(instance.socket_gems.empty());
    REQUIRE(instance.enchantments.empty());
    REQUIRE(instance.custom_data.empty());
    REQUIRE(instance.is_bound == false);
    REQUIRE(instance.bound_to.is_null());
    REQUIRE(instance.created_timestamp == 0);
    REQUIRE(instance.acquired_timestamp == 0);
}

TEST_CASE("ItemInstance validity", "[inventory][instance]") {
    ItemInstance instance;

    SECTION("Invalid when definition_id empty") {
        REQUIRE_FALSE(instance.is_valid());
    }

    SECTION("Valid when definition_id set") {
        instance.definition_id = "health_potion";
        REQUIRE(instance.is_valid());
    }
}

TEST_CASE("ItemInstance durability queries", "[inventory][instance]") {
    ItemInstance instance;

    SECTION("has_durability - no durability") {
        instance.max_durability = -1;
        REQUIRE_FALSE(instance.has_durability());

        instance.max_durability = 0;
        REQUIRE_FALSE(instance.has_durability());
    }

    SECTION("has_durability - with durability") {
        instance.max_durability = 100;
        REQUIRE(instance.has_durability());
    }
}

TEST_CASE("ItemInstance durability percent", "[inventory][instance]") {
    ItemInstance instance;
    instance.max_durability = 100;
    instance.current_durability = 75;

    float percent = instance.get_durability_percent();
    REQUIRE_THAT(percent, WithinAbs(0.75f, 0.01f));
}

TEST_CASE("ItemInstance durability percent full", "[inventory][instance]") {
    ItemInstance instance;
    instance.max_durability = 50;
    instance.current_durability = 50;

    float percent = instance.get_durability_percent();
    REQUIRE_THAT(percent, WithinAbs(1.0f, 0.01f));
}

TEST_CASE("ItemInstance durability percent empty", "[inventory][instance]") {
    ItemInstance instance;
    instance.max_durability = 100;
    instance.current_durability = 0;

    float percent = instance.get_durability_percent();
    REQUIRE_THAT(percent, WithinAbs(0.0f, 0.01f));
}

TEST_CASE("ItemInstance custom data", "[inventory][instance]") {
    ItemInstance instance;
    instance.definition_id = "magic_staff";
    instance.custom_data["crafted_by"] = "Player123";
    instance.custom_data["inscription"] = "For my beloved";
    instance.custom_data["kill_count"] = "42";

    REQUIRE(instance.custom_data.size() == 3);
    REQUIRE(instance.custom_data["crafted_by"] == "Player123");
    REQUIRE(instance.custom_data["inscription"] == "For my beloved");
    REQUIRE(instance.custom_data["kill_count"] == "42");
}

TEST_CASE("ItemInstance sockets and enchantments", "[inventory][instance]") {
    ItemInstance instance;
    instance.definition_id = "legendary_sword";

    instance.socket_gems.push_back("ruby_gem");
    instance.socket_gems.push_back("diamond_gem");

    instance.enchantments.push_back("flame_enchant");
    instance.enchantments.push_back("lifesteal_enchant");

    REQUIRE(instance.socket_gems.size() == 2);
    REQUIRE(instance.socket_gems[0] == "ruby_gem");
    REQUIRE(instance.socket_gems[1] == "diamond_gem");

    REQUIRE(instance.enchantments.size() == 2);
    REQUIRE(instance.enchantments[0] == "flame_enchant");
    REQUIRE(instance.enchantments[1] == "lifesteal_enchant");
}

TEST_CASE("ItemInstance binding", "[inventory][instance]") {
    ItemInstance instance;
    instance.definition_id = "epic_armor";
    instance.is_bound = true;
    instance.bound_to = engine::core::UUID::generate();

    REQUIRE(instance.is_bound == true);
    REQUIRE_FALSE(instance.bound_to.is_null());
}

TEST_CASE("ItemInstance random modifiers", "[inventory][instance]") {
    ItemInstance instance;
    instance.definition_id = "magic_ring";

    ItemRandomModifier mod1;
    mod1.stat = StatType::Intelligence;
    mod1.modifier_type = ModifierType::Flat;
    mod1.value = 15.0f;
    mod1.tier = ModifierTier::Greater;
    mod1.prefix = "Wise";

    ItemRandomModifier mod2;
    mod2.stat = StatType::MagicDamage;
    mod2.modifier_type = ModifierType::PercentAdd;
    mod2.value = 0.10f;
    mod2.tier = ModifierTier::Major;
    mod2.suffix = "of the Arcane";

    instance.random_modifiers.push_back(mod1);
    instance.random_modifiers.push_back(mod2);

    REQUIRE(instance.random_modifiers.size() == 2);
    REQUIRE(instance.random_modifiers[0].stat == StatType::Intelligence);
    REQUIRE(instance.random_modifiers[0].prefix == "Wise");
    REQUIRE(instance.random_modifiers[1].stat == StatType::MagicDamage);
    REQUIRE(instance.random_modifiers[1].suffix == "of the Arcane");
}

TEST_CASE("ItemInstance timestamps", "[inventory][instance]") {
    ItemInstance instance;
    instance.definition_id = "quest_item";
    instance.created_timestamp = 1704067200;  // Some timestamp
    instance.acquired_timestamp = 1704153600;

    REQUIRE(instance.created_timestamp == 1704067200);
    REQUIRE(instance.acquired_timestamp == 1704153600);
}

// ============================================================================
// LootTableEntry Tests
// ============================================================================

TEST_CASE("LootTableEntry defaults", "[inventory][loot]") {
    LootTableEntry entry;

    REQUIRE(entry.item_id.empty());
    REQUIRE_THAT(entry.weight, WithinAbs(1.0f, 0.001f));
    REQUIRE(entry.min_count == 1);
    REQUIRE(entry.max_count == 1);
    REQUIRE(entry.min_level == 1);
    REQUIRE(entry.max_level == 100);
    REQUIRE_THAT(entry.quality_bonus, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("LootTableEntry custom values", "[inventory][loot]") {
    LootTableEntry entry;
    entry.item_id = "gold_coin";
    entry.weight = 5.0f;
    entry.min_count = 10;
    entry.max_count = 50;
    entry.min_level = 1;
    entry.max_level = 10;
    entry.quality_bonus = 0.1f;

    REQUIRE(entry.item_id == "gold_coin");
    REQUIRE_THAT(entry.weight, WithinAbs(5.0f, 0.001f));
    REQUIRE(entry.min_count == 10);
    REQUIRE(entry.max_count == 50);
    REQUIRE(entry.min_level == 1);
    REQUIRE(entry.max_level == 10);
    REQUIRE_THAT(entry.quality_bonus, WithinAbs(0.1f, 0.001f));
}

// ============================================================================
// LootTable Tests
// ============================================================================

TEST_CASE("LootTable defaults", "[inventory][loot]") {
    LootTable table;

    REQUIRE(table.table_id.empty());
    REQUIRE(table.entries.empty());
    REQUIRE(table.guaranteed_drops == 0);
    REQUIRE(table.max_drops == 1);
    REQUIRE_THAT(table.nothing_chance, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("LootTable custom setup", "[inventory][loot]") {
    LootTable table;
    table.table_id = "goblin_loot";
    table.guaranteed_drops = 1;
    table.max_drops = 3;
    table.nothing_chance = 0.1f;

    LootTableEntry gold;
    gold.item_id = "gold_coin";
    gold.weight = 10.0f;
    gold.min_count = 5;
    gold.max_count = 20;

    LootTableEntry sword;
    sword.item_id = "rusty_sword";
    sword.weight = 2.0f;
    sword.min_level = 1;
    sword.max_level = 5;

    LootTableEntry rare;
    rare.item_id = "goblin_charm";
    rare.weight = 0.5f;
    rare.quality_bonus = 0.25f;

    table.entries.push_back(gold);
    table.entries.push_back(sword);
    table.entries.push_back(rare);

    REQUIRE(table.table_id == "goblin_loot");
    REQUIRE(table.entries.size() == 3);
    REQUIRE(table.guaranteed_drops == 1);
    REQUIRE(table.max_drops == 3);
    REQUIRE_THAT(table.nothing_chance, WithinAbs(0.1f, 0.001f));

    // Check entries
    REQUIRE(table.entries[0].item_id == "gold_coin");
    REQUIRE_THAT(table.entries[0].weight, WithinAbs(10.0f, 0.001f));
    REQUIRE(table.entries[1].item_id == "rusty_sword");
    REQUIRE(table.entries[2].item_id == "goblin_charm");
    REQUIRE_THAT(table.entries[2].quality_bonus, WithinAbs(0.25f, 0.001f));
}

// ============================================================================
// ItemInstanceBuilder Tests
// ============================================================================

TEST_CASE("ItemInstanceBuilder basic", "[inventory][instance]") {
    auto instance = create_item()
        .from("iron_sword")
        .count(1)
        .level(10)
        .quality(75)
        .build();

    REQUIRE(instance.definition_id == "iron_sword");
    REQUIRE(instance.stack_count == 1);
    REQUIRE(instance.item_level == 10);
    REQUIRE(instance.quality == 75);
}

TEST_CASE("ItemInstanceBuilder with durability", "[inventory][instance]") {
    auto instance = create_item()
        .from("steel_armor")
        .durability(80, 100)
        .build();

    REQUIRE(instance.definition_id == "steel_armor");
    REQUIRE(instance.current_durability == 80);
    REQUIRE(instance.max_durability == 100);
    REQUIRE(instance.has_durability());
}

TEST_CASE("ItemInstanceBuilder with modifiers", "[inventory][instance]") {
    auto instance = create_item()
        .from("magic_ring")
        .level(20)
        .modifier(StatType::Intelligence, 15.0f, ModifierTier::Greater)
        .modifier(StatType::MagicDamage, 10.0f, ModifierTier::Normal)
        .build();

    REQUIRE(instance.definition_id == "magic_ring");
    REQUIRE(instance.item_level == 20);
    REQUIRE(instance.random_modifiers.size() == 2);
    REQUIRE(instance.random_modifiers[0].stat == StatType::Intelligence);
    REQUIRE_THAT(instance.random_modifiers[0].value, WithinAbs(15.0f, 0.001f));
    REQUIRE(instance.random_modifiers[0].tier == ModifierTier::Greater);
}

TEST_CASE("ItemInstanceBuilder with sockets and enchants", "[inventory][instance]") {
    auto instance = create_item()
        .from("legendary_helm")
        .socket("ruby_gem")
        .socket("sapphire_gem")
        .enchant("fortitude_enchant")
        .build();

    REQUIRE(instance.socket_gems.size() == 2);
    REQUIRE(instance.socket_gems[0] == "ruby_gem");
    REQUIRE(instance.socket_gems[1] == "sapphire_gem");
    REQUIRE(instance.enchantments.size() == 1);
    REQUIRE(instance.enchantments[0] == "fortitude_enchant");
}

TEST_CASE("ItemInstanceBuilder with binding", "[inventory][instance]") {
    auto instance = create_item()
        .from("soulbound_weapon")
        .bind()
        .build();

    REQUIRE(instance.is_bound == true);
}

TEST_CASE("ItemInstanceBuilder with custom data", "[inventory][instance]") {
    auto instance = create_item()
        .from("crafted_item")
        .custom("crafter", "Artisan")
        .custom("date", "2024-01-01")
        .build();

    REQUIRE(instance.custom_data.size() == 2);
    REQUIRE(instance.custom_data["crafter"] == "Artisan");
    REQUIRE(instance.custom_data["date"] == "2024-01-01");
}
