#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/inventory/inventory_component.hpp>

using namespace engine::inventory;
using namespace engine::stats;
using Catch::Matchers::WithinAbs;

// ============================================================================
// InventorySlot Tests
// ============================================================================

TEST_CASE("InventorySlot defaults", "[inventory][component]") {
    InventorySlot slot;

    REQUIRE_FALSE(slot.item.has_value());
    REQUIRE(slot.is_locked == false);
    REQUIRE(slot.is_favorite == false);
    REQUIRE(slot.is_empty());
    REQUIRE_FALSE(slot.has_item());
}

TEST_CASE("InventorySlot with item", "[inventory][component]") {
    InventorySlot slot;

    ItemInstance item;
    item.definition_id = "health_potion";
    item.stack_count = 5;

    slot.item = item;

    REQUIRE(slot.has_item());
    REQUIRE_FALSE(slot.is_empty());
    REQUIRE(slot.item->definition_id == "health_potion");
    REQUIRE(slot.item->stack_count == 5);
}

TEST_CASE("InventorySlot locked", "[inventory][component]") {
    InventorySlot slot;
    slot.is_locked = true;

    REQUIRE(slot.is_locked == true);
    REQUIRE(slot.is_empty());  // Locked doesn't mean has item
}

TEST_CASE("InventorySlot favorite", "[inventory][component]") {
    InventorySlot slot;

    ItemInstance item;
    item.definition_id = "legendary_sword";
    slot.item = item;
    slot.is_favorite = true;

    REQUIRE(slot.has_item());
    REQUIRE(slot.is_favorite == true);
}

// ============================================================================
// InventoryComponent Tests
// ============================================================================

TEST_CASE("InventoryComponent defaults", "[inventory][component]") {
    InventoryComponent inv;

    REQUIRE(inv.slots.empty());
    REQUIRE(inv.max_slots == 40);
    REQUIRE_THAT(inv.max_weight, WithinAbs(0.0f, 0.001f));
    REQUIRE(inv.auto_sort == false);
    REQUIRE(inv.currencies.empty());
}

TEST_CASE("InventoryComponent slot count", "[inventory][component]") {
    InventoryComponent inv;

    REQUIRE(inv.get_slot_count() == 0);

    inv.resize(20);
    REQUIRE(inv.get_slot_count() == 20);
    REQUIRE(inv.slots.size() == 20);

    inv.resize(40);
    REQUIRE(inv.get_slot_count() == 40);
}

TEST_CASE("InventoryComponent get_slot", "[inventory][component]") {
    InventoryComponent inv;
    inv.resize(10);

    SECTION("Valid index") {
        InventorySlot* slot = inv.get_slot(0);
        REQUIRE(slot != nullptr);
        REQUIRE(slot->is_empty());

        const InventorySlot* const_slot = static_cast<const InventoryComponent&>(inv).get_slot(5);
        REQUIRE(const_slot != nullptr);
    }

    SECTION("Invalid index - returns nullptr") {
        InventorySlot* slot = inv.get_slot(100);
        REQUIRE(slot == nullptr);

        slot = inv.get_slot(-1);
        REQUIRE(slot == nullptr);
    }
}

TEST_CASE("InventoryComponent count queries", "[inventory][component]") {
    InventoryComponent inv;
    inv.resize(10);

    SECTION("All empty") {
        REQUIRE(inv.count_empty_slots() == 10);
        REQUIRE(inv.count_used_slots() == 0);
        REQUIRE(inv.count_total_items() == 0);
    }

    SECTION("Some occupied") {
        ItemInstance item1;
        item1.definition_id = "potion";
        item1.stack_count = 5;
        inv.slots[0].item = item1;

        ItemInstance item2;
        item2.definition_id = "sword";
        item2.stack_count = 1;
        inv.slots[3].item = item2;

        REQUIRE(inv.count_empty_slots() == 8);
        REQUIRE(inv.count_used_slots() == 2);
        REQUIRE(inv.count_total_items() == 6);  // 5 + 1
    }
}

