#pragma once

#include <engine/core/math.hpp>
#include <engine/core/event_dispatcher.hpp>
#include <engine/scene/entity.hpp>
#include <cstdint>
#include <string>

namespace engine::scene { class World; }

namespace engine::gameplay {

using namespace engine::core;

// ============================================================================
// Water Volume Component
// ============================================================================

struct WaterVolumeComponent {
    // Water surface properties
    float water_height = 0.0f;              // World Y of water surface

    // Current/flow
    Vec3 current_direction{0.0f, 0.0f, 0.0f};
    float current_strength = 0.0f;          // Units per second

    // Physics modifiers
    float buoyancy = 1.0f;                  // Multiplier for upward force
    float drag = 2.0f;                      // Movement resistance
    float density = 1.0f;                   // Water density (1.0 = normal water)

    // Visual/audio settings
    bool apply_underwater_effects = true;   // Enable underwater post-process
    bool apply_underwater_audio = true;     // Enable underwater audio filter
    std::string underwater_reverb_preset = "Underwater";

    // Damage properties (for hazardous water like lava, acid)
    bool is_swimmable = true;               // Can entities swim in this?
    bool causes_damage = false;             // Does this water cause damage?
    float damage_per_second = 0.0f;         // Damage rate if causes_damage
    std::string damage_type = "drowning";   // Damage type for resistance calc

    // Volume bounds (if not using physics collider)
    Vec3 half_extents{10.0f, 5.0f, 10.0f};  // Box half-size for simple volumes
    bool use_collider_bounds = true;        // Use attached physics collider instead

    // ========================================================================
    // Helper Methods
    // ========================================================================

    // Get water height at a specific XZ position (for wave effects)
    float get_height_at(float /*x*/, float /*z*/) const {
        // Base implementation returns flat water
        // Override in derived or use wave system
        return water_height;
    }

    // Check if a world position is underwater
    bool is_position_underwater(const Vec3& position) const {
        return position.y < water_height;
    }

    // Get depth at position (positive = underwater, negative = above)
    float get_depth_at(const Vec3& position) const {
        return water_height - position.y;
    }

    // Get current force at position
    Vec3 get_current_at(const Vec3& /*position*/) const {
        // Base implementation returns uniform current
        return current_direction * current_strength;
    }
};

// ============================================================================
// Water Events
// ============================================================================

struct EnteredWaterEvent {
    scene::Entity entity;
    scene::Entity water_volume;
    float water_height;
};

struct ExitedWaterEvent {
    scene::Entity entity;
    scene::Entity water_volume;
};

struct SubmergedEvent {
    scene::Entity entity;
    scene::Entity water_volume;
};

struct SurfacedEvent {
    scene::Entity entity;
    scene::Entity water_volume;
};

struct StartedDrowningEvent {
    scene::Entity entity;
};

struct DrownedEvent {
    scene::Entity entity;  // Entity died from drowning
};

struct BreathRestoredEvent {
    scene::Entity entity;
    float breath_amount;
};

// ============================================================================
// Water Query Result
// ============================================================================

struct WaterQueryResult {
    bool in_water = false;
    float water_height = 0.0f;
    float depth = 0.0f;                     // Positive = underwater
    Vec3 current{0.0f};
    float buoyancy = 1.0f;
    float drag = 2.0f;
    scene::Entity water_entity = scene::NullEntity;
    bool is_swimmable = true;
    bool causes_damage = false;
    float damage_per_second = 0.0f;
};

// ============================================================================
// Water System Functions
// ============================================================================

// Query water at a specific world position
WaterQueryResult query_water_at(scene::World& world, const Vec3& position);

// Water volume detection system - updates entities in water
// Call in FixedUpdate phase, before character_movement_system
void water_detection_system(scene::World& world, double dt);

// Breath and drowning system - handles breath depletion and damage
// Call in FixedUpdate phase, after character_movement_system
void breath_system(scene::World& world, double dt);

// Water movement system - handles water-specific movement physics
// Called internally by character_movement_system when in water
void apply_water_movement(
    scene::World& world,
    scene::Entity entity,
    const WaterQueryResult& water_info,
    double dt
);

// ============================================================================
// Component Registration
// ============================================================================

void register_water_components();

} // namespace engine::gameplay
