#include <engine/reflect/reflect.hpp>
#include <engine/audio/audio_components.hpp>

// This file registers all audio components with the reflection system.
// Components are automatically registered at static initialization time.

namespace {

using namespace engine::audio;
using namespace engine::reflect;

// Register AttenuationModel enum
struct AttenuationModelRegistrar {
    AttenuationModelRegistrar() {
        TypeRegistry::instance().register_enum<AttenuationModel>("AttenuationModel", {
            { AttenuationModel::None, "None" },
            { AttenuationModel::Linear, "Linear" },
            { AttenuationModel::InverseSquare, "Inverse Square" },
            { AttenuationModel::Logarithmic, "Logarithmic" }
        });
    }
};
static AttenuationModelRegistrar _attenuation_registrar;

// Register AudioSource component
struct AudioSourceRegistrar {
    AudioSourceRegistrar() {
        TypeRegistry::instance().register_component<AudioSource>("AudioSource",
            TypeMeta().set_display_name("Audio Source").set_description("3D spatial audio source"));

        // Playback settings
        TypeRegistry::instance().register_property<AudioSource, &AudioSource::playing>("playing",
            PropertyMeta().set_display_name("Playing").set_category("Playback"));

        TypeRegistry::instance().register_property<AudioSource, &AudioSource::loop>("loop",
            PropertyMeta().set_display_name("Loop").set_category("Playback"));

        TypeRegistry::instance().register_property<AudioSource, &AudioSource::volume>("volume",
            PropertyMeta().set_display_name("Volume").set_category("Playback").set_range(0.0f, 2.0f));

        TypeRegistry::instance().register_property<AudioSource, &AudioSource::pitch>("pitch",
            PropertyMeta().set_display_name("Pitch").set_category("Playback").set_range(0.1f, 4.0f));

        // Spatial settings
        TypeRegistry::instance().register_property<AudioSource, &AudioSource::spatial>("spatial",
            PropertyMeta().set_display_name("Spatial").set_category("3D Audio"));

        TypeRegistry::instance().register_property<AudioSource, &AudioSource::min_distance>("min_distance",
            PropertyMeta().set_display_name("Min Distance").set_category("3D Audio").set_range(0.0f, 1000.0f));

        TypeRegistry::instance().register_property<AudioSource, &AudioSource::max_distance>("max_distance",
            PropertyMeta().set_display_name("Max Distance").set_category("3D Audio").set_range(0.0f, 10000.0f));

        TypeRegistry::instance().register_property<AudioSource, &AudioSource::attenuation>("attenuation",
            PropertyMeta().set_display_name("Attenuation Model").set_category("3D Audio"));

        TypeRegistry::instance().register_property<AudioSource, &AudioSource::rolloff>("rolloff",
            PropertyMeta().set_display_name("Rolloff Factor").set_category("3D Audio").set_range(0.0f, 10.0f));

        // Cone settings
        TypeRegistry::instance().register_property<AudioSource, &AudioSource::use_cone>("use_cone",
            PropertyMeta().set_display_name("Use Cone").set_category("Directional"));

        TypeRegistry::instance().register_property<AudioSource, &AudioSource::cone_inner_angle>("cone_inner_angle",
            PropertyMeta().set_display_name("Inner Angle").set_category("Directional").set_range(0.0f, 360.0f).set_angle(true));

        TypeRegistry::instance().register_property<AudioSource, &AudioSource::cone_outer_angle>("cone_outer_angle",
            PropertyMeta().set_display_name("Outer Angle").set_category("Directional").set_range(0.0f, 360.0f).set_angle(true));

        TypeRegistry::instance().register_property<AudioSource, &AudioSource::cone_outer_volume>("cone_outer_volume",
            PropertyMeta().set_display_name("Outer Volume").set_category("Directional").set_range(0.0f, 1.0f));

        // Doppler settings
        TypeRegistry::instance().register_property<AudioSource, &AudioSource::enable_doppler>("enable_doppler",
            PropertyMeta().set_display_name("Enable Doppler").set_category("Doppler"));

        TypeRegistry::instance().register_property<AudioSource, &AudioSource::doppler_factor>("doppler_factor",
            PropertyMeta().set_display_name("Doppler Factor").set_category("Doppler").set_range(0.0f, 5.0f));

        // Computed values (read-only, for debugging)
        TypeRegistry::instance().register_property<AudioSource, &AudioSource::computed_volume>("computed_volume",
            PropertyMeta().set_display_name("Computed Volume").set_category("Debug").set_read_only(true));

        TypeRegistry::instance().register_property<AudioSource, &AudioSource::computed_pan>("computed_pan",
            PropertyMeta().set_display_name("Computed Pan").set_category("Debug").set_read_only(true));
    }
};
static AudioSourceRegistrar _audiosource_registrar;

// Register AudioListener component
struct AudioListenerRegistrar {
    AudioListenerRegistrar() {
        TypeRegistry::instance().register_component<AudioListener>("AudioListener",
            TypeMeta().set_display_name("Audio Listener").set_description("Audio listener for 3D positioning (typically on camera/player)"));

        TypeRegistry::instance().register_property<AudioListener, &AudioListener::active>("active",
            PropertyMeta().set_display_name("Active"));

        TypeRegistry::instance().register_property<AudioListener, &AudioListener::priority>("priority",
            PropertyMeta().set_display_name("Priority").set_range(0, 255));

        TypeRegistry::instance().register_property<AudioListener, &AudioListener::volume_scale>("volume_scale",
            PropertyMeta().set_display_name("Volume Scale").set_range(0.0f, 2.0f));
    }
};
static AudioListenerRegistrar _audiolistener_registrar;

// Register AudioTrigger component
struct AudioTriggerRegistrar {
    AudioTriggerRegistrar() {
        TypeRegistry::instance().register_component<AudioTrigger>("AudioTrigger",
            TypeMeta().set_display_name("Audio Trigger").set_description("Zone-based sound trigger"));

        TypeRegistry::instance().register_property<AudioTrigger, &AudioTrigger::trigger_radius>("trigger_radius",
            PropertyMeta().set_display_name("Trigger Radius").set_range(0.0f, 1000.0f));

        TypeRegistry::instance().register_property<AudioTrigger, &AudioTrigger::one_shot>("one_shot",
            PropertyMeta().set_display_name("One Shot"));

        TypeRegistry::instance().register_property<AudioTrigger, &AudioTrigger::cooldown>("cooldown",
            PropertyMeta().set_display_name("Cooldown").set_range(0.0f, 60.0f));

        TypeRegistry::instance().register_property<AudioTrigger, &AudioTrigger::triggered>("triggered",
            PropertyMeta().set_display_name("Triggered").set_read_only(true));
    }
};
static AudioTriggerRegistrar _audiotrigger_registrar;

// Register ReverbZone component
struct ReverbZoneRegistrar {
    ReverbZoneRegistrar() {
        TypeRegistry::instance().register_component<ReverbZone>("ReverbZone",
            TypeMeta().set_display_name("Reverb Zone").set_description("Environmental reverb zone"));

        TypeRegistry::instance().register_property<ReverbZone, &ReverbZone::active>("active",
            PropertyMeta().set_display_name("Active"));

        TypeRegistry::instance().register_property<ReverbZone, &ReverbZone::min_distance>("min_distance",
            PropertyMeta().set_display_name("Min Distance").set_category("Distance").set_range(0.0f, 1000.0f));

        TypeRegistry::instance().register_property<ReverbZone, &ReverbZone::max_distance>("max_distance",
            PropertyMeta().set_display_name("Max Distance").set_category("Distance").set_range(0.0f, 1000.0f));

        TypeRegistry::instance().register_property<ReverbZone, &ReverbZone::decay_time>("decay_time",
            PropertyMeta().set_display_name("Decay Time").set_category("Reverb").set_range(0.1f, 20.0f));

        TypeRegistry::instance().register_property<ReverbZone, &ReverbZone::early_delay>("early_delay",
            PropertyMeta().set_display_name("Early Delay").set_category("Reverb").set_range(0.0f, 0.3f));

        TypeRegistry::instance().register_property<ReverbZone, &ReverbZone::late_delay>("late_delay",
            PropertyMeta().set_display_name("Late Delay").set_category("Reverb").set_range(0.0f, 0.1f));

        TypeRegistry::instance().register_property<ReverbZone, &ReverbZone::diffusion>("diffusion",
            PropertyMeta().set_display_name("Diffusion").set_category("Reverb").set_range(0.0f, 1.0f));

        TypeRegistry::instance().register_property<ReverbZone, &ReverbZone::density>("density",
            PropertyMeta().set_display_name("Density").set_category("Reverb").set_range(0.0f, 1.0f));

        TypeRegistry::instance().register_property<ReverbZone, &ReverbZone::high_frequency_decay>("high_frequency_decay",
            PropertyMeta().set_display_name("HF Decay").set_category("Reverb").set_range(0.0f, 1.0f));
    }
};
static ReverbZoneRegistrar _reverbzone_registrar;

} // anonymous namespace
