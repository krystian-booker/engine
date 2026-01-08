#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/inventory/item_definition.hpp>

using namespace engine::inventory;
using namespace engine::stats;
using Catch::Matchers::WithinAbs;

// ============================================================================
// ItemType Tests
// ============================================================================

TEST_CASE("ItemType enum", "[inventory][definition]") {
    REQUIRE(static_cast<uint8_t>(ItemType::Consumable) == 0);
    REQUIRE(static_cast<uint8_t>(ItemType::Equipment) == 1);
    REQUIRE(static_cast<uint8_t>(ItemType::Material) == 2);
    REQUIRE(static_cast<uint8_t>(ItemType::Quest) == 3);
    REQUIRE(static_cast<uint8_t>(ItemType::Key) == 4);
    REQUIRE(static_cast<uint8_t>(ItemType::Currency) == 5);
    REQUIRE(static_cast<uint8_t>(ItemType::Ammo) == 6);
    REQUIRE(static_cast<uint8_t>(ItemType::Misc) == 7);
}

// ============================================================================
// ItemRarity Tests
// ============================================================================

TEST_CASE("ItemRarity enum", "[inventory][definition]") {
    REQUIRE(static_cast<uint8_t>(ItemRarity::Common) == 0);
    REQUIRE(static_cast<uint8_t>(ItemRarity::Uncommon) == 1);
    REQUIRE(static_cast<uint8_t>(ItemRarity::Rare) == 2);
    REQUIRE(static_cast<uint8_t>(ItemRarity::Epic) == 3);
    REQUIRE(static_cast<uint8_t>(ItemRarity::Legendary) == 4);
    REQUIRE(static_cast<uint8_t>(ItemRarity::Unique) == 5);
    REQUIRE(static_cast<uint8_t>(ItemRarity::Artifact) == 6);
}

// ============================================================================
// EquipmentSlot Tests
// ============================================================================

TEST_CASE("EquipmentSlot enum", "[inventory][definition]") {
    REQUIRE(static_cast<uint8_t>(EquipmentSlot::None) == 0);
    REQUIRE(static_cast<uint8_t>(EquipmentSlot::MainHand) == 1);
    REQUIRE(static_cast<uint8_t>(EquipmentSlot::OffHand) == 2);
    REQUIRE(static_cast<uint8_t>(EquipmentSlot::TwoHand) == 3);
    REQUIRE(static_cast<uint8_t>(EquipmentSlot::Head) == 4);
    REQUIRE(static_cast<uint8_t>(EquipmentSlot::Chest) == 5);
    REQUIRE(static_cast<uint8_t>(EquipmentSlot::Hands) == 6);
    REQUIRE(static_cast<uint8_t>(EquipmentSlot::Legs) == 7);
    REQUIRE(static_cast<uint8_t>(EquipmentSlot::Feet) == 8);
    REQUIRE(static_cast<uint8_t>(EquipmentSlot::Neck) == 9);
    REQUIRE(static_cast<uint8_t>(EquipmentSlot::Ring1) == 10);
    REQUIRE(static_cast<uint8_t>(EquipmentSlot::Ring2) == 11);
    REQUIRE(static_cast<uint8_t>(EquipmentSlot::Belt) == 12);
    REQUIRE(static_cast<uint8_t>(EquipmentSlot::Back) == 13);
    REQUIRE(static_cast<uint8_t>(EquipmentSlot::Accessory1) == 14);
    REQUIRE(static_cast<uint8_t>(EquipmentSlot::Accessory2) == 15);
    REQUIRE(static_cast<uint8_t>(EquipmentSlot::Count) == 16);
}

// ============================================================================
// WeaponType Tests
// ============================================================================

