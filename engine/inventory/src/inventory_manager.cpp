#include <engine/inventory/inventory_manager.hpp>
#include <engine/inventory/inventory_events.hpp>
#include <engine/core/log.hpp>
#include <engine/core/game_events.hpp>
#include <engine/stats/stat_component.hpp>

namespace engine::inventory {

// ============================================================================
// InventoryManager Singleton
// ============================================================================

InventoryManager& InventoryManager::instance() {
    static InventoryManager s_instance;
    return s_instance;
}

// ============================================================================
// Item Operations
// ============================================================================

TransferResult InventoryManager::give_item(scene::World& world, scene::Entity entity,
                                           const std::string& item_id, int count) {
    if (!item_registry().exists(item_id)) {
        core::log(core::LogLevel::Warn, "[Inventory] Cannot give unknown item: {}", item_id);
        return TransferResult::Failed;
    }

    ItemInstance item = ItemInstance::create(item_id, count);
    return give_item(world, entity, item);
}

TransferResult InventoryManager::give_item(scene::World& world, scene::Entity entity,
                                           const ItemInstance& item) {
    auto* inv = world.try_get<InventoryComponent>(entity);
    if (!inv) {
        core::log(core::LogLevel::Warn, "[Inventory] Entity has no InventoryComponent");
        return TransferResult::InvalidTarget;
    }

    if (!inv->has_space_for(item)) {
        // Fire inventory full event
        InventoryFullEvent event;
        event.entity = entity;
        event.failed_item = item;
        event.available_slots = inv->count_empty_slots();
        core::game_events().broadcast(event);

        return TransferResult::TargetFull;
    }

    int slot_index = inv->add_item(item);
    if (slot_index < 0) {
        return TransferResult::Failed;
    }

    // Fire item added event
    if (m_on_item_added) {
        m_on_item_added(world, entity, slot_index, item);
    }

    ItemAddedEvent event;
    event.entity = entity;
    event.slot_index = slot_index;
    event.item = item;
    event.source = "give";
    core::game_events().broadcast(event);

    return TransferResult::Success;
}

TransferResult InventoryManager::take_item(scene::World& world, scene::Entity entity,
                                           const std::string& item_id, int count) {
    auto* inv = world.try_get<InventoryComponent>(entity);
    if (!inv) {
        return TransferResult::InvalidSource;
    }

    if (!inv->has_item(item_id, count)) {
        return TransferResult::ItemNotFound;
    }

    // Find and remove items
    int remaining = count;
    std::vector<int> slots = inv->find_all_items(item_id);

    for (int slot : slots) {
        const auto* item = inv->get_item(slot);
        if (!item) continue;

        int to_take = std::min(remaining, item->stack_count);
        ItemInstance taken = inv->take_item(slot, to_take);
        remaining -= to_take;

        if (m_on_item_removed) {
            m_on_item_removed(world, entity, slot, taken);
        }

        ItemRemovedEvent event;
        event.entity = entity;
        event.slot_index = slot;
        event.item = taken;
        event.count_removed = to_take;
        event.reason = "take";
        core::game_events().broadcast(event);

        if (remaining <= 0) break;
    }

    return remaining <= 0 ? TransferResult::Success : TransferResult::PartialSuccess;
}

TransferResult InventoryManager::take_item(scene::World& world, scene::Entity entity,
                                           int slot_index, int count) {
    auto* inv = world.try_get<InventoryComponent>(entity);
    if (!inv) {
        return TransferResult::InvalidSource;
    }

    const auto* item = inv->get_item(slot_index);
    if (!item) {
        return TransferResult::ItemNotFound;
    }

    ItemInstance taken = inv->take_item(slot_index, count);

    if (m_on_item_removed) {
        m_on_item_removed(world, entity, slot_index, taken);
    }

    ItemRemovedEvent event;
    event.entity = entity;
    event.slot_index = slot_index;
    event.item = taken;
    event.count_removed = taken.stack_count;
    event.reason = "take";
    core::game_events().broadcast(event);

    return TransferResult::Success;
}

TransferResult InventoryManager::transfer(scene::World& world,
                                          scene::Entity from, scene::Entity to,
                                          int from_slot, int count) {
    auto* from_inv = world.try_get<InventoryComponent>(from);
    auto* to_inv = world.try_get<InventoryComponent>(to);

    if (!from_inv) return TransferResult::InvalidSource;
    if (!to_inv) return TransferResult::InvalidTarget;

    const auto* item = from_inv->get_item(from_slot);
    if (!item) return TransferResult::ItemNotFound;

    int transfer_count = (count < 0) ? item->stack_count : std::min(count, item->stack_count);

    // Check if target has space
    ItemInstance temp = *item;
    temp.stack_count = transfer_count;
    if (!to_inv->has_space_for(temp)) {
        return TransferResult::TargetFull;
    }

    // Take from source
    ItemInstance taken = from_inv->take_item(from_slot, transfer_count);

    // Add to target
    int to_slot = to_inv->add_item(taken);

    // Fire events
    ItemTransferredEvent event;
    event.from_entity = from;
    event.to_entity = to;
    event.from_slot = from_slot;
    event.to_slot = to_slot;
    event.item = taken;
    event.count = taken.stack_count;
    core::game_events().broadcast(event);

    return TransferResult::Success;
}

TransferResult InventoryManager::transfer_all(scene::World& world,
                                              scene::Entity from, scene::Entity to) {
    auto* from_inv = world.try_get<InventoryComponent>(from);
    auto* to_inv = world.try_get<InventoryComponent>(to);

    if (!from_inv) return TransferResult::InvalidSource;
    if (!to_inv) return TransferResult::InvalidTarget;

    bool any_success = false;
    bool any_failure = false;

    for (int i = 0; i < from_inv->get_slot_count(); ++i) {
        if (from_inv->get_item(i)) {
            TransferResult result = transfer(world, from, to, i);
            if (result == TransferResult::Success) {
                any_success = true;
            } else {
                any_failure = true;
            }
        }
    }

    if (any_success && any_failure) return TransferResult::PartialSuccess;
    if (any_success) return TransferResult::Success;
    if (any_failure) return TransferResult::Failed;
    return TransferResult::SourceEmpty;
}

// ============================================================================
// Item Usage
// ============================================================================

UseResult InventoryManager::use_item(scene::World& world, scene::Entity entity, int slot_index) {
    return use_item(world, entity, slot_index, entity);  // Self-use
}

UseResult InventoryManager::use_item(scene::World& world, scene::Entity entity,
                                     int slot_index, scene::Entity target) {
    auto* inv = world.try_get<InventoryComponent>(entity);
    if (!inv) return UseResult::Failed;

    auto* item = inv->get_item(slot_index);
    if (!item) return UseResult::Failed;

    const auto* def = item->get_definition();
    if (!def) return UseResult::Failed;

    // Check requirements
    if (!check_requirements(world, entity, def)) {
        return UseResult::RequirementsNotMet;
    }

    // Handle equipment
    if (def->is_equipment()) {
        if (equip_from_inventory(world, entity, slot_index)) {
            return UseResult::Equipped;
        }
        return UseResult::Failed;
    }

    // Handle consumables
    if (def->type == ItemType::Consumable) {
        // Apply instant heals
        if (auto* stats = world.try_get<stats::StatsComponent>(target)) {
            for (const auto& [stat, amount] : def->instant_heals) {
                stats->modify_current(stat, amount);
            }
        }

        // Apply effects
        // TODO: Integrate with effects system when available
        // for (const auto& effect_id : def->apply_effects) {
        //     effects().apply(world, target, effect_id, entity);
        // }

        // Fire event
        if (m_on_item_used) {
            m_on_item_used(world, entity, *item);
        }

        ItemUsedEvent event;
        event.entity = entity;
        event.target = target;
        event.slot_index = slot_index;
        event.item = *item;

        // Consume the item
        if (def->destroys_on_use) {
            inv->remove_item(slot_index, 1);
            event.destroyed = true;
        } else {
            event.destroyed = false;
        }

        core::game_events().broadcast(event);
        return UseResult::Used;
    }

    return UseResult::CannotUse;
}

// ============================================================================
// Equipment
// ============================================================================

bool InventoryManager::equip_from_inventory(scene::World& world, scene::Entity entity,
                                            int inventory_slot, EquipmentSlot equip_slot) {
    auto* inv = world.try_get<InventoryComponent>(entity);
    auto* equip = world.try_get<EquipmentComponent>(entity);

    if (!inv || !equip) return false;

    auto* item = inv->get_item(inventory_slot);
    if (!item) return false;

    const auto* def = item->get_definition();
    if (!def || !def->is_equipment()) return false;

    // Determine target slot
    EquipmentSlot target_slot = equip_slot;
    if (target_slot == EquipmentSlot::None) {
        target_slot = def->slot;
    }

    // Check if can equip
    if (!equip->can_equip(target_slot, *item)) {
        return false;
    }

    if (!can_equip(world, entity, *item)) {
        return false;
    }

    // Take item from inventory
    ItemInstance to_equip = inv->take_item(inventory_slot);

    // Equip and get previous
    std::optional<ItemInstance> previous = equip->equip(target_slot, to_equip);

    // Apply stats
    apply_equipment_stats(world, entity, to_equip);

    // Fire equipped event
    if (m_on_equipped) {
        m_on_equipped(world, entity, target_slot, to_equip);
    }

    ItemEquippedEvent event;
    event.entity = entity;
    event.slot = target_slot;
    event.item = to_equip;
    event.previous_item = previous;
    core::game_events().broadcast(event);

    // Put previous in inventory
    if (previous.has_value()) {
        remove_equipment_stats(world, entity, previous.value());
        inv->add_item(previous.value());
    }

    return true;
}

bool InventoryManager::unequip_to_inventory(scene::World& world, scene::Entity entity,
                                            EquipmentSlot slot) {
    auto* inv = world.try_get<InventoryComponent>(entity);
    auto* equip = world.try_get<EquipmentComponent>(entity);

    if (!inv || !equip) return false;
    if (!equip->has_equipped(slot)) return false;

    // Check inventory space
    const auto* item = equip->get_equipped(slot);
    if (!inv->has_space_for(*item)) return false;

    // Unequip
    std::optional<ItemInstance> removed = equip->unequip(slot);
    if (!removed.has_value()) return false;

    // Remove stats
    remove_equipment_stats(world, entity, removed.value());

    // Add to inventory
    int inv_slot = inv->add_item(removed.value());

    // Fire events
    if (m_on_unequipped) {
        m_on_unequipped(world, entity, slot, removed.value());
    }

    ItemUnequippedEvent event;
    event.entity = entity;
    event.slot = slot;
    event.item = removed.value();
    event.moved_to_inventory = true;
    event.inventory_slot = inv_slot;
    core::game_events().broadcast(event);

    return true;
}

bool InventoryManager::swap_equipment(scene::World& world, scene::Entity entity,
                                      int inventory_slot, EquipmentSlot equip_slot) {
    auto* inv = world.try_get<InventoryComponent>(entity);
    auto* equip = world.try_get<EquipmentComponent>(entity);

    if (!inv || !equip) return false;

    auto* inv_item = inv->get_item(inventory_slot);
    if (!inv_item) return false;

    const auto* def = inv_item->get_definition();
    if (!def || !def->is_equipment()) return false;

    if (!can_equip(world, entity, *inv_item)) return false;

    // Take from inventory
    ItemInstance to_equip = inv->take_item(inventory_slot);

    // Swap
    std::optional<ItemInstance> previous = equip->equip(equip_slot, to_equip);

    apply_equipment_stats(world, entity, to_equip);

    if (previous.has_value()) {
        remove_equipment_stats(world, entity, previous.value());
        inv->add_to_slot(inventory_slot, previous.value());
    }

    // Fire events
    if (m_on_equipped) {
        m_on_equipped(world, entity, equip_slot, to_equip);
    }

    ItemEquippedEvent event;
    event.entity = entity;
    event.slot = equip_slot;
    event.item = to_equip;
    event.previous_item = previous;
    core::game_events().broadcast(event);

    return true;
}

bool InventoryManager::can_equip(scene::World& world, scene::Entity entity,
                                 const ItemInstance& item) const {
    const auto* def = item.get_definition();
    if (!def) return false;

    return check_requirements(world, entity, def);
}

// ============================================================================
// Currency
// ============================================================================

bool InventoryManager::give_currency(scene::World& world, scene::Entity entity,
                                     const std::string& currency_id, int64_t amount) {
    auto* inv = world.try_get<InventoryComponent>(entity);
    if (!inv) return false;

    int64_t old_amount = inv->get_currency(currency_id);
    inv->add_currency(currency_id, amount);
    int64_t new_amount = inv->get_currency(currency_id);

    if (m_on_currency_changed) {
        m_on_currency_changed(world, entity, currency_id, new_amount - old_amount);
    }

    CurrencyChangedEvent event;
    event.entity = entity;
    event.currency_id = currency_id;
    event.old_amount = old_amount;
    event.new_amount = new_amount;
    event.delta = new_amount - old_amount;
    event.reason = "give";
    core::game_events().broadcast(event);

    return true;
}

bool InventoryManager::take_currency(scene::World& world, scene::Entity entity,
                                     const std::string& currency_id, int64_t amount) {
    auto* inv = world.try_get<InventoryComponent>(entity);
    if (!inv) return false;

    if (!inv->can_afford(currency_id, amount)) return false;

    int64_t old_amount = inv->get_currency(currency_id);
    inv->spend_currency(currency_id, amount);
    int64_t new_amount = inv->get_currency(currency_id);

    if (m_on_currency_changed) {
        m_on_currency_changed(world, entity, currency_id, new_amount - old_amount);
    }

    CurrencyChangedEvent event;
    event.entity = entity;
    event.currency_id = currency_id;
    event.old_amount = old_amount;
    event.new_amount = new_amount;
    event.delta = new_amount - old_amount;
    event.reason = "take";
    core::game_events().broadcast(event);

    return true;
}

bool InventoryManager::transfer_currency(scene::World& world,
                                         scene::Entity from, scene::Entity to,
                                         const std::string& currency_id, int64_t amount) {
    if (!take_currency(world, from, currency_id, amount)) {
        return false;
    }
    return give_currency(world, to, currency_id, amount);
}

// ============================================================================
// Queries
// ============================================================================

bool InventoryManager::has_item(scene::World& world, scene::Entity entity,
                                const std::string& item_id, int count) const {
    const auto* inv = world.try_get<InventoryComponent>(entity);
    if (!inv) return false;
    return inv->has_item(item_id, count);
}

int InventoryManager::count_item(scene::World& world, scene::Entity entity,
                                 const std::string& item_id) const {
    const auto* inv = world.try_get<InventoryComponent>(entity);
    if (!inv) return 0;
    return inv->count_item(item_id);
}

bool InventoryManager::has_equipment_in_slot(scene::World& world, scene::Entity entity,
                                             EquipmentSlot slot) const {
    const auto* equip = world.try_get<EquipmentComponent>(entity);
    if (!equip) return false;
    return equip->has_equipped(slot);
}

// ============================================================================
// Loot
// ============================================================================

std::vector<ItemInstance> InventoryManager::generate_loot(const std::string& loot_table_id,
                                                          int player_level, float luck_bonus) {
    // TODO: Load loot table from registry
    // For now, return empty
    core::log(core::LogLevel::Debug, "[Inventory] Generating loot from table: {} (level {}, luck {})",
                   loot_table_id, player_level, luck_bonus);
    return {};
}

TransferResult InventoryManager::give_loot(scene::World& world, scene::Entity entity,
                                           const std::string& loot_table_id,
                                           int player_level, float luck_bonus) {
    auto items = generate_loot(loot_table_id, player_level, luck_bonus);
    if (items.empty()) {
        return TransferResult::SourceEmpty;
    }

    bool any_success = false;
    bool any_failure = false;

    for (const auto& item : items) {
        TransferResult result = give_item(world, entity, item);
        if (result == TransferResult::Success) {
            any_success = true;
        } else {
            any_failure = true;
        }
    }

    LootGeneratedEvent event;
    event.loot_table_id = loot_table_id;
    event.source = scene::Entity{};
    event.recipient = entity;
    event.items = items;
    event.player_level = player_level;
    event.luck_bonus = luck_bonus;
    core::game_events().broadcast(event);

    if (any_success && any_failure) return TransferResult::PartialSuccess;
    if (any_success) return TransferResult::Success;
    return TransferResult::Failed;
}

// ============================================================================
// Callbacks
// ============================================================================

void InventoryManager::set_on_item_added(SlotCallback callback) {
    m_on_item_added = std::move(callback);
}

void InventoryManager::set_on_item_removed(SlotCallback callback) {
    m_on_item_removed = std::move(callback);
}

void InventoryManager::set_on_item_used(ItemCallback callback) {
    m_on_item_used = std::move(callback);
}

void InventoryManager::set_on_equipped(EquipCallback callback) {
    m_on_equipped = std::move(callback);
}

void InventoryManager::set_on_unequipped(EquipCallback callback) {
    m_on_unequipped = std::move(callback);
}

void InventoryManager::set_on_currency_changed(CurrencyCallback callback) {
    m_on_currency_changed = std::move(callback);
}

// ============================================================================
// Internal Helpers
// ============================================================================

void InventoryManager::apply_equipment_stats(scene::World& world, scene::Entity entity,
                                             const ItemInstance& item) {
    auto* stats_comp = world.try_get<stats::StatsComponent>(entity);
    if (!stats_comp) return;

    auto modifiers = item.get_all_modifiers();
    for (auto& mod : modifiers) {
        stats_comp->add_modifier(mod);
    }
}

void InventoryManager::remove_equipment_stats(scene::World& world, scene::Entity entity,
                                              const ItemInstance& item) {
    auto* stats_comp = world.try_get<stats::StatsComponent>(entity);
    if (!stats_comp) return;

    // Remove modifiers with this item's source ID
    for (auto& [stat, mods] : stats_comp->modifiers) {
        std::erase_if(mods, [&](const stats::StatModifier& m) {
            return m.source_id == item.definition_id ||
                   m.source_id == item.definition_id + "_scaling" ||
                   m.source_id == item.definition_id + "_random";
        });
    }
}

bool InventoryManager::check_requirements(scene::World& world, scene::Entity entity,
                                          const ItemDefinition* def) const {
    if (!def || def->requirements.empty()) return true;

    const auto* stats_comp = world.try_get<stats::StatsComponent>(entity);
    if (!stats_comp) return false;

    for (const auto& req : def->requirements) {
        float value = stats_comp->get(req.stat);
        if (value < req.min_value) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// ECS System
// ============================================================================

void equipment_system(scene::World& world, double /*dt*/) {
    // This system can be used to update equipment stats when equipment changes
    // For now, stats are applied/removed immediately during equip/unequip
    // This could be extended to handle durability decay, set bonuses, etc.

    auto view = world.view<EquipmentComponent>();
    for (auto entity : view) {
        // Process equipment if needed
        (void)entity;
    }
}

} // namespace engine::inventory
