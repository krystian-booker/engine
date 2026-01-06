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
    registry.register_type<InventorySlot>("InventorySlot")
        .property("is_locked", &InventorySlot::is_locked)
        .property("is_favorite", &InventorySlot::is_favorite);

    // InventoryComponent
    registry.register_type<InventoryComponent>("InventoryComponent")
        .property("max_slots", &InventoryComponent::max_slots)
        .property("max_weight", &InventoryComponent::max_weight)
        .property("auto_sort", &InventoryComponent::auto_sort);

    // EquipmentComponent
    registry.register_type<EquipmentComponent>("EquipmentComponent");

    // ItemInstance
    registry.register_type<ItemInstance>("ItemInstance")
        .property("definition_id", &ItemInstance::definition_id)
        .property("stack_count", &ItemInstance::stack_count)
        .property("item_level", &ItemInstance::item_level)
        .property("quality", &ItemInstance::quality)
        .property("current_durability", &ItemInstance::current_durability)
        .property("max_durability", &ItemInstance::max_durability)
        .property("is_bound", &ItemInstance::is_bound);

    // ItemDefinition
    registry.register_type<ItemDefinition>("ItemDefinition")
        .property("item_id", &ItemDefinition::item_id)
        .property("display_name", &ItemDefinition::display_name)
        .property("description", &ItemDefinition::description)
        .property("icon_path", &ItemDefinition::icon_path)
        .property("max_stack", &ItemDefinition::max_stack)
        .property("weight", &ItemDefinition::weight)
        .property("base_value", &ItemDefinition::base_value);

    core::log_info("inventory", "Registered inventory components with reflection");
}

// ============================================================================
// Event Registration
// ============================================================================

void register_inventory_events() {
    // Events are typically handled through the GameEventBus
    // This function registers event types for scripting/debugging if needed

    core::log_info("inventory", "Registered inventory events");
}

} // namespace engine::inventory
