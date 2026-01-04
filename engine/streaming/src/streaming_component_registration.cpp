#include <engine/reflect/reflect.hpp>
#include <engine/streaming/scene_streaming.hpp>
#include <engine/streaming/streaming_volume.hpp>

// This file registers all streaming components with the reflection system.
// Components are automatically registered at static initialization time.

namespace {

using namespace engine::streaming;
using namespace engine::reflect;

// Register StreamingComponent
struct StreamingComponentRegistrar {
    StreamingComponentRegistrar() {
        TypeRegistry::instance().register_component<StreamingComponent>("StreamingComponent",
            TypeMeta().set_display_name("Streaming").set_description("Marks entity as belonging to a streaming cell"));

        TypeRegistry::instance().register_property<StreamingComponent, &StreamingComponent::cell_name>("cell_name",
            PropertyMeta().set_display_name("Cell Name"));

        TypeRegistry::instance().register_property<StreamingComponent, &StreamingComponent::persist_across_cells>("persist_across_cells",
            PropertyMeta().set_display_name("Persist Across Cells"));

        TypeRegistry::instance().register_property<StreamingComponent, &StreamingComponent::stream_with_player>("stream_with_player",
            PropertyMeta().set_display_name("Stream With Player"));
    }
};
static StreamingComponentRegistrar _streaming_registrar;

// Register StreamingZoneComponent
struct StreamingZoneComponentRegistrar {
    StreamingZoneComponentRegistrar() {
        TypeRegistry::instance().register_component<StreamingZoneComponent>("StreamingZoneComponent",
            TypeMeta().set_display_name("Streaming Zone").set_description("Trigger zone for streaming cells"));

        TypeRegistry::instance().register_property<StreamingZoneComponent, &StreamingZoneComponent::cells_to_load>("cells_to_load",
            PropertyMeta().set_display_name("Cells to Load"));

        TypeRegistry::instance().register_property<StreamingZoneComponent, &StreamingZoneComponent::cells_to_unload>("cells_to_unload",
            PropertyMeta().set_display_name("Cells to Unload"));

        TypeRegistry::instance().register_property<StreamingZoneComponent, &StreamingZoneComponent::activation_radius>("activation_radius",
            PropertyMeta().set_display_name("Activation Radius").set_range(0.0f, 1000.0f));

        TypeRegistry::instance().register_property<StreamingZoneComponent, &StreamingZoneComponent::one_shot>("one_shot",
            PropertyMeta().set_display_name("One Shot"));

        TypeRegistry::instance().register_property<StreamingZoneComponent, &StreamingZoneComponent::triggered>("triggered",
            PropertyMeta().set_display_name("Triggered").set_read_only(true));
    }
};
static StreamingZoneComponentRegistrar _streamingzone_registrar;

// Register StreamingVolumeComponent
struct StreamingVolumeComponentRegistrar {
    StreamingVolumeComponentRegistrar() {
        TypeRegistry::instance().register_component<StreamingVolumeComponent>("StreamingVolumeComponent",
            TypeMeta().set_display_name("Streaming Volume").set_description("Volume that triggers streaming behavior"));

        TypeRegistry::instance().register_property<StreamingVolumeComponent, &StreamingVolumeComponent::volume_name>("volume_name",
            PropertyMeta().set_display_name("Volume Name"));

        TypeRegistry::instance().register_property<StreamingVolumeComponent, &StreamingVolumeComponent::use_entity_bounds>("use_entity_bounds",
            PropertyMeta().set_display_name("Use Entity Bounds"));

        TypeRegistry::instance().register_property<StreamingVolumeComponent, &StreamingVolumeComponent::use_inline_volume>("use_inline_volume",
            PropertyMeta().set_display_name("Use Inline Volume"));
    }
};
static StreamingVolumeComponentRegistrar _streamingvolume_registrar;

// Register StreamingPortalComponent
struct StreamingPortalComponentRegistrar {
    StreamingPortalComponentRegistrar() {
        TypeRegistry::instance().register_component<StreamingPortalComponent>("StreamingPortalComponent",
            TypeMeta().set_display_name("Streaming Portal").set_description("Portal connecting two streaming cells"));

        TypeRegistry::instance().register_property<StreamingPortalComponent, &StreamingPortalComponent::cell_a>("cell_a",
            PropertyMeta().set_display_name("Cell A"));

        TypeRegistry::instance().register_property<StreamingPortalComponent, &StreamingPortalComponent::cell_b>("cell_b",
            PropertyMeta().set_display_name("Cell B"));

        TypeRegistry::instance().register_property<StreamingPortalComponent, &StreamingPortalComponent::position>("position",
            PropertyMeta().set_display_name("Position"));

        TypeRegistry::instance().register_property<StreamingPortalComponent, &StreamingPortalComponent::normal>("normal",
            PropertyMeta().set_display_name("Normal"));

        TypeRegistry::instance().register_property<StreamingPortalComponent, &StreamingPortalComponent::width>("width",
            PropertyMeta().set_display_name("Width").set_range(0.1f, 100.0f));

        TypeRegistry::instance().register_property<StreamingPortalComponent, &StreamingPortalComponent::height>("height",
            PropertyMeta().set_display_name("Height").set_range(0.1f, 100.0f));

        TypeRegistry::instance().register_property<StreamingPortalComponent, &StreamingPortalComponent::bidirectional>("bidirectional",
            PropertyMeta().set_display_name("Bidirectional"));

        TypeRegistry::instance().register_property<StreamingPortalComponent, &StreamingPortalComponent::occlude>("occlude",
            PropertyMeta().set_display_name("Occlude"));
    }
};
static StreamingPortalComponentRegistrar _streamingportal_registrar;

} // anonymous namespace
