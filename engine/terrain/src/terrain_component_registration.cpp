// This file registers all terrain components with the reflection system.
// Components are automatically registered at static initialization time.

#include <engine/reflect/reflect.hpp>
#include <engine/terrain/terrain.hpp>

namespace {

using namespace engine::terrain;
using namespace engine::reflect;

// Register TerrainComponent
struct TerrainComponentRegistrar {
    TerrainComponentRegistrar() {
        TypeRegistry::instance().register_component<TerrainComponent>("TerrainComponent",
            TypeMeta().set_display_name("Terrain").set_description("Heightmap terrain with LOD and physics"));

        TypeRegistry::instance().register_property<TerrainComponent, &TerrainComponent::terrain_id>(
            "terrain_id",
            PropertyMeta().set_display_name("Terrain ID").set_tooltip("ID in TerrainManager"));
    }
};
static TerrainComponentRegistrar _terrain_registrar;

} // anonymous namespace
