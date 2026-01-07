#include <engine/inventory/inventory.hpp>
#include <engine/reflect/type_registry.hpp>
#include <engine/core/log.hpp>

namespace engine::inventory {

// ============================================================================
// Component Registration
// ============================================================================

void register_inventory_components() {
    auto& registry = reflect::TypeRegistry::instance();

    // InventorySlot
    // InventorySlot
    registry.register_type<InventorySlot>("InventorySlot");
    registry.register_property<InventorySlot, &InventorySlot::is_locked>("is_locked");
    registry.register_property<InventorySlot, &InventorySlot::is_favorite>("is_favorite");

    // InventoryComponent
    registry.register_type<InventoryComponent>("InventoryComponent");
    registry.register_property<InventoryComponent, &InventoryComponent::max_slots>("max_slots");
    registry.register_property<InventoryComponent, &InventoryComponent::max_weight>("max_weight");
    registry.register_property<InventoryComponent, &InventoryComponent::auto_sort>("auto_sort");

    // EquipmentComponent
    registry.register_type<EquipmentComponent>("EquipmentComponent");

    // ItemInstance
    registry.register_type<ItemInstance>("ItemInstance");
    registry.register_property<ItemInstance, &ItemInstance::definition_id>("definition_id");
    registry.register_property<ItemInstance, &ItemInstance::stack_count>("stack_count");
    registry.register_property<ItemInstance, &ItemInstance::item_level>("item_level");
    registry.register_property<ItemInstance, &ItemInstance::quality>("quality");
    registry.register_property<ItemInstance, &ItemInstance::current_durability>("current_durability");
    registry.register_property<ItemInstance, &ItemInstance::max_durability>("max_durability");
    registry.register_property<ItemInstance, &ItemInstance::is_bound>("is_bound");

    // ItemDefinition
    registry.register_type<ItemDefinition>("ItemDefinition");
    registry.register_property<ItemDefinition, &ItemDefinition::item_id>("item_id");
    registry.register_property<ItemDefinition, &ItemDefinition::display_name>("display_name");
    registry.register_property<ItemDefinition, &ItemDefinition::description>("description");
    registry.register_property<ItemDefinition, &ItemDefinition::icon_path>("icon_path");
    registry.register_property<ItemDefinition, &ItemDefinition::max_stack>("max_stack");
    registry.register_property<ItemDefinition, &ItemDefinition::weight>("weight");
    registry.register_property<ItemDefinition, &ItemDefinition::base_value>("base_value");

    core::log(core::LogLevel::Info, "[Inventory] Registered inventory components with reflection");
}

// ============================================================================
// Event Registration
// ============================================================================

void register_inventory_events() {
    // Events are typically handled through the GameEventBus
    // This function registers event types for scripting/debugging if needed

    core::log(core::LogLevel::Info, "[Inventory] Registered inventory events");
}

} // namespace engine::inventory
