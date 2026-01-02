#pragma once

#include <engine/core/math.hpp>
#include <engine/audio/sound.hpp>
#include <cstdint>

namespace engine::audio {

using namespace engine::core;

// Distance attenuation models
enum class AttenuationModel : uint8_t {
    None,           // No attenuation
    Linear,         // Linear falloff
    InverseSquare,  // 1/d^2 (physically accurate)
    Logarithmic,    // Log-based (sounds natural)
    Custom          // User-defined curve
};

// 3D audio source component
struct AudioSource {
    SoundHandle sound;

    // Playback state
    bool playing = false;
    bool loop = false;
    float volume = 1.0f;
    float pitch = 1.0f;

    // 3D spatial settings
    bool spatial = true;
    float min_distance = 1.0f;      // Distance at which sound is full volume
    float max_distance = 50.0f;     // Distance at which sound is inaudible
    AttenuationModel attenuation = AttenuationModel::InverseSquare;
    float rolloff = 1.0f;           // Rolloff factor

    // Cone settings (for directional sounds)
    bool use_cone = false;
    float cone_inner_angle = 360.0f;  // Full volume angle (degrees)
    float cone_outer_angle = 360.0f;  // Zero volume angle (degrees)
    float cone_outer_volume = 0.0f;   // Volume at outer angle

    // Doppler effect
    bool enable_doppler = true;
    float doppler_factor = 1.0f;

    // Computed values (updated by audio system)
    float computed_volume = 1.0f;
    float computed_pan = 0.0f;  // -1 = left, 0 = center, 1 = right
    
    // For Doppler calculation
    Vec3 prev_position{0.0f};
    bool first_update = true;
};

// Audio listener component (typically on camera/player)
struct AudioListener {
    bool active = true;
    uint8_t priority = 0;      // Highest priority listener is used
    float volume_scale = 1.0f;  // Master volume multiplier

    // Velocity for doppler calculations
    Vec3 velocity{0.0f};
    Vec3 prev_position{0.0f};
    bool first_update = true;
};

// Audio trigger zone (plays sound when entered)
struct AudioTrigger {
    SoundHandle sound;
    float trigger_radius = 5.0f;
    bool one_shot = true;
    bool triggered = false;
    float cooldown = 0.0f;          // Time before can trigger again
    float cooldown_timer = 0.0f;    // Current cooldown
};

// Reverb zone component
struct ReverbZone {
    float min_distance = 0.0f;   // Full reverb inside this distance
    float max_distance = 10.0f;  // No reverb outside this distance

    // Reverb parameters
    float decay_time = 1.0f;     // Seconds
    float early_delay = 0.02f;
    float late_delay = 0.04f;
    float diffusion = 0.5f;
    float density = 0.5f;
    float high_frequency_decay = 0.8f;

    bool active = true;
};

// Calculate attenuation for a given distance
float calculate_attenuation(float distance, float min_dist, float max_dist,
                           AttenuationModel model, float rolloff);

// Calculate cone attenuation
float calculate_cone_attenuation(const Vec3& source_forward,
                                  const Vec3& to_listener,
                                  float inner_angle,
                                  float outer_angle,
                                  float outer_volume);

} // namespace engine::audio
