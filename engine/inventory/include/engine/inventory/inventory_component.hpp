#pragma once

#include <engine/inventory/item_instance.hpp>
#include <engine/scene/entity.hpp>
#include <vector>
#include <optional>
#include <functional>

namespace engine::inventory {

// ============================================================================
// Inventory Slot
// ============================================================================

struct InventorySlot {
    std::optional<ItemInstance> item;
    bool is_locked = false;         // Prevent modifications
    bool is_favorite = false;       // Marked as favorite

    bool is_empty() const { return !item.has_value(); }
    bool has_item() const { return item.has_value(); }
};

// ============================================================================
// InventoryComponent - ECS component for item storage
// ============================================================================

struct InventoryComponent {
    std::vector<InventorySlot> slots;
    int max_slots = 40;
    float max_weight = 0.0f;        // 0 = unlimited
    bool auto_sort = false;

    // Currency (separate from items)
    std::unordered_map<std::string, int64_t> currencies;  // "gold" -> 1000

    // ========================================================================
    // Slot Access
    // ========================================================================

    void resize(int new_size);
    int get_slot_count() const { return static_cast<int>(slots.size()); }

    InventorySlot* get_slot(int index);
    const InventorySlot* get_slot(int index) const;

    ItemInstance* get_item(int index);
    const ItemInstance* get_item(int index) const;

    // ========================================================================
    // Queries
    // ========================================================================

    // Count items
    int count_item(const std::string& item_id) const;
    int count_total_items() const;
    int count_empty_slots() const;
    int count_used_slots() const;

    // Find items
    int find_item(const std::string& item_id) const;                    // First slot with item
    int find_item_instance(const core::UUID& instance_id) const;        // Find by instance ID
    std::vector<int> find_all_items(const std::string& item_id) const;  // All slots with item
    std::vector<int> find_items_by_type(ItemType type) const;
    std::vector<int> find_items_by_tag(const std::string& tag) const;

    // Find empty slot
    int find_empty_slot() const;

    // Find slot that can accept more of this item (for stacking)
    int find_stackable_slot(const std::string& item_id) const;

    // Check capacity
    bool has_item(const std::string& item_id, int count = 1) const;
    bool has_space_for(const ItemInstance& item) const;
    bool can_add(const std::string& item_id, int count = 1) const;

    // Weight
    float get_current_weight() const;
    float get_weight_capacity() const { return max_weight; }
    bool is_over_weight() const;
    float get_weight_percent() const;

    // ========================================================================
    // Modification
    // ========================================================================

    // Add item, returns slot index or -1 if failed
    int add_item(const ItemInstance& item);
    int add_item(const std::string& item_id, int count = 1);

    // Add to specific slot
    bool add_to_slot(int index, const ItemInstance& item);

    // Remove item
    bool remove_item(int index, int count = -1);  // -1 = all
    bool remove_item_by_id(const std::string& item_id, int count = 1);
    ItemInstance take_item(int index, int count = -1);  // Remove and return

    // Move/swap
    bool move_item(int from_index, int to_index);
    bool swap_items(int index_a, int index_b);

    // Split stack
    bool split_stack(int index, int amount, int target_slot = -1);

    // Merge stacks
    bool merge_stacks(int from_index, int to_index);

    // ========================================================================
    // Currency
    // ========================================================================

    int64_t get_currency(const std::string& currency_id) const;
    void set_currency(const std::string& currency_id, int64_t amount);
    void add_currency(const std::string& currency_id, int64_t amount);
    bool spend_currency(const std::string& currency_id, int64_t amount);
    bool can_afford(const std::string& currency_id, int64_t amount) const;

    // ========================================================================
    // Sorting
    // ========================================================================

    void sort_by_type();
    void sort_by_name();
    void sort_by_rarity();
    void sort_by_value();
    void sort_custom(std::function<bool(const ItemInstance&, const ItemInstance&)> compare);
    void compact();  // Move all items to front, empty slots to back

    // ========================================================================
    // Serialization helpers
    // ========================================================================

    void clear();
};

// ============================================================================
// Equipment Component
// ============================================================================

struct EquipmentComponent {
    std::array<std::optional<ItemInstance>, static_cast<size_t>(EquipmentSlot::Count)> slots;

    // ========================================================================
    // Access
    // ========================================================================

    ItemInstance* get_equipped(EquipmentSlot slot);
    const ItemInstance* get_equipped(EquipmentSlot slot) const;

    bool has_equipped(EquipmentSlot slot) const;
    bool is_slot_empty(EquipmentSlot slot) const;

    // ========================================================================
    // Equip/Unequip
    // ========================================================================

    // Returns previously equipped item (if any)
    std::optional<ItemInstance> equip(EquipmentSlot slot, const ItemInstance& item);
    std::optional<ItemInstance> unequip(EquipmentSlot slot);
    void unequip_all();

    // ========================================================================
    // Stats
    // ========================================================================

    // Get all stat modifiers from equipped items
    std::vector<stats::StatModifier> get_all_equipment_modifiers() const;

    // Get total bonus for a stat from all equipment
    float get_total_stat_bonus(stats::StatType stat) const;

    // ========================================================================
    // Queries
    // ========================================================================

    std::vector<EquipmentSlot> get_occupied_slots() const;
    std::vector<EquipmentSlot> get_empty_slots() const;
    int count_equipped() const;

    // Check if can equip (slot compatibility)
    bool can_equip(EquipmentSlot slot, const ItemInstance& item) const;
};

// ============================================================================
// Component Registration
// ============================================================================

void register_inventory_components();

} // namespace engine::inventory
