#include <engine/render/lod.hpp>
#include <engine/render/render_pipeline.hpp>
#include <algorithm>
#include <cmath>

namespace engine::render {

// LODGroup implementation

void LODGroup::add_level(const LODLevel& level) {
    levels.push_back(level);
    sort_levels();
}

void LODGroup::add_level(MeshHandle mesh, float screen_ratio) {
    LODLevel level;
    level.mesh = mesh;
    level.screen_height_ratio = screen_ratio;
    add_level(level);
}

void LODGroup::sort_levels() {
    // Sort by screen ratio (highest first, so LOD 0 is highest detail)
    std::sort(levels.begin(), levels.end(),
        [](const LODLevel& a, const LODLevel& b) {
            return a.screen_height_ratio > b.screen_height_ratio;
        });
}

// LODSelector implementation

LODSelectionResult LODSelector::select(
    const LODGroup& group,
    const AABB& world_bounds,
    const CameraData& camera
) const {
    LODSelectionResult result;

    if (group.empty()) {
        result.current_lod = 0;
        result.target_lod = 0;
        return result;
    }

    // Calculate screen ratio
    result.screen_ratio = calculate_screen_ratio(world_bounds, camera);

    // Apply LOD bias
    float biased_ratio = result.screen_ratio;
    float total_bias = m_global_bias + group.lod_bias;
    if (total_bias != 0.0f) {
        // Positive bias = lower LOD (multiply ratio to make it seem smaller)
        // Negative bias = higher LOD (multiply ratio to make it seem larger)
        biased_ratio *= std::pow(2.0f, -total_bias);
    }

    // Check cull distance
    if (group.use_cull_distance && group.cull_distance > 0.0f) {
        float distance = calculate_distance(world_bounds, camera);
        if (distance > group.cull_distance) {
            result.is_culled = true;
            result.current_lod = -1;
            result.target_lod = -1;
            return result;
        }
    }

    // Check forced LOD
    if (m_force_lod >= 0) {
        int forced = std::min(m_force_lod, static_cast<int>(group.levels.size()) - 1);
        result.current_lod = forced;
        result.target_lod = forced;
        result.fade_progress = 1.0f;
        return result;
    }

    // Select LOD based on screen ratio
    int selected_lod = 0;
    for (size_t i = 0; i < group.levels.size(); ++i) {
        if (biased_ratio >= group.levels[i].screen_height_ratio) {
            selected_lod = static_cast<int>(i);
            break;
        }
        selected_lod = static_cast<int>(i);  // Use lowest if all thresholds passed
    }

    // Apply max LOD limit
    if (m_max_lod_level >= 0) {
        selected_lod = std::min(selected_lod, m_max_lod_level);
    }

    result.target_lod = selected_lod;
    result.current_lod = selected_lod;  // Actual transition handled by LODState

    return result;
}

float LODSelector::calculate_screen_ratio(
    const AABB& world_bounds,
    const CameraData& camera
) const {
    // Get bounds center and size
    Vec3 center = (world_bounds.min + world_bounds.max) * 0.5f;
    Vec3 size = world_bounds.max - world_bounds.min;
    float radius = glm::length(size) * 0.5f;

    // Calculate distance from camera
    float distance = glm::length(center - camera.position);
    if (distance < 0.001f) {
        return 1.0f;  // Very close, use highest LOD
    }

    // Calculate screen space height
    // Using: screen_height = (object_height * screen_height) / (2 * distance * tan(fov/2))
    // Simplified to ratio: object_height / (2 * distance * tan(fov/2))

    float half_fov_tan = std::tan(camera.fov * 0.5f);
    float screen_ratio = radius / (distance * half_fov_tan);

    return std::clamp(screen_ratio, 0.0f, 1.0f);
}

float LODSelector::calculate_distance(
    const AABB& world_bounds,
    const CameraData& camera
) const {
    Vec3 center = (world_bounds.min + world_bounds.max) * 0.5f;
    return glm::length(center - camera.position);
}

// LODState implementation

void LODState::update(float dt) {
    if (!is_transitioning) return;

    fade_time += dt;
    if (fade_time >= fade_duration) {
        current_lod = target_lod;
        is_transitioning = false;
        fade_time = 0.0f;
    }
}

void LODState::start_transition(int new_lod, float duration) {
    if (new_lod == current_lod && !is_transitioning) {
        return;
    }

    if (new_lod == target_lod && is_transitioning) {
        return;  // Already transitioning to this LOD
    }

    target_lod = new_lod;
    fade_duration = duration;
    fade_time = 0.0f;
    is_transitioning = true;
}

float LODState::get_fade_progress() const {
    if (!is_transitioning || fade_duration <= 0.0f) {
        return 1.0f;
    }
    return std::clamp(fade_time / fade_duration, 0.0f, 1.0f);
}

// LODComponent implementation

MeshHandle LODComponent::get_current_mesh() const {
    if (!enabled || lod_group.empty()) {
        return MeshHandle{};
    }

    int lod_index = state.current_lod;
    if (lod_index < 0 || lod_index >= static_cast<int>(lod_group.levels.size())) {
        lod_index = 0;
    }

    return lod_group.levels[lod_index].mesh;
}

MaterialHandle LODComponent::get_current_material() const {
    if (!enabled || lod_group.empty()) {
        return MaterialHandle{};
    }

    int lod_index = state.current_lod;
    if (lod_index < 0 || lod_index >= static_cast<int>(lod_group.levels.size())) {
        lod_index = 0;
    }

    return lod_group.levels[lod_index].material;
}

bool LODComponent::get_crossfade_meshes(
    MeshHandle& mesh_a, MaterialHandle& mat_a, float& weight_a,
    MeshHandle& mesh_b, MaterialHandle& mat_b, float& weight_b
) const {
    if (!enabled || !state.is_transitioning || lod_group.empty()) {
        return false;
    }

    if (lod_group.fade_mode != LODFadeMode::CrossFade) {
        return false;
    }

    int lod_a = state.current_lod;
    int lod_b = state.target_lod;

    if (lod_a < 0 || lod_a >= static_cast<int>(lod_group.levels.size()) ||
        lod_b < 0 || lod_b >= static_cast<int>(lod_group.levels.size())) {
        return false;
    }

    float progress = state.get_fade_progress();

    mesh_a = lod_group.levels[lod_a].mesh;
    mat_a = lod_group.levels[lod_a].material;
    weight_a = 1.0f - progress;

    mesh_b = lod_group.levels[lod_b].mesh;
    mat_b = lod_group.levels[lod_b].material;
    weight_b = progress;

    return true;
}

// LOD system update function

void lod_system_update(
    entt::registry& registry,
    const CameraData& camera,
    const LODSelector& selector,
    float dt
) {
    auto view = registry.view<LODComponent>();

    for (auto entity : view) {
        auto& lod = view.get<LODComponent>(entity);

        if (!lod.enabled) continue;

        // Get world bounds for this entity
        // In a real implementation, you'd get this from a BoundsComponent or similar
        AABB bounds;
        // bounds = ... get from entity

        // Apply custom bias if set
        LODGroup adjusted_group = lod.lod_group;
        if (lod.use_custom_bias) {
            adjusted_group.lod_bias = lod.custom_bias;
        }

        // Select LOD
        LODSelectionResult result = selector.select(adjusted_group, bounds, camera);
        lod.last_result = result;

        // Start transition if LOD changed
        if (result.target_lod != lod.state.target_lod && !result.is_culled) {
            lod.state.start_transition(result.target_lod, lod.lod_group.fade_duration);
        }

        // Update transition state
        lod.state.update(dt);
    }
}

// LOD presets

namespace LODPresets {

LODGroup create_simple_3_level(
    MeshHandle high,
    MeshHandle medium,
    MeshHandle low,
    float medium_threshold,
    float low_threshold
) {
    LODGroup group;

    LODLevel high_level;
    high_level.mesh = high;
    high_level.screen_height_ratio = 1.0f;
    group.levels.push_back(high_level);

    LODLevel medium_level;
    medium_level.mesh = medium;
    medium_level.screen_height_ratio = medium_threshold;
    group.levels.push_back(medium_level);

    LODLevel low_level;
    low_level.mesh = low;
    low_level.screen_height_ratio = low_threshold;
    group.levels.push_back(low_level);

    return group;
}

LODGroup create_with_billboard(
    MeshHandle high,
    MeshHandle medium,
    MeshHandle low,
    MeshHandle billboard,
    float medium_threshold,
    float low_threshold,
    float billboard_threshold
) {
    LODGroup group = create_simple_3_level(high, medium, low, medium_threshold, low_threshold);

    LODLevel billboard_level;
    billboard_level.mesh = billboard;
    billboard_level.screen_height_ratio = billboard_threshold;
    billboard_level.cast_shadows = false;  // Billboards typically don't cast shadows
    group.levels.push_back(billboard_level);

    return group;
}

LODGroup create_character(
    MeshHandle high,
    MeshHandle medium,
    MeshHandle low,
    float animation_lod_distance
) {
    LODGroup group = create_simple_3_level(high, medium, low, 0.35f, 0.15f);
    group.reduce_animation_at_distance = true;
    group.animation_lod_distance = animation_lod_distance;
    group.fade_mode = LODFadeMode::Dither;  // Characters look better with dithered transitions
    return group;
}

} // namespace LODPresets

// Quality presets

LODQualityPreset LODQualityPreset::ultra() {
    LODQualityPreset preset;
    preset.global_bias = -0.5f;  // Prefer higher detail
    preset.max_lod_level = -1;   // No limit
    preset.use_crossfade = true;
    preset.fade_duration = 0.5f;
    return preset;
}

LODQualityPreset LODQualityPreset::high() {
    LODQualityPreset preset;
    preset.global_bias = 0.0f;
    preset.max_lod_level = -1;
    preset.use_crossfade = true;
    preset.fade_duration = 0.4f;
    return preset;
}

LODQualityPreset LODQualityPreset::medium() {
    LODQualityPreset preset;
    preset.global_bias = 0.5f;  // Prefer lower detail
    preset.max_lod_level = 2;   // Max LOD 2 (skip highest detail)
    preset.use_crossfade = false;
    preset.fade_duration = 0.3f;
    return preset;
}

LODQualityPreset LODQualityPreset::low() {
    LODQualityPreset preset;
    preset.global_bias = 1.0f;  // Strong preference for lower detail
    preset.max_lod_level = 1;   // Skip two highest detail levels
    preset.use_crossfade = false;
    preset.fade_duration = 0.0f;  // Instant switching
    return preset;
}

} // namespace engine::render
