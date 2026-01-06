#pragma once

#include <engine/inventory/inventory_component.hpp>
#include <engine/scene/entity.hpp>
#include <engine/scene/world.hpp>
#include <functional>
#include <string>

namespace engine::inventory {

// ============================================================================
// Transfer Result
// ============================================================================

enum class TransferResult : uint8_t {
    Success,
    PartialSuccess,     // Some items transferred
    SourceEmpty,
    TargetFull,
    ItemNotFound,
    InvalidSource,
    InvalidTarget,
    NotEnoughCurrency,
    RequirementsNotMet,
    Failed
};

// ============================================================================
// Use Result
// ============================================================================

enum class UseResult : uint8_t {
    Used,               // Successfully used
    PartialUse,         // Used but item remains (stackable)
    Equipped,           // Was equipment, now equipped
    CannotUse,          // Item not usable
    RequirementsNotMet, // Character doesn't meet requirements
    OnCooldown,         // Item/effect on cooldown
    InvalidTarget,      // Invalid use target
    Failed
};

// ============================================================================
// Inventory Manager
// ============================================================================

class InventoryManager {
public:
    static InventoryManager& instance();

    // Delete copy/move
    InventoryManager(const InventoryManager&) = delete;
    InventoryManager& operator=(const InventoryManager&) = delete;

    // ========================================================================
    // Item Operations
    // ========================================================================

    // Give item to entity
    TransferResult give_item(scene::World& world, scene::Entity entity,
                             const std::string& item_id, int count = 1);
    TransferResult give_item(scene::World& world, scene::Entity entity,
                             const ItemInstance& item);

    // Take item from entity
    TransferResult take_item(scene::World& world, scene::Entity entity,
                             const std::string& item_id, int count = 1);
    TransferResult take_item(scene::World& world, scene::Entity entity,
                             int slot_index, int count = -1);

    // Transfer between entities
    TransferResult transfer(scene::World& world,
                            scene::Entity from, scene::Entity to,
                            int from_slot, int count = -1);
    TransferResult transfer_all(scene::World& world,
                                scene::Entity from, scene::Entity to);

    // ========================================================================
    // Item Usage
    // ========================================================================

    // Use item (consume, equip, etc.)
    UseResult use_item(scene::World& world, scene::Entity entity, int slot_index);
    UseResult use_item(scene::World& world, scene::Entity entity,
                       int slot_index, scene::Entity target);

    // ========================================================================
    // Equipment
    // ========================================================================

    // Equip item from inventory
    bool equip_from_inventory(scene::World& world, scene::Entity entity,
                              int inventory_slot, EquipmentSlot equip_slot = EquipmentSlot::None);

    // Unequip to inventory
    bool unequip_to_inventory(scene::World& world, scene::Entity entity,
                              EquipmentSlot slot);

    // Swap equipment with inventory
    bool swap_equipment(scene::World& world, scene::Entity entity,
                        int inventory_slot, EquipmentSlot equip_slot);

    // Check if can equip (meets requirements)
    bool can_equip(scene::World& world, scene::Entity entity,
                   const ItemInstance& item) const;

    // ========================================================================
    // Currency
    // ========================================================================

    bool give_currency(scene::World& world, scene::Entity entity,
                       const std::string& currency_id, int64_t amount);
    bool take_currency(scene::World& world, scene::Entity entity,
                       const std::string& currency_id, int64_t amount);
    bool transfer_currency(scene::World& world,
                           scene::Entity from, scene::Entity to,
                           const std::string& currency_id, int64_t amount);

    // ========================================================================
    // Queries
    // ========================================================================

    bool has_item(scene::World& world, scene::Entity entity,
                  const std::string& item_id, int count = 1) const;
    int count_item(scene::World& world, scene::Entity entity,
                   const std::string& item_id) const;
    bool has_equipment_in_slot(scene::World& world, scene::Entity entity,
                               EquipmentSlot slot) const;

    // ========================================================================
    // Loot
    // ========================================================================

    // Generate and give loot
    std::vector<ItemInstance> generate_loot(const std::string& loot_table_id,
                                            int player_level = 1,
                                            float luck_bonus = 0.0f);
    TransferResult give_loot(scene::World& world, scene::Entity entity,
                             const std::string& loot_table_id,
                             int player_level = 1, float luck_bonus = 0.0f);

    // ========================================================================
    // Callbacks
    // ========================================================================

    using ItemCallback = std::function<void(scene::World&, scene::Entity, const ItemInstance&)>;
    using SlotCallback = std::function<void(scene::World&, scene::Entity, int slot, const ItemInstance&)>;
    using EquipCallback = std::function<void(scene::World&, scene::Entity, EquipmentSlot, const ItemInstance&)>;
    using CurrencyCallback = std::function<void(scene::World&, scene::Entity, const std::string&, int64_t)>;

    void set_on_item_added(SlotCallback callback);
    void set_on_item_removed(SlotCallback callback);
    void set_on_item_used(ItemCallback callback);
    void set_on_equipped(EquipCallback callback);
    void set_on_unequipped(EquipCallback callback);
    void set_on_currency_changed(CurrencyCallback callback);

private:
    InventoryManager() = default;
    ~InventoryManager() = default;

    // Internal helpers
    void apply_equipment_stats(scene::World& world, scene::Entity entity, const ItemInstance& item);
    void remove_equipment_stats(scene::World& world, scene::Entity entity, const ItemInstance& item);
    bool check_requirements(scene::World& world, scene::Entity entity, const ItemDefinition* def) const;

    SlotCallback m_on_item_added;
    SlotCallback m_on_item_removed;
    ItemCallback m_on_item_used;
    EquipCallback m_on_equipped;
    EquipCallback m_on_unequipped;
    CurrencyCallback m_on_currency_changed;
};

// ============================================================================
// Global Access
// ============================================================================

inline InventoryManager& inventory() { return InventoryManager::instance(); }

// ============================================================================
// ECS Systems
// ============================================================================

// Update equipment stats (call when equipment changes)
void equipment_system(scene::World& world, double dt);

} // namespace engine::inventory