TEST_CASE("InventoryComponent find_empty_slot", "[inventory][component]") {
    InventoryComponent inv;
    inv.resize(5);

    SECTION("All empty - returns first") {
        int slot = inv.find_empty_slot();
        REQUIRE(slot == 0);
    }

    SECTION("First occupied - returns second") {
        ItemInstance item;
        item.definition_id = "item";
        inv.slots[0].item = item;

        int slot = inv.find_empty_slot();
        REQUIRE(slot == 1);
    }

    SECTION("All occupied - returns -1") {
        ItemInstance item;
        item.definition_id = "item";
        for (auto& s : inv.slots) {
            s.item = item;
        }

        int slot = inv.find_empty_slot();
        REQUIRE(slot == -1);
    }
}

TEST_CASE("InventoryComponent find_item", "[inventory][component]") {
    InventoryComponent inv;
    inv.resize(10);

    ItemInstance potion;
    potion.definition_id = "health_potion";
    inv.slots[3].item = potion;

    ItemInstance sword;
    sword.definition_id = "iron_sword";
    inv.slots[7].item = sword;

    SECTION("Find existing item") {
        int slot = inv.find_item("health_potion");
        REQUIRE(slot == 3);

        slot = inv.find_item("iron_sword");
        REQUIRE(slot == 7);
    }

    SECTION("Find non-existing item") {
        int slot = inv.find_item("gold_bar");
        REQUIRE(slot == -1);
    }
}

TEST_CASE("InventoryComponent find_all_items", "[inventory][component]") {
    InventoryComponent inv;
    inv.resize(10);

    ItemInstance potion;
    potion.definition_id = "health_potion";
    inv.slots[1].item = potion;
    inv.slots[4].item = potion;
    inv.slots[8].item = potion;

    auto slots = inv.find_all_items("health_potion");
    REQUIRE(slots.size() == 3);
    REQUIRE(slots[0] == 1);
    REQUIRE(slots[1] == 4);
    REQUIRE(slots[2] == 8);
}

TEST_CASE("InventoryComponent count_item", "[inventory][component]") {
    InventoryComponent inv;
    inv.resize(10);

    ItemInstance potion1;
    potion1.definition_id = "health_potion";
    potion1.stack_count = 10;
    inv.slots[0].item = potion1;

    ItemInstance potion2;
    potion2.definition_id = "health_potion";
    potion2.stack_count = 5;
    inv.slots[5].item = potion2;

    ItemInstance sword;
    sword.definition_id = "iron_sword";
    sword.stack_count = 1;
    inv.slots[3].item = sword;

    int count = inv.count_item("health_potion");
    REQUIRE(count == 15);  // 10 + 5

    count = inv.count_item("iron_sword");
    REQUIRE(count == 1);

    count = inv.count_item("gold_bar");
    REQUIRE(count == 0);
}

TEST_CASE("InventoryComponent has_item", "[inventory][component]") {
    InventoryComponent inv;
    inv.resize(10);

    ItemInstance potion;
    potion.definition_id = "health_potion";
    potion.stack_count = 10;
    inv.slots[0].item = potion;

    REQUIRE(inv.has_item("health_potion"));
    REQUIRE(inv.has_item("health_potion", 5));
    REQUIRE(inv.has_item("health_potion", 10));
    REQUIRE_FALSE(inv.has_item("health_potion", 15));
    REQUIRE_FALSE(inv.has_item("mana_potion"));
}

// ============================================================================
// Currency Tests
// ============================================================================

