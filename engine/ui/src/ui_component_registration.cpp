#include <engine/reflect/reflect.hpp>
#include <engine/ui/ui_components.hpp>

// This file registers all UI components with the reflection system.
// Components are automatically registered at static initialization time.

namespace {

using namespace engine::ui;
using namespace engine::reflect;

// Register UICanvasComponent
struct UICanvasComponentRegistrar {
    UICanvasComponentRegistrar() {
        TypeRegistry::instance().register_component<UICanvasComponent>("UICanvasComponent",
            TypeMeta().set_display_name("UI Canvas").set_description("Screen-space UI canvas for HUD elements"));

        TypeRegistry::instance().register_property<UICanvasComponent, &UICanvasComponent::sort_order>("sort_order",
            PropertyMeta().set_display_name("Sort Order").set_category("Rendering")
                .set_tooltip("Render order (higher = rendered on top)").set_range(-1000, 1000));

        TypeRegistry::instance().register_property<UICanvasComponent, &UICanvasComponent::enabled>("enabled",
            PropertyMeta().set_display_name("Enabled").set_category("General")
                .set_tooltip("Whether canvas is updated and rendered"));
    }
};
static UICanvasComponentRegistrar _uicanvas_registrar;

// Register UIWorldCanvasComponent
struct UIWorldCanvasComponentRegistrar {
    UIWorldCanvasComponentRegistrar() {
        TypeRegistry::instance().register_component<UIWorldCanvasComponent>("UIWorldCanvasComponent",
            TypeMeta().set_display_name("UI World Canvas")
                .set_description("World-space UI canvas for health bars, nameplates, interaction prompts"));

        // Transform settings
        TypeRegistry::instance().register_property<UIWorldCanvasComponent, &UIWorldCanvasComponent::offset>("offset",
            PropertyMeta().set_display_name("Offset").set_category("Transform")
                .set_tooltip("Local offset from entity position"));

        TypeRegistry::instance().register_property<UIWorldCanvasComponent, &UIWorldCanvasComponent::use_entity_transform>("use_entity_transform",
            PropertyMeta().set_display_name("Use Entity Transform").set_category("Transform")
                .set_tooltip("Sync position from entity's LocalTransform"));

        // Distance settings
        TypeRegistry::instance().register_property<UIWorldCanvasComponent, &UIWorldCanvasComponent::max_distance>("max_distance",
            PropertyMeta().set_display_name("Max Distance").set_category("Distance")
                .set_tooltip("Maximum render distance").set_range(1.0f, 1000.0f));

        TypeRegistry::instance().register_property<UIWorldCanvasComponent, &UIWorldCanvasComponent::fade_range>("fade_range",
            PropertyMeta().set_display_name("Fade Range").set_category("Distance")
                .set_tooltip("Distance range over which to fade out").set_range(0.0f, 100.0f));

        // Screen size settings
        TypeRegistry::instance().register_property<UIWorldCanvasComponent, &UIWorldCanvasComponent::constant_screen_size>("constant_screen_size",
            PropertyMeta().set_display_name("Constant Screen Size").set_category("Size")
                .set_tooltip("Keep same screen size regardless of distance"));

        TypeRegistry::instance().register_property<UIWorldCanvasComponent, &UIWorldCanvasComponent::reference_distance>("reference_distance",
            PropertyMeta().set_display_name("Reference Distance").set_category("Size")
                .set_tooltip("Distance at which canvas appears at base size").set_range(1.0f, 100.0f));

        TypeRegistry::instance().register_property<UIWorldCanvasComponent, &UIWorldCanvasComponent::min_scale>("min_scale",
            PropertyMeta().set_display_name("Min Scale").set_category("Size")
                .set_tooltip("Minimum scale multiplier").set_range(0.1f, 1.0f));

        TypeRegistry::instance().register_property<UIWorldCanvasComponent, &UIWorldCanvasComponent::max_scale>("max_scale",
            PropertyMeta().set_display_name("Max Scale").set_category("Size")
                .set_tooltip("Maximum scale multiplier").set_range(1.0f, 10.0f));

        TypeRegistry::instance().register_property<UIWorldCanvasComponent, &UIWorldCanvasComponent::enabled>("enabled",
            PropertyMeta().set_display_name("Enabled").set_category("General")
                .set_tooltip("Whether canvas is updated and rendered"));
    }
};
static UIWorldCanvasComponentRegistrar _uiworldcanvas_registrar;

} // anonymous namespace