TEST_CASE("WeaponType enum", "[inventory][definition]") {
    REQUIRE(static_cast<uint8_t>(WeaponType::None) == 0);
    REQUIRE(static_cast<uint8_t>(WeaponType::Sword) == 1);
    REQUIRE(static_cast<uint8_t>(WeaponType::Axe) == 2);
    REQUIRE(static_cast<uint8_t>(WeaponType::Mace) == 3);
    REQUIRE(static_cast<uint8_t>(WeaponType::Dagger) == 4);
    REQUIRE(static_cast<uint8_t>(WeaponType::Spear) == 5);
    REQUIRE(static_cast<uint8_t>(WeaponType::Staff) == 6);
    REQUIRE(static_cast<uint8_t>(WeaponType::Bow) == 7);
    REQUIRE(static_cast<uint8_t>(WeaponType::Crossbow) == 8);
    REQUIRE(static_cast<uint8_t>(WeaponType::Shield) == 9);
    REQUIRE(static_cast<uint8_t>(WeaponType::TwoHandedSword) == 10);
    REQUIRE(static_cast<uint8_t>(WeaponType::TwoHandedAxe) == 11);
    REQUIRE(static_cast<uint8_t>(WeaponType::TwoHandedMace) == 12);
    REQUIRE(static_cast<uint8_t>(WeaponType::Polearm) == 13);
    REQUIRE(static_cast<uint8_t>(WeaponType::Wand) == 14);
    REQUIRE(static_cast<uint8_t>(WeaponType::Fist) == 15);
}

// ============================================================================
// ArmorType Tests
// ============================================================================

TEST_CASE("ArmorType enum", "[inventory][definition]") {
    REQUIRE(static_cast<uint8_t>(ArmorType::None) == 0);
    REQUIRE(static_cast<uint8_t>(ArmorType::Cloth) == 1);
    REQUIRE(static_cast<uint8_t>(ArmorType::Light) == 2);
    REQUIRE(static_cast<uint8_t>(ArmorType::Medium) == 3);
    REQUIRE(static_cast<uint8_t>(ArmorType::Heavy) == 4);
    REQUIRE(static_cast<uint8_t>(ArmorType::Shield) == 5);
}

// ============================================================================
// ItemRequirement Tests
// ============================================================================

TEST_CASE("ItemRequirement defaults", "[inventory][definition]") {
    ItemRequirement req{};
    req.stat = StatType::Strength;
    req.min_value = 15.0f;
    req.description = "Strength 15";

    REQUIRE(req.stat == StatType::Strength);
    REQUIRE_THAT(req.min_value, WithinAbs(15.0f, 0.001f));
    REQUIRE(req.description == "Strength 15");
}

// ============================================================================
// ItemDefinition Tests
// ============================================================================

TEST_CASE("ItemDefinition defaults", "[inventory][definition]") {
    ItemDefinition def;

    REQUIRE(def.item_id.empty());
    REQUIRE(def.display_name.empty());
    REQUIRE(def.description.empty());
    REQUIRE(def.lore.empty());
    REQUIRE(def.icon_path.empty());
    REQUIRE(def.mesh_path.empty());

    REQUIRE(def.type == ItemType::Misc);
    REQUIRE(def.rarity == ItemRarity::Common);
    REQUIRE(def.slot == EquipmentSlot::None);
    REQUIRE(def.weapon_type == WeaponType::None);
    REQUIRE(def.armor_type == ArmorType::None);

    REQUIRE(def.max_stack == 1);
    REQUIRE_THAT(def.weight, WithinAbs(0.0f, 0.001f));
    REQUIRE(def.base_value == 0);
    REQUIRE(def.buy_price == 0);

    REQUIRE(def.stat_bonuses.empty());
    REQUIRE(def.stat_scaling.empty());
    REQUIRE(def.requirements.empty());
    REQUIRE(def.apply_effects.empty());
    REQUIRE(def.instant_heals.empty());
    REQUIRE(def.tags.empty());

    REQUIRE(def.is_unique == false);
    REQUIRE(def.is_quest_item == false);
    REQUIRE(def.is_tradeable == true);
    REQUIRE(def.is_sellable == true);
    REQUIRE(def.is_droppable == true);
    REQUIRE(def.destroys_on_use == true);

    REQUIRE(def.max_durability == 0);
    REQUIRE(def.breaks_when_depleted == false);

    REQUIRE(def.min_level == 1);
    REQUIRE(def.max_level == 100);
}

