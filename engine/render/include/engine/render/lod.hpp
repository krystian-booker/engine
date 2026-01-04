#pragma once

#include <engine/render/types.hpp>
#include <engine/core/math.hpp>
#include <vector>
#include <memory>
#include <functional>

#include <entt/entity/fwd.hpp>

namespace engine::render {

using namespace engine::core;

// Forward declarations
struct CameraData;

// LOD transition fade mode
enum class LODFadeMode {
    None,           // Instant switch (may cause popping)
    CrossFade,      // Blend both LODs during transition (2x draw calls)
    SpeedTree,      // Dithered fade using noise pattern
    Dither          // Screen-space dithered transition
};

// Single LOD level definition
struct LODLevel {
    MeshHandle mesh;
    MaterialHandle material;        // Can use simpler materials for lower LODs

    // Screen space size thresholds (0-1 ratio of screen height)
    float screen_height_ratio = 0.0f;  // Switch to this LOD when smaller than this

    // Transition settings
    float transition_width = 0.1f;     // Range for fading (in screen ratio)

    // Optional shadow settings (use cheaper shadow mesh for lower LODs)
    MeshHandle shadow_mesh;            // If invalid, uses main mesh
    bool cast_shadows = true;

    LODLevel() = default;
    LODLevel(MeshHandle m, float ratio)
        : mesh(m), screen_height_ratio(ratio) {}
};

// LOD group for an object
struct LODGroup {
    std::vector<LODLevel> levels;  // Sorted from highest to lowest detail

    LODFadeMode fade_mode = LODFadeMode::Dither;
    float fade_duration = 0.5f;        // Time to complete fade transition

    // Per-object LOD bias (negative = higher detail, positive = lower detail)
    float lod_bias = 0.0f;

    // Cull distance (object not rendered beyond this)
    float cull_distance = 0.0f;        // 0 = no distance culling
    bool use_cull_distance = false;

    // Animation LOD settings
    bool reduce_animation_at_distance = true;
    float animation_lod_distance = 50.0f;  // Distance to reduce animation quality

    // Add a LOD level
    void add_level(const LODLevel& level);
    void add_level(MeshHandle mesh, float screen_ratio);

    // Sort levels by screen ratio (highest first)
    void sort_levels();

    // Get the number of LOD levels
    size_t level_count() const { return levels.size(); }

    // Check if the group has any levels
    bool empty() const { return levels.empty(); }
};

// LOD selection result
struct LODSelectionResult {
    int current_lod = 0;           // Current LOD index (-1 if culled)
    int target_lod = 0;            // Target LOD index (-1 if culled)
    float fade_progress = 1.0f;    // 0-1 transition progress
    bool is_transitioning = false;
    bool is_culled = false;
    float screen_ratio = 0.0f;     // Current screen space ratio
};

// LOD selector - calculates which LOD to use
class LODSelector {
public:
    LODSelector() = default;

    // Select LOD based on camera and object bounds
    LODSelectionResult select(
        const LODGroup& group,
        const AABB& world_bounds,
        const CameraData& camera
    ) const;

    // Calculate screen height ratio for given bounds
    float calculate_screen_ratio(
        const AABB& world_bounds,
        const CameraData& camera
    ) const;

    // Calculate distance from camera
    float calculate_distance(
        const AABB& world_bounds,
        const CameraData& camera
    ) const;

    // Global LOD settings
    void set_global_bias(float bias) { m_global_bias = bias; }
    float get_global_bias() const { return m_global_bias; }

    // Force a specific LOD level for debugging
    void set_force_lod(int level) { m_force_lod = level; }
    void clear_force_lod() { m_force_lod = -1; }
    int get_force_lod() const { return m_force_lod; }

    // Maximum LOD level (useful for low-end devices)
    void set_max_lod_level(int max_level) { m_max_lod_level = max_level; }
    int get_max_lod_level() const { return m_max_lod_level; }

private:
    float m_global_bias = 0.0f;
    int m_force_lod = -1;  // -1 = auto select
    int m_max_lod_level = -1;  // -1 = no limit
};

// LOD state for a single entity (tracks transitions)
struct LODState {
    int current_lod = 0;
    int target_lod = 0;
    float fade_time = 0.0f;
    float fade_duration = 0.5f;
    bool is_transitioning = false;

    void update(float dt);
    void start_transition(int new_lod, float duration);
    float get_fade_progress() const;
};

// ECS Component for LOD
struct LODComponent {
    LODGroup lod_group;
    LODState state;
    LODSelectionResult last_result;

    // Whether LOD is enabled for this entity
    bool enabled = true;

    // Override global settings per-entity
    bool use_custom_bias = false;
    float custom_bias = 0.0f;

    // Get the current mesh to render
    MeshHandle get_current_mesh() const;
    MaterialHandle get_current_material() const;

    // Get both meshes for crossfade (returns false if not transitioning)
    bool get_crossfade_meshes(
        MeshHandle& mesh_a, MaterialHandle& mat_a, float& weight_a,
        MeshHandle& mesh_b, MaterialHandle& mat_b, float& weight_b
    ) const;
};

// LOD group presets for common configurations
namespace LODPresets {

// Simple 3-level LOD (high, medium, low)
LODGroup create_simple_3_level(
    MeshHandle high,
    MeshHandle medium,
    MeshHandle low,
    float medium_threshold = 0.3f,
    float low_threshold = 0.1f
);

// 4-level LOD with billboard at distance
LODGroup create_with_billboard(
    MeshHandle high,
    MeshHandle medium,
    MeshHandle low,
    MeshHandle billboard,
    float medium_threshold = 0.4f,
    float low_threshold = 0.2f,
    float billboard_threshold = 0.05f
);

// Character LOD (with animation reduction)
LODGroup create_character(
    MeshHandle high,
    MeshHandle medium,
    MeshHandle low,
    float animation_lod_distance = 30.0f
);

} // namespace LODPresets

// Quality preset helper
struct LODQualityPreset {
    float global_bias = 0.0f;
    int max_lod_level = -1;
    bool use_crossfade = true;
    float fade_duration = 0.5f;

    static LODQualityPreset ultra();
    static LODQualityPreset high();
    static LODQualityPreset medium();
    static LODQualityPreset low();
};

} // namespace engine::render
