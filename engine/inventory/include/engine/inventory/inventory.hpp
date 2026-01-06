#pragma once

// ============================================================================
// Engine Inventory System - Umbrella Header
// ============================================================================
//
// Complete inventory system for item management, equipment, and currencies.
//
// Quick Start:
// ------------
// 1. Register item definitions:
//    item()
//        .id("health_potion")
//        .name("Health Potion")
//        .type(ItemType::Consumable)
//        .rarity(ItemRarity::Common)
//        .stack(20)
//        .heal(stats::StatType::Health, 50.0f)
//        .register_item();
//
// 2. Give items to entities:
//    inventory().give_item(world, player, "health_potion", 5);
//
// 3. Equip items:
//    inventory().equip_from_inventory(world, player, slot_index, EquipmentSlot::MainHand);
//
// 4. Use items:
//    inventory().use_item(world, player, slot_index);
//
// Components:
// -----------
// - InventoryComponent: Item storage slots and currencies
// - EquipmentComponent: Equipment slots for gear
//
// Systems:
// --------
// - equipment_system: Updates equipment stat modifiers
//
// ============================================================================

#include <engine/inventory/item_definition.hpp>
#include <engine/inventory/item_instance.hpp>
#include <engine/inventory/inventory_component.hpp>
#include <engine/inventory/inventory_manager.hpp>
#include <engine/inventory/inventory_events.hpp>