TEST_CASE("InventoryComponent currency operations", "[inventory][component]") {
    InventoryComponent inv;

    SECTION("Get non-existing currency returns 0") {
        int64_t gold = inv.get_currency("gold");
        REQUIRE(gold == 0);
    }

    SECTION("Set and get currency") {
        inv.set_currency("gold", 1000);
        REQUIRE(inv.get_currency("gold") == 1000);
    }

    SECTION("Add currency") {
        inv.set_currency("gold", 500);
        inv.add_currency("gold", 250);
        REQUIRE(inv.get_currency("gold") == 750);
    }

    SECTION("Add to non-existing currency") {
        inv.add_currency("gems", 50);
        REQUIRE(inv.get_currency("gems") == 50);
    }

    SECTION("Can afford") {
        inv.set_currency("gold", 1000);
        REQUIRE(inv.can_afford("gold", 500));
        REQUIRE(inv.can_afford("gold", 1000));
        REQUIRE_FALSE(inv.can_afford("gold", 1500));
        REQUIRE_FALSE(inv.can_afford("gems", 1));
    }

    SECTION("Spend currency - success") {
        inv.set_currency("gold", 1000);
        bool success = inv.spend_currency("gold", 300);
        REQUIRE(success == true);
        REQUIRE(inv.get_currency("gold") == 700);
    }

    SECTION("Spend currency - failure") {
        inv.set_currency("gold", 100);
        bool success = inv.spend_currency("gold", 500);
        REQUIRE(success == false);
        REQUIRE(inv.get_currency("gold") == 100);  // Unchanged
    }
}

TEST_CASE("InventoryComponent multiple currencies", "[inventory][component]") {
    InventoryComponent inv;

    inv.set_currency("gold", 1000);
    inv.set_currency("silver", 5000);
    inv.set_currency("gems", 50);

    REQUIRE(inv.currencies.size() == 3);
    REQUIRE(inv.get_currency("gold") == 1000);
    REQUIRE(inv.get_currency("silver") == 5000);
    REQUIRE(inv.get_currency("gems") == 50);
}

// ============================================================================
// EquipmentComponent Tests
// ============================================================================

TEST_CASE("EquipmentComponent defaults", "[inventory][component]") {
    EquipmentComponent equip;

    // All slots should be empty
    REQUIRE(equip.is_slot_empty(EquipmentSlot::MainHand));
    REQUIRE(equip.is_slot_empty(EquipmentSlot::Head));
    REQUIRE(equip.is_slot_empty(EquipmentSlot::Chest));
    REQUIRE_FALSE(equip.has_equipped(EquipmentSlot::MainHand));
}

TEST_CASE("EquipmentComponent slot access", "[inventory][component]") {
    EquipmentComponent equip;

    SECTION("Get empty slot returns nullptr") {
        ItemInstance* item = equip.get_equipped(EquipmentSlot::MainHand);
        REQUIRE(item == nullptr);
    }

    SECTION("Get occupied slot returns item") {
        ItemInstance sword;
        sword.definition_id = "iron_sword";
        equip.slots[static_cast<size_t>(EquipmentSlot::MainHand)] = sword;

        ItemInstance* item = equip.get_equipped(EquipmentSlot::MainHand);
        REQUIRE(item != nullptr);
        REQUIRE(item->definition_id == "iron_sword");
    }
}

TEST_CASE("EquipmentComponent equip/unequip", "[inventory][component]") {
    EquipmentComponent equip;

    SECTION("Equip to empty slot") {
        ItemInstance sword;
        sword.definition_id = "iron_sword";

        auto old = equip.equip(EquipmentSlot::MainHand, sword);
        REQUIRE_FALSE(old.has_value());  // Nothing was equipped before
        REQUIRE(equip.has_equipped(EquipmentSlot::MainHand));
        REQUIRE(equip.get_equipped(EquipmentSlot::MainHand)->definition_id == "iron_sword");
    }

    SECTION("Equip replacing existing item") {
        ItemInstance sword1;
        sword1.definition_id = "iron_sword";
        equip.equip(EquipmentSlot::MainHand, sword1);

        ItemInstance sword2;
        sword2.definition_id = "steel_sword";
        auto old = equip.equip(EquipmentSlot::MainHand, sword2);

        REQUIRE(old.has_value());
        REQUIRE(old->definition_id == "iron_sword");
        REQUIRE(equip.get_equipped(EquipmentSlot::MainHand)->definition_id == "steel_sword");
    }

    SECTION("Unequip") {
        ItemInstance helm;
        helm.definition_id = "iron_helm";
        equip.equip(EquipmentSlot::Head, helm);

        auto removed = equip.unequip(EquipmentSlot::Head);
        REQUIRE(removed.has_value());
        REQUIRE(removed->definition_id == "iron_helm");
        REQUIRE(equip.is_slot_empty(EquipmentSlot::Head));
    }

    SECTION("Unequip empty slot") {
        auto removed = equip.unequip(EquipmentSlot::Head);
        REQUIRE_FALSE(removed.has_value());
    }
}

