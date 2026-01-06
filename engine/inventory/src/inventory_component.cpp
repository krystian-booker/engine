#include <engine/inventory/inventory_component.hpp>
#include <algorithm>

namespace engine::inventory {

// ============================================================================
// InventoryComponent
// ============================================================================

void InventoryComponent::resize(int new_size) {
    slots.resize(static_cast<size_t>(new_size));
}

InventorySlot* InventoryComponent::get_slot(int index) {
    if (index < 0 || index >= static_cast<int>(slots.size())) {
        return nullptr;
    }
    return &slots[static_cast<size_t>(index)];
}

const InventorySlot* InventoryComponent::get_slot(int index) const {
    if (index < 0 || index >= static_cast<int>(slots.size())) {
        return nullptr;
    }
    return &slots[static_cast<size_t>(index)];
}

ItemInstance* InventoryComponent::get_item(int index) {
    auto* slot = get_slot(index);
    if (slot && slot->has_item()) {
        return &slot->item.value();
    }
    return nullptr;
}

const ItemInstance* InventoryComponent::get_item(int index) const {
    const auto* slot = get_slot(index);
    if (slot && slot->has_item()) {
        return &slot->item.value();
    }
    return nullptr;
}

// ============================================================================
// Queries
// ============================================================================

int InventoryComponent::count_item(const std::string& item_id) const {
    int count = 0;
    for (const auto& slot : slots) {
        if (slot.has_item() && slot.item->definition_id == item_id) {
            count += slot.item->stack_count;
        }
    }
    return count;
}

int InventoryComponent::count_total_items() const {
    int count = 0;
    for (const auto& slot : slots) {
        if (slot.has_item()) {
            count += slot.item->stack_count;
        }
    }
    return count;
}

int InventoryComponent::count_empty_slots() const {
    int count = 0;
    for (const auto& slot : slots) {
        if (slot.is_empty()) {
            ++count;
        }
    }
    return count;
}

int InventoryComponent::count_used_slots() const {
    return static_cast<int>(slots.size()) - count_empty_slots();
}

int InventoryComponent::find_item(const std::string& item_id) const {
    for (size_t i = 0; i < slots.size(); ++i) {
        if (slots[i].has_item() && slots[i].item->definition_id == item_id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int InventoryComponent::find_item_instance(const core::UUID& instance_id) const {
    for (size_t i = 0; i < slots.size(); ++i) {
        if (slots[i].has_item() && slots[i].item->instance_id == instance_id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::vector<int> InventoryComponent::find_all_items(const std::string& item_id) const {
    std::vector<int> result;
    for (size_t i = 0; i < slots.size(); ++i) {
        if (slots[i].has_item() && slots[i].item->definition_id == item_id) {
            result.push_back(static_cast<int>(i));
        }
    }
    return result;
}

std::vector<int> InventoryComponent::find_items_by_type(ItemType type) const {
    std::vector<int> result;
    for (size_t i = 0; i < slots.size(); ++i) {
        if (slots[i].has_item()) {
            const auto* def = slots[i].item->get_definition();
            if (def && def->type == type) {
                result.push_back(static_cast<int>(i));
            }
        }
    }
    return result;
}

std::vector<int> InventoryComponent::find_items_by_tag(const std::string& tag) const {
    std::vector<int> result;
    for (size_t i = 0; i < slots.size(); ++i) {
        if (slots[i].has_item()) {
            const auto* def = slots[i].item->get_definition();
            if (def && def->has_tag(tag)) {
                result.push_back(static_cast<int>(i));
            }
        }
    }
    return result;
}

int InventoryComponent::find_empty_slot() const {
    for (size_t i = 0; i < slots.size(); ++i) {
        if (slots[i].is_empty() && !slots[i].is_locked) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int InventoryComponent::find_stackable_slot(const std::string& item_id) const {
    for (size_t i = 0; i < slots.size(); ++i) {
        if (slots[i].has_item() &&
            slots[i].item->definition_id == item_id &&
            slots[i].item->get_stack_space() > 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool InventoryComponent::has_item(const std::string& item_id, int count) const {
    return count_item(item_id) >= count;
}

bool InventoryComponent::has_space_for(const ItemInstance& item) const {
    // Check if we can stack with existing
    if (item.is_stackable()) {
        int remaining = item.stack_count;
        for (const auto& slot : slots) {
            if (slot.has_item() && slot.item->can_stack_with(item)) {
                remaining -= slot.item->get_stack_space();
                if (remaining <= 0) return true;
            }
        }
        // Need empty slots for the rest
        const auto* def = item.get_definition();
        int slots_needed = (remaining + (def ? def->max_stack : 1) - 1) / (def ? def->max_stack : 1);
        return count_empty_slots() >= slots_needed;
    }

    // Non-stackable: need one empty slot
    return find_empty_slot() >= 0;
}

bool InventoryComponent::can_add(const std::string& item_id, int count) const {
    ItemInstance temp = ItemInstance::create(item_id, count);
    return has_space_for(temp);
}

float InventoryComponent::get_current_weight() const {
    float weight = 0.0f;
    for (const auto& slot : slots) {
        if (slot.has_item()) {
            const auto* def = slot.item->get_definition();
            if (def) {
                weight += def->weight * static_cast<float>(slot.item->stack_count);
            }
        }
    }
    return weight;
}

bool InventoryComponent::is_over_weight() const {
    return max_weight > 0.0f && get_current_weight() > max_weight;
}

float InventoryComponent::get_weight_percent() const {
    if (max_weight <= 0.0f) return 0.0f;
    return get_current_weight() / max_weight;
}

// ============================================================================
// Modification
// ============================================================================

int InventoryComponent::add_item(const ItemInstance& item) {
    // Ensure we have slots
    if (slots.empty()) {
        resize(max_slots);
    }

    // Try to stack with existing items first
    if (item.is_stackable()) {
        int remaining = item.stack_count;
        ItemInstance temp = item;

        for (auto& slot : slots) {
            if (slot.has_item() && slot.item->can_stack_with(temp)) {
                int overflow = slot.item->add_stack(remaining);
                remaining = overflow;
                if (remaining <= 0) break;
            }
        }

        if (remaining <= 0) {
            // All stacked, return any slot with the item
            return find_item(item.definition_id);
        }

        // Need new slots for the rest
        temp.stack_count = remaining;
        int slot_idx = find_empty_slot();
        if (slot_idx >= 0) {
            slots[static_cast<size_t>(slot_idx)].item = temp;
            return slot_idx;
        }
        return -1;  // No space
    }

    // Non-stackable: find empty slot
    int slot_idx = find_empty_slot();
    if (slot_idx >= 0) {
        slots[static_cast<size_t>(slot_idx)].item = item;
        return slot_idx;
    }
    return -1;
}

int InventoryComponent::add_item(const std::string& item_id, int count) {
    ItemInstance item = ItemInstance::create(item_id, count);
    return add_item(item);
}

bool InventoryComponent::add_to_slot(int index, const ItemInstance& item) {
    auto* slot = get_slot(index);
    if (!slot || slot->is_locked) return false;

    if (slot->is_empty()) {
        slot->item = item;
        return true;
    }

    // Try to stack
    if (slot->item->can_stack_with(item)) {
        slot->item->add_stack(item.stack_count);
        return true;
    }

    return false;
}

bool InventoryComponent::remove_item(int index, int count) {
    auto* slot = get_slot(index);
    if (!slot || !slot->has_item() || slot->is_locked) return false;

    if (count < 0 || count >= slot->item->stack_count) {
        // Remove all
        slot->item.reset();
    } else {
        slot->item->remove_stack(count);
    }
    return true;
}

bool InventoryComponent::remove_item_by_id(const std::string& item_id, int count) {
    int remaining = count;

    for (auto& slot : slots) {
        if (slot.has_item() && slot.item->definition_id == item_id && !slot.is_locked) {
            int to_remove = std::min(remaining, slot.item->stack_count);
            slot.item->remove_stack(to_remove);
            remaining -= to_remove;

            if (slot.item->stack_count <= 0) {
                slot.item.reset();
            }

            if (remaining <= 0) break;
        }
    }

    return remaining <= 0;
}

ItemInstance InventoryComponent::take_item(int index, int count) {
    auto* slot = get_slot(index);
    if (!slot || !slot->has_item() || slot->is_locked) {
        return ItemInstance{};
    }

    if (count < 0 || count >= slot->item->stack_count) {
        ItemInstance result = std::move(slot->item.value());
        slot->item.reset();
        return result;
    } else {
        return slot->item->split(count);
    }
}

bool InventoryComponent::move_item(int from_index, int to_index) {
    auto* from_slot = get_slot(from_index);
    auto* to_slot = get_slot(to_index);

    if (!from_slot || !to_slot) return false;
    if (!from_slot->has_item()) return false;
    if (from_slot->is_locked || to_slot->is_locked) return false;

    if (to_slot->is_empty()) {
        to_slot->item = std::move(from_slot->item);
        from_slot->item.reset();
        return true;
    }

    // Try to stack
    if (to_slot->item->can_stack_with(from_slot->item.value())) {
        int overflow = to_slot->item->add_stack(from_slot->item->stack_count);
        if (overflow <= 0) {
            from_slot->item.reset();
        } else {
            from_slot->item->stack_count = overflow;
        }
        return true;
    }

    return false;
}

bool InventoryComponent::swap_items(int index_a, int index_b) {
    auto* slot_a = get_slot(index_a);
    auto* slot_b = get_slot(index_b);

    if (!slot_a || !slot_b) return false;
    if (slot_a->is_locked || slot_b->is_locked) return false;

    std::swap(slot_a->item, slot_b->item);
    return true;
}

bool InventoryComponent::split_stack(int index, int amount, int target_slot) {
    auto* slot = get_slot(index);
    if (!slot || !slot->has_item() || slot->is_locked) return false;
    if (slot->item->stack_count <= 1) return false;
    if (amount <= 0 || amount >= slot->item->stack_count) return false;

    ItemInstance split_item = slot->item->split(amount);

    if (target_slot >= 0) {
        auto* target = get_slot(target_slot);
        if (target && target->is_empty() && !target->is_locked) {
            target->item = std::move(split_item);
            return true;
        }
        return false;  // Can't use target slot
    }

    // Find empty slot
    int empty = find_empty_slot();
    if (empty >= 0) {
        slots[static_cast<size_t>(empty)].item = std::move(split_item);
        return true;
    }

    // No space, put it back
    slot->item->add_stack(split_item.stack_count);
    return false;
}

bool InventoryComponent::merge_stacks(int from_index, int to_index) {
    auto* from_slot = get_slot(from_index);
    auto* to_slot = get_slot(to_index);

    if (!from_slot || !to_slot) return false;
    if (!from_slot->has_item() || !to_slot->has_item()) return false;
    if (from_slot->is_locked || to_slot->is_locked) return false;
    if (!to_slot->item->can_stack_with(from_slot->item.value())) return false;

    int overflow = to_slot->item->add_stack(from_slot->item->stack_count);
    if (overflow <= 0) {
        from_slot->item.reset();
    } else {
        from_slot->item->stack_count = overflow;
    }

    return true;
}

// ============================================================================
// Currency
// ============================================================================

int64_t InventoryComponent::get_currency(const std::string& currency_id) const {
    auto it = currencies.find(currency_id);
    if (it != currencies.end()) {
        return it->second;
    }
    return 0;
}

void InventoryComponent::set_currency(const std::string& currency_id, int64_t amount) {
    currencies[currency_id] = std::max(int64_t(0), amount);
}

void InventoryComponent::add_currency(const std::string& currency_id, int64_t amount) {
    currencies[currency_id] = std::max(int64_t(0), currencies[currency_id] + amount);
}

bool InventoryComponent::spend_currency(const std::string& currency_id, int64_t amount) {
    if (!can_afford(currency_id, amount)) return false;
    currencies[currency_id] -= amount;
    return true;
}

bool InventoryComponent::can_afford(const std::string& currency_id, int64_t amount) const {
    return get_currency(currency_id) >= amount;
}

// ============================================================================
// Sorting
// ============================================================================

void InventoryComponent::sort_by_type() {
    sort_custom([](const ItemInstance& a, const ItemInstance& b) {
        const auto* def_a = a.get_definition();
        const auto* def_b = b.get_definition();
        if (!def_a || !def_b) return false;
        return static_cast<int>(def_a->type) < static_cast<int>(def_b->type);
    });
}

void InventoryComponent::sort_by_name() {
    sort_custom([](const ItemInstance& a, const ItemInstance& b) {
        return a.get_display_name() < b.get_display_name();
    });
}

void InventoryComponent::sort_by_rarity() {
    sort_custom([](const ItemInstance& a, const ItemInstance& b) {
        const auto* def_a = a.get_definition();
        const auto* def_b = b.get_definition();
        if (!def_a || !def_b) return false;
        return static_cast<int>(def_a->rarity) > static_cast<int>(def_b->rarity);  // Higher rarity first
    });
}

void InventoryComponent::sort_by_value() {
    sort_custom([](const ItemInstance& a, const ItemInstance& b) {
        const auto* def_a = a.get_definition();
        const auto* def_b = b.get_definition();
        if (!def_a || !def_b) return false;
        return def_a->base_value > def_b->base_value;  // Higher value first
    });
}

void InventoryComponent::sort_custom(std::function<bool(const ItemInstance&, const ItemInstance&)> compare) {
    // Collect all items
    std::vector<ItemInstance> items;
    for (auto& slot : slots) {
        if (slot.has_item() && !slot.is_locked) {
            items.push_back(std::move(slot.item.value()));
            slot.item.reset();
        }
    }

    // Sort
    std::sort(items.begin(), items.end(), compare);

    // Put back
    size_t item_idx = 0;
    for (auto& slot : slots) {
        if (!slot.is_locked && item_idx < items.size()) {
            slot.item = std::move(items[item_idx++]);
        }
    }
}

void InventoryComponent::compact() {
    // Move all items to front
    size_t write_idx = 0;
    for (size_t read_idx = 0; read_idx < slots.size(); ++read_idx) {
        if (slots[read_idx].has_item() && !slots[read_idx].is_locked) {
            if (write_idx != read_idx) {
                // Find first non-locked empty slot
                while (write_idx < read_idx && (slots[write_idx].has_item() || slots[write_idx].is_locked)) {
                    ++write_idx;
                }
                if (write_idx < read_idx && !slots[write_idx].is_locked) {
                    slots[write_idx].item = std::move(slots[read_idx].item);
                    slots[read_idx].item.reset();
                }
            }
            ++write_idx;
        }
    }
}

void InventoryComponent::clear() {
    for (auto& slot : slots) {
        if (!slot.is_locked) {
            slot.item.reset();
        }
    }
    currencies.clear();
}

// ============================================================================
// EquipmentComponent
// ============================================================================

ItemInstance* EquipmentComponent::get_equipped(EquipmentSlot slot) {
    size_t idx = static_cast<size_t>(slot);
    if (idx >= slots.size()) return nullptr;
    if (slots[idx].has_value()) {
        return &slots[idx].value();
    }
    return nullptr;
}

const ItemInstance* EquipmentComponent::get_equipped(EquipmentSlot slot) const {
    size_t idx = static_cast<size_t>(slot);
    if (idx >= slots.size()) return nullptr;
    if (slots[idx].has_value()) {
        return &slots[idx].value();
    }
    return nullptr;
}

bool EquipmentComponent::has_equipped(EquipmentSlot slot) const {
    return get_equipped(slot) != nullptr;
}

bool EquipmentComponent::is_slot_empty(EquipmentSlot slot) const {
    return !has_equipped(slot);
}

std::optional<ItemInstance> EquipmentComponent::equip(EquipmentSlot slot, const ItemInstance& item) {
    size_t idx = static_cast<size_t>(slot);
    if (idx >= slots.size()) return std::nullopt;

    std::optional<ItemInstance> previous = std::move(slots[idx]);
    slots[idx] = item;
    return previous;
}

std::optional<ItemInstance> EquipmentComponent::unequip(EquipmentSlot slot) {
    size_t idx = static_cast<size_t>(slot);
    if (idx >= slots.size()) return std::nullopt;

    std::optional<ItemInstance> result = std::move(slots[idx]);
    slots[idx].reset();
    return result;
}

void EquipmentComponent::unequip_all() {
    for (auto& slot : slots) {
        slot.reset();
    }
}

std::vector<stats::StatModifier> EquipmentComponent::get_all_equipment_modifiers() const {
    std::vector<stats::StatModifier> result;
    for (const auto& slot : slots) {
        if (slot.has_value()) {
            auto mods = slot->get_all_modifiers();
            result.insert(result.end(), mods.begin(), mods.end());
        }
    }
    return result;
}

float EquipmentComponent::get_total_stat_bonus(stats::StatType stat) const {
    float total = 0.0f;
    for (const auto& slot : slots) {
        if (slot.has_value()) {
            total += slot->get_stat_bonus(stat);
        }
    }
    return total;
}

std::vector<EquipmentSlot> EquipmentComponent::get_occupied_slots() const {
    std::vector<EquipmentSlot> result;
    for (size_t i = 0; i < slots.size(); ++i) {
        if (slots[i].has_value()) {
            result.push_back(static_cast<EquipmentSlot>(i));
        }
    }
    return result;
}

std::vector<EquipmentSlot> EquipmentComponent::get_empty_slots() const {
    std::vector<EquipmentSlot> result;
    for (size_t i = 0; i < slots.size(); ++i) {
        if (!slots[i].has_value()) {
            result.push_back(static_cast<EquipmentSlot>(i));
        }
    }
    return result;
}

int EquipmentComponent::count_equipped() const {
    int count = 0;
    for (const auto& slot : slots) {
        if (slot.has_value()) ++count;
    }
    return count;
}

bool EquipmentComponent::can_equip(EquipmentSlot slot, const ItemInstance& item) const {
    const auto* def = item.get_definition();
    if (!def) return false;
    if (!def->is_equipment()) return false;

    // Check if item's slot matches
    EquipmentSlot item_slot = def->slot;

    // Direct match
    if (item_slot == slot) return true;

    // Two-hand weapons can go in main hand
    if (item_slot == EquipmentSlot::TwoHand && slot == EquipmentSlot::MainHand) {
        return true;
    }

    // Rings can go in either ring slot
    if ((item_slot == EquipmentSlot::Ring1 || item_slot == EquipmentSlot::Ring2) &&
        (slot == EquipmentSlot::Ring1 || slot == EquipmentSlot::Ring2)) {
        return true;
    }

    // Accessories can go in either accessory slot
    if ((item_slot == EquipmentSlot::Accessory1 || item_slot == EquipmentSlot::Accessory2) &&
        (slot == EquipmentSlot::Accessory1 || slot == EquipmentSlot::Accessory2)) {
        return true;
    }

    return false;
}

} // namespace engine::inventory