TEST_CASE("ItemDefinition helpers", "[inventory][definition]") {
    ItemDefinition def;

    SECTION("is_stackable") {
        def.max_stack = 1;
        REQUIRE_FALSE(def.is_stackable());

        def.max_stack = 20;
        REQUIRE(def.is_stackable());
    }

    SECTION("is_equipment") {
        def.type = ItemType::Misc;
        REQUIRE_FALSE(def.is_equipment());

        def.type = ItemType::Equipment;
        REQUIRE(def.is_equipment());
    }

    SECTION("is_consumable") {
        def.type = ItemType::Misc;
        REQUIRE_FALSE(def.is_consumable());

        def.type = ItemType::Consumable;
        REQUIRE(def.is_consumable());
    }

    SECTION("is_weapon") {
        def.weapon_type = WeaponType::None;
        REQUIRE_FALSE(def.is_weapon());

        def.weapon_type = WeaponType::Sword;
        REQUIRE(def.is_weapon());
    }

    SECTION("is_armor") {
        def.armor_type = ArmorType::None;
        REQUIRE_FALSE(def.is_armor());

        def.armor_type = ArmorType::Heavy;
        REQUIRE(def.is_armor());
    }

    SECTION("has_requirements") {
        REQUIRE_FALSE(def.has_requirements());

        def.requirements.push_back({StatType::Strength, 10.0f, "Strength 10"});
        REQUIRE(def.has_requirements());
    }
}

TEST_CASE("ItemDefinition equipment setup", "[inventory][definition]") {
    ItemDefinition def;
    def.item_id = "iron_sword";
    def.display_name = "Iron Sword";
    def.type = ItemType::Equipment;
    def.slot = EquipmentSlot::MainHand;
    def.weapon_type = WeaponType::Sword;
    def.rarity = ItemRarity::Uncommon;
    def.weight = 3.5f;
    def.base_value = 100;
    def.max_durability = 50;
    def.breaks_when_depleted = true;

    def.stat_bonuses.push_back({StatType::PhysicalDamage, 15.0f});
    def.requirements.push_back({StatType::Strength, 10.0f, "Strength 10"});
    def.tags.push_back("weapon");
    def.tags.push_back("melee");

    REQUIRE(def.item_id == "iron_sword");
    REQUIRE(def.display_name == "Iron Sword");
    REQUIRE(def.is_equipment());
    REQUIRE(def.is_weapon());
    REQUIRE_FALSE(def.is_armor());
    REQUIRE(def.slot == EquipmentSlot::MainHand);
    REQUIRE(def.rarity == ItemRarity::Uncommon);
    REQUIRE_THAT(def.weight, WithinAbs(3.5f, 0.001f));
    REQUIRE(def.stat_bonuses.size() == 1);
    REQUIRE(def.has_requirements());
    REQUIRE(def.tags.size() == 2);
    REQUIRE(def.max_durability == 50);
}

TEST_CASE("ItemDefinition consumable setup", "[inventory][definition]") {
    ItemDefinition def;
    def.item_id = "health_potion";
    def.display_name = "Health Potion";
    def.type = ItemType::Consumable;
    def.max_stack = 20;
    def.destroys_on_use = true;

    def.instant_heals.push_back({StatType::Health, 50.0f});
    def.apply_effects.push_back("regeneration");

    REQUIRE(def.is_consumable());
    REQUIRE(def.is_stackable());
    REQUIRE(def.max_stack == 20);
    REQUIRE(def.destroys_on_use == true);
    REQUIRE(def.instant_heals.size() == 1);
    REQUIRE(def.apply_effects.size() == 1);
    REQUIRE(def.apply_effects[0] == "regeneration");
}

// ============================================================================
// ItemBuilder Tests
// ============================================================================