TEST_CASE("EquipmentComponent unequip_all", "[inventory][component]") {
    EquipmentComponent equip;

    ItemInstance sword;
    sword.definition_id = "sword";
    equip.equip(EquipmentSlot::MainHand, sword);

    ItemInstance helm;
    helm.definition_id = "helm";
    equip.equip(EquipmentSlot::Head, helm);

    ItemInstance chest;
    chest.definition_id = "chest";
    equip.equip(EquipmentSlot::Chest, chest);

    REQUIRE(equip.count_equipped() == 3);

    equip.unequip_all();

    REQUIRE(equip.count_equipped() == 0);
    REQUIRE(equip.is_slot_empty(EquipmentSlot::MainHand));
    REQUIRE(equip.is_slot_empty(EquipmentSlot::Head));
    REQUIRE(equip.is_slot_empty(EquipmentSlot::Chest));
}

TEST_CASE("EquipmentComponent slot queries", "[inventory][component]") {
    EquipmentComponent equip;

    ItemInstance sword;
    sword.definition_id = "sword";
    equip.equip(EquipmentSlot::MainHand, sword);

    ItemInstance helm;
    helm.definition_id = "helm";
    equip.equip(EquipmentSlot::Head, helm);

    SECTION("Get occupied slots") {
        auto occupied = equip.get_occupied_slots();
        REQUIRE(occupied.size() == 2);
        // Order may vary, just check both are present
        bool has_mainhand = false;
        bool has_head = false;
        for (auto slot : occupied) {
            if (slot == EquipmentSlot::MainHand) has_mainhand = true;
            if (slot == EquipmentSlot::Head) has_head = true;
        }
        REQUIRE(has_mainhand);
        REQUIRE(has_head);
    }

    SECTION("Count equipped") {
        REQUIRE(equip.count_equipped() == 2);
    }
}

TEST_CASE("EquipmentComponent full equipment set", "[inventory][component]") {
    EquipmentComponent equip;

    // Equip a full set
    ItemInstance mainhand;
    mainhand.definition_id = "sword";
    equip.equip(EquipmentSlot::MainHand, mainhand);

    ItemInstance offhand;
    offhand.definition_id = "shield";
    equip.equip(EquipmentSlot::OffHand, offhand);

    ItemInstance head;
    head.definition_id = "helm";
    equip.equip(EquipmentSlot::Head, head);

    ItemInstance chest;
    chest.definition_id = "chestplate";
    equip.equip(EquipmentSlot::Chest, chest);

    ItemInstance hands;
    hands.definition_id = "gauntlets";
    equip.equip(EquipmentSlot::Hands, hands);

    ItemInstance legs;
    legs.definition_id = "greaves";
    equip.equip(EquipmentSlot::Legs, legs);

    ItemInstance feet;
    feet.definition_id = "boots";
    equip.equip(EquipmentSlot::Feet, feet);

    ItemInstance neck;
    neck.definition_id = "amulet";
    equip.equip(EquipmentSlot::Neck, neck);

    ItemInstance ring1;
    ring1.definition_id = "ring_power";
    equip.equip(EquipmentSlot::Ring1, ring1);

    ItemInstance ring2;
    ring2.definition_id = "ring_defense";
    equip.equip(EquipmentSlot::Ring2, ring2);

    REQUIRE(equip.count_equipped() == 10);
    REQUIRE(equip.get_equipped(EquipmentSlot::MainHand)->definition_id == "sword");
    REQUIRE(equip.get_equipped(EquipmentSlot::Ring2)->definition_id == "ring_defense");
}
