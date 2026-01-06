#pragma once

#include <engine/inventory/item_instance.hpp>
#include <engine/inventory/item_definition.hpp>
#include <engine/scene/entity.hpp>
#include <string>
#include <cstdint>

namespace engine::inventory {

// ============================================================================
// Item Added Event
// ============================================================================

struct ItemAddedEvent {
    scene::Entity entity;
    int slot_index;
    ItemInstance item;
    std::string source;             // "loot", "purchase", "craft", "quest", etc.
};

// ============================================================================
// Item Removed Event
// ============================================================================

struct ItemRemovedEvent {
    scene::Entity entity;
    int slot_index;
    ItemInstance item;
    int count_removed;
    std::string reason;             // "used", "sold", "dropped", "destroyed", etc.
};

// ============================================================================
// Item Moved Event
// ============================================================================

struct ItemMovedEvent {
    scene::Entity entity;
    int from_slot;
    int to_slot;
    ItemInstance item;
};

// ============================================================================
// Item Transferred Event
// ============================================================================

struct ItemTransferredEvent {
    scene::Entity from_entity;
    scene::Entity to_entity;
    int from_slot;
    int to_slot;
    ItemInstance item;
    int count;
};

// ============================================================================
// Item Used Event
// ============================================================================

struct ItemUsedEvent {
    scene::Entity entity;
    scene::Entity target;           // May be same as entity for self-use
    int slot_index;
    ItemInstance item;
    bool destroyed;                 // Was the item consumed?
};

// ============================================================================
// Item Equipped Event
// ============================================================================

struct ItemEquippedEvent {
    scene::Entity entity;
    EquipmentSlot slot;
    ItemInstance item;
    std::optional<ItemInstance> previous_item;  // What was unequipped
};

// ============================================================================
// Item Unequipped Event
// ============================================================================

struct ItemUnequippedEvent {
    scene::Entity entity;
    EquipmentSlot slot;
    ItemInstance item;
    bool moved_to_inventory;
    int inventory_slot;             // -1 if not moved to inventory
};

// ============================================================================
// Currency Changed Event
// ============================================================================

struct CurrencyChangedEvent {
    scene::Entity entity;
    std::string currency_id;
    int64_t old_amount;
    int64_t new_amount;
    int64_t delta;
    std::string reason;             // "purchase", "sale", "loot", "reward", etc.
};

// ============================================================================
// Inventory Full Event
// ============================================================================

struct InventoryFullEvent {
    scene::Entity entity;
    ItemInstance failed_item;       // Item that couldn't be added
    int available_slots;
};

// ============================================================================
// Item Durability Changed Event
// ============================================================================

struct DurabilityChangedEvent {
    scene::Entity entity;
    int slot_index;                 // -1 if equipped
    EquipmentSlot equip_slot;       // None if in inventory
    ItemInstance item;
    int old_durability;
    int new_durability;
};

// ============================================================================
// Item Broken Event
// ============================================================================

struct ItemBrokenEvent {
    scene::Entity entity;
    int slot_index;
    EquipmentSlot equip_slot;
    ItemInstance item;
    bool destroyed;                 // Was the item destroyed or just broken?
};

// ============================================================================
// Item Repaired Event
// ============================================================================

struct ItemRepairedEvent {
    scene::Entity entity;
    int slot_index;
    EquipmentSlot equip_slot;
    ItemInstance item;
    int old_durability;
    int new_durability;
};

// ============================================================================
// Loot Generated Event
// ============================================================================

struct LootGeneratedEvent {
    std::string loot_table_id;
    scene::Entity source;           // Entity that dropped loot
    scene::Entity recipient;        // Entity receiving loot
    std::vector<ItemInstance> items;
    int player_level;
    float luck_bonus;
};

// ============================================================================
// Item Stack Changed Event
// ============================================================================

struct ItemStackChangedEvent {
    scene::Entity entity;
    int slot_index;
    ItemInstance item;
    int old_count;
    int new_count;
};

// ============================================================================
// Inventory Sorted Event
// ============================================================================

struct InventorySortedEvent {
    scene::Entity entity;
    std::string sort_type;          // "type", "name", "rarity", "value", "custom"
};

// ============================================================================
// Equipment Stats Changed Event
// ============================================================================

struct EquipmentStatsChangedEvent {
    scene::Entity entity;
    std::vector<stats::StatModifier> added_modifiers;
    std::vector<stats::StatModifier> removed_modifiers;
};

// ============================================================================
// Item Acquired Event (higher-level than ItemAdded)
// ============================================================================

struct ItemAcquiredEvent {
    scene::Entity entity;
    ItemInstance item;
    std::string acquisition_type;   // "pickup", "loot", "purchase", "craft", "quest", "trade"
    scene::Entity source;           // Chest, enemy, NPC, etc.
};

// ============================================================================
// Event Registration
// ============================================================================

void register_inventory_events();

} // namespace engine::inventory