TEST_CASE("ItemBuilder fluent API weapon", "[inventory][definition]") {
    auto def = item()
        .id("steel_sword")
        .name("Steel Sword")
        .description("A well-crafted steel blade")
        .lore("Forged in the fires of Ironforge")
        .icon("icons/weapons/steel_sword.png")
        .mesh("meshes/weapons/steel_sword.fbx")
        .type(ItemType::Equipment)
        .rarity(ItemRarity::Rare)
        .equipment(EquipmentSlot::MainHand)
        .weapon(WeaponType::Sword)
        .weight(4.0f)
        .value(250, 500)
        .stat(StatType::PhysicalDamage, 25.0f)
        .stat(StatType::CritChance, 5.0f)
        .require(StatType::Strength, 15.0f, "Requires Strength 15")
        // Removed Level req because StatType::Level does not exist
        // .require(StatType::Level, 10.0f, "Requires Level 10")
        .tag("weapon")
        .tag("melee")
        .tag("steel")
        .durability(100, true)
        .build();

    REQUIRE(def.item_id == "steel_sword");
    REQUIRE(def.display_name == "Steel Sword");
    REQUIRE(def.description == "A well-crafted steel blade");
    REQUIRE(def.lore == "Forged in the fires of Ironforge");
    REQUIRE(def.icon_path == "icons/weapons/steel_sword.png");
    REQUIRE(def.mesh_path == "meshes/weapons/steel_sword.fbx");
    REQUIRE(def.type == ItemType::Equipment);
    REQUIRE(def.rarity == ItemRarity::Rare);
    REQUIRE(def.slot == EquipmentSlot::MainHand);
    REQUIRE(def.weapon_type == WeaponType::Sword);
    REQUIRE_THAT(def.weight, WithinAbs(4.0f, 0.001f));
    REQUIRE(def.base_value == 250);
    REQUIRE(def.buy_price == 500);
    REQUIRE(def.stat_bonuses.size() == 2);
    // Requirements size is 1 now
    REQUIRE(def.requirements.size() == 1);
    REQUIRE(def.tags.size() == 3);
    REQUIRE(def.max_durability == 100);
    REQUIRE(def.breaks_when_depleted == true);
}

TEST_CASE("ItemBuilder fluent API consumable", "[inventory][definition]") {
    auto def = item()
        .id("mana_potion")
        .name("Mana Potion")
        .description("Restores mana")
        .type(ItemType::Consumable)
        .rarity(ItemRarity::Common)
        .stack(50)
        .weight(0.2f)
        .value(10, 25)
        .heal(StatType::Mana, 100.0f)
        .effect("mana_regen_boost")
        .tag("potion")
        .tag("consumable")
        .build();

    REQUIRE(def.item_id == "mana_potion");
    REQUIRE(def.display_name == "Mana Potion");
    REQUIRE(def.type == ItemType::Consumable);
    REQUIRE(def.rarity == ItemRarity::Common);
    REQUIRE(def.max_stack == 50);
    REQUIRE(def.is_stackable());
    REQUIRE_THAT(def.weight, WithinAbs(0.2f, 0.001f));
    REQUIRE(def.base_value == 10);
    REQUIRE(def.buy_price == 25);
    REQUIRE(def.instant_heals.size() == 1);
    REQUIRE(def.apply_effects.size() == 1);
    REQUIRE(def.tags.size() == 2);
}

TEST_CASE("ItemBuilder fluent API armor", "[inventory][definition]") {
    auto def = item()
        .id("plate_chest")
        .name("Plate Chestpiece")
        .type(ItemType::Equipment)
        .rarity(ItemRarity::Epic)
        .equipment(EquipmentSlot::Chest)
        .armor(ArmorType::Heavy)
        .stat(StatType::PhysicalDefense, 50.0f)
        .stat(StatType::MaxHealth, 100.0f)
        .unique()
        .build();

    REQUIRE(def.item_id == "plate_chest");
    REQUIRE(def.type == ItemType::Equipment);
    REQUIRE(def.slot == EquipmentSlot::Chest);
    REQUIRE(def.armor_type == ArmorType::Heavy);
    REQUIRE(def.rarity == ItemRarity::Epic);
    REQUIRE(def.is_equipment());
    REQUIRE(def.is_armor());
    REQUIRE_FALSE(def.is_weapon());
    REQUIRE(def.is_unique == true);
    REQUIRE(def.stat_bonuses.size() == 2);
}

TEST_CASE("ItemBuilder fluent API quest item", "[inventory][definition]") {
    auto def = item()
        .id("ancient_key")
        .name("Ancient Key")
        .description("Opens the door to the ancient temple")
        .type(ItemType::Key)
        .rarity(ItemRarity::Unique)
        .quest_item()
        .build();

    REQUIRE(def.item_id == "ancient_key");
    REQUIRE(def.type == ItemType::Key);
    REQUIRE(def.rarity == ItemRarity::Unique);
    REQUIRE(def.is_quest_item == true);
    // Quest items cannot be dropped/sold
    REQUIRE(def.is_droppable == false);
    REQUIRE(def.is_sellable == false);
    REQUIRE(def.is_tradeable == false);
}
