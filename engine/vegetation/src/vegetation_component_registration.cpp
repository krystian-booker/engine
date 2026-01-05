#include <engine/reflect/reflect.hpp>
#include <engine/vegetation/grass.hpp>
#include <engine/vegetation/foliage.hpp>

// This file registers all vegetation components with the reflection system.
// Components are automatically registered at static initialization time.

namespace {

using namespace engine::vegetation;
using namespace engine::reflect;

// Register GrassComponent
struct GrassComponentRegistrar {
    GrassComponentRegistrar() {
        TypeRegistry::instance().register_component<GrassComponent>("GrassComponent",
            TypeMeta().set_display_name("Grass").set_description("Grass rendering settings for terrain"));

        TypeRegistry::instance().register_property<GrassComponent, &GrassComponent::auto_generate>("auto_generate",
            PropertyMeta().set_display_name("Auto Generate").set_category("Generation"));

        TypeRegistry::instance().register_property<GrassComponent, &GrassComponent::density_map_path>("density_map_path",
            PropertyMeta().set_display_name("Density Map Path").set_category("Generation"));

        // Density settings
        TypeRegistry::instance().register_property<GrassComponent, &GrassComponent::settings>(
            "settings.density",
            PropertyMeta().set_display_name("Density").set_category("Density").set_range(1.0f, 200.0f),
            [](const GrassComponent& c) { return c.settings.density; },
            [](GrassComponent& c, float v) { c.settings.density = v; });

        TypeRegistry::instance().register_property<GrassComponent, &GrassComponent::settings>(
            "settings.density_variance",
            PropertyMeta().set_display_name("Density Variance").set_category("Density").set_range(0.0f, 1.0f),
            [](const GrassComponent& c) { return c.settings.density_variance; },
            [](GrassComponent& c, float v) { c.settings.density_variance = v; });

        // Blade shape
        TypeRegistry::instance().register_property<GrassComponent, &GrassComponent::settings>(
            "settings.blade_width",
            PropertyMeta().set_display_name("Blade Width").set_category("Blade Shape").set_range(0.01f, 0.2f),
            [](const GrassComponent& c) { return c.settings.blade_width; },
            [](GrassComponent& c, float v) { c.settings.blade_width = v; });

        TypeRegistry::instance().register_property<GrassComponent, &GrassComponent::settings>(
            "settings.blade_height",
            PropertyMeta().set_display_name("Blade Height").set_category("Blade Shape").set_range(0.1f, 2.0f),
            [](const GrassComponent& c) { return c.settings.blade_height; },
            [](GrassComponent& c, float v) { c.settings.blade_height = v; });

        // LOD settings
        TypeRegistry::instance().register_property<GrassComponent, &GrassComponent::settings>(
            "settings.lod_start_distance",
            PropertyMeta().set_display_name("LOD Start").set_category("LOD").set_range(5.0f, 100.0f),
            [](const GrassComponent& c) { return c.settings.lod_start_distance; },
            [](GrassComponent& c, float v) { c.settings.lod_start_distance = v; });

        TypeRegistry::instance().register_property<GrassComponent, &GrassComponent::settings>(
            "settings.lod_end_distance",
            PropertyMeta().set_display_name("LOD End").set_category("LOD").set_range(20.0f, 200.0f),
            [](const GrassComponent& c) { return c.settings.lod_end_distance; },
            [](GrassComponent& c, float v) { c.settings.lod_end_distance = v; });

        TypeRegistry::instance().register_property<GrassComponent, &GrassComponent::settings>(
            "settings.cull_distance",
            PropertyMeta().set_display_name("Cull Distance").set_category("LOD").set_range(30.0f, 500.0f),
            [](const GrassComponent& c) { return c.settings.cull_distance; },
            [](GrassComponent& c, float v) { c.settings.cull_distance = v; });

        // Wind settings
        TypeRegistry::instance().register_property<GrassComponent, &GrassComponent::settings>(
            "settings.wind.strength",
            PropertyMeta().set_display_name("Wind Strength").set_category("Wind").set_range(0.0f, 2.0f),
            [](const GrassComponent& c) { return c.settings.wind.strength; },
            [](GrassComponent& c, float v) { c.settings.wind.strength = v; });

        TypeRegistry::instance().register_property<GrassComponent, &GrassComponent::settings>(
            "settings.wind.speed",
            PropertyMeta().set_display_name("Wind Speed").set_category("Wind").set_range(0.0f, 5.0f),
            [](const GrassComponent& c) { return c.settings.wind.speed; },
            [](GrassComponent& c, float v) { c.settings.wind.speed = v; });

        TypeRegistry::instance().register_property<GrassComponent, &GrassComponent::settings>(
            "settings.wind.frequency",
            PropertyMeta().set_display_name("Wind Frequency").set_category("Wind").set_range(0.1f, 10.0f),
            [](const GrassComponent& c) { return c.settings.wind.frequency; },
            [](GrassComponent& c, float v) { c.settings.wind.frequency = v; });

        // Interaction settings
        TypeRegistry::instance().register_property<GrassComponent, &GrassComponent::settings>(
            "settings.enable_interaction",
            PropertyMeta().set_display_name("Enable Interaction").set_category("Interaction"),
            [](const GrassComponent& c) { return c.settings.enable_interaction; },
            [](GrassComponent& c, bool v) { c.settings.enable_interaction = v; });

        TypeRegistry::instance().register_property<GrassComponent, &GrassComponent::settings>(
            "settings.interaction_radius",
            PropertyMeta().set_display_name("Interaction Radius").set_category("Interaction").set_range(0.1f, 5.0f),
            [](const GrassComponent& c) { return c.settings.interaction_radius; },
            [](GrassComponent& c, float v) { c.settings.interaction_radius = v; });

        // Rendering settings
        TypeRegistry::instance().register_property<GrassComponent, &GrassComponent::settings>(
            "settings.cast_shadows",
            PropertyMeta().set_display_name("Cast Shadows").set_category("Rendering"),
            [](const GrassComponent& c) { return c.settings.cast_shadows; },
            [](GrassComponent& c, bool v) { c.settings.cast_shadows = v; });

        TypeRegistry::instance().register_property<GrassComponent, &GrassComponent::settings>(
            "settings.receive_shadows",
            PropertyMeta().set_display_name("Receive Shadows").set_category("Rendering"),
            [](const GrassComponent& c) { return c.settings.receive_shadows; },
            [](GrassComponent& c, bool v) { c.settings.receive_shadows = v; });
    }
};
static GrassComponentRegistrar _grass_registrar;

// Register FoliageComponent
struct FoliageComponentRegistrar {
    FoliageComponentRegistrar() {
        TypeRegistry::instance().register_component<FoliageComponent>("FoliageComponent",
            TypeMeta().set_display_name("Foliage").set_description("Foliage instance settings (trees, bushes, etc.)"));

        TypeRegistry::instance().register_property<FoliageComponent, &FoliageComponent::type_id>("type_id",
            PropertyMeta().set_display_name("Type ID").set_category("Type"));

        TypeRegistry::instance().register_property<FoliageComponent, &FoliageComponent::scale>("scale",
            PropertyMeta().set_display_name("Scale").set_category("Transform").set_range(0.1f, 10.0f));

        TypeRegistry::instance().register_property<FoliageComponent, &FoliageComponent::cast_shadows>("cast_shadows",
            PropertyMeta().set_display_name("Cast Shadows").set_category("Rendering"));

        // Note: instance_index is runtime state, not registered for serialization
    }
};
static FoliageComponentRegistrar _foliage_registrar;

// Register GrassInteractor as a component for entities that interact with grass
struct GrassInteractorComponent {
    float radius = 1.0f;
    float strength = 1.0f;
    bool enabled = true;
};

struct GrassInteractorComponentRegistrar {
    GrassInteractorComponentRegistrar() {
        TypeRegistry::instance().register_component<GrassInteractorComponent>("GrassInteractorComponent",
            TypeMeta().set_display_name("Grass Interactor").set_description("Makes entity bend grass when moving through it"));

        TypeRegistry::instance().register_property<GrassInteractorComponent, &GrassInteractorComponent::radius>("radius",
            PropertyMeta().set_display_name("Radius").set_range(0.1f, 10.0f));

        TypeRegistry::instance().register_property<GrassInteractorComponent, &GrassInteractorComponent::strength>("strength",
            PropertyMeta().set_display_name("Strength").set_range(0.0f, 2.0f));

        TypeRegistry::instance().register_property<GrassInteractorComponent, &GrassInteractorComponent::enabled>("enabled",
            PropertyMeta().set_display_name("Enabled"));
    }
};
static GrassInteractorComponentRegistrar _grass_interactor_registrar;

} // anonymous namespace
