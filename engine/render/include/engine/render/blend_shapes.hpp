#pragma once

#include <engine/render/types.hpp>
#include <engine/core/math.hpp>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

namespace engine::render {

using namespace engine::core;

// Forward declarations
class IRenderer;

// Single vertex delta for a morph target
struct MorphTargetDelta {
    uint32_t vertex_index = 0;
    Vec3 position_delta{0.0f};
    Vec3 normal_delta{0.0f};
    Vec3 tangent_delta{0.0f};

    MorphTargetDelta() = default;
    MorphTargetDelta(uint32_t idx, const Vec3& pos, const Vec3& norm = Vec3{0.0f}, const Vec3& tan = Vec3{0.0f})
        : vertex_index(idx), position_delta(pos), normal_delta(norm), tangent_delta(tan) {}
};

// A single morph target (blend shape)
class MorphTarget {
public:
    MorphTarget() = default;
    explicit MorphTarget(const std::string& name);

    const std::string& get_name() const { return m_name; }
    void set_name(const std::string& name) { m_name = name; }

    // Add vertex deltas (sparse representation)
    void add_delta(const MorphTargetDelta& delta);
    void add_delta(uint32_t vertex_index, const Vec3& position_delta,
                   const Vec3& normal_delta = Vec3{0.0f},
                   const Vec3& tangent_delta = Vec3{0.0f});

    // Bulk add deltas
    void set_deltas(std::vector<MorphTargetDelta> deltas);

    // Get deltas
    const std::vector<MorphTargetDelta>& get_deltas() const { return m_deltas; }
    size_t get_delta_count() const { return m_deltas.size(); }

    // Clear all deltas
    void clear();

    // Check if target affects a specific vertex
    bool affects_vertex(uint32_t vertex_index) const;

    // Get delta for a specific vertex (returns nullptr if not affected)
    const MorphTargetDelta* get_delta_for_vertex(uint32_t vertex_index) const;

private:
    std::string m_name;
    std::vector<MorphTargetDelta> m_deltas;
    std::unordered_map<uint32_t, size_t> m_vertex_to_delta_index;
};

// Collection of morph targets for a mesh
class BlendShapeSet {
public:
    BlendShapeSet() = default;
    ~BlendShapeSet();

    // Morph target management
    void add_morph_target(MorphTarget target);
    void add_morph_target(const std::string& name, std::vector<MorphTargetDelta> deltas);
    void remove_morph_target(const std::string& name);
    void clear();

    // Access morph targets
    MorphTarget* get_target(const std::string& name);
    const MorphTarget* get_target(const std::string& name) const;
    MorphTarget* get_target(size_t index);
    const MorphTarget* get_target(size_t index) const;
    size_t get_target_count() const { return m_targets.size(); }

    // Find target index by name (-1 if not found)
    int find_target_index(const std::string& name) const;

    // Get all target names
    std::vector<std::string> get_target_names() const;

    // Set the vertex count this blend shape set is for
    void set_vertex_count(uint32_t count) { m_vertex_count = count; }
    uint32_t get_vertex_count() const { return m_vertex_count; }

private:
    std::vector<MorphTarget> m_targets;
    std::unordered_map<std::string, size_t> m_name_to_index;
    uint32_t m_vertex_count = 0;
};

// Runtime instance of blend shapes (holds weights)
class BlendShapeInstance {
public:
    BlendShapeInstance() = default;
    explicit BlendShapeInstance(std::shared_ptr<BlendShapeSet> shape_set);

    void set_shape_set(std::shared_ptr<BlendShapeSet> shape_set);
    std::shared_ptr<BlendShapeSet> get_shape_set() const { return m_shape_set; }

    // Set weight by name (0-1 range, can exceed for exaggeration)
    void set_weight(const std::string& name, float weight);
    void set_weight(size_t index, float weight);

    // Get weight
    float get_weight(const std::string& name) const;
    float get_weight(size_t index) const;

    // Animated weight interpolation
    void set_target_weight(const std::string& name, float target, float speed = 5.0f);
    void update_weights(float delta_time);

    // Reset all weights to 0
    void reset_all_weights();

    // Get all weights
    const std::vector<float>& get_weights() const { return m_weights; }

    // Check if any weights are non-zero
    bool has_active_weights() const;

    // Get number of active (non-zero) weights
    size_t get_active_weight_count() const;

private:
    std::shared_ptr<BlendShapeSet> m_shape_set;
    std::vector<float> m_weights;
    std::vector<float> m_target_weights;
    std::vector<float> m_weight_speeds;
};

// CPU-based blend shape deformation
class BlendShapeDeformer {
public:
    BlendShapeDeformer() = default;

    // Apply blend shapes to mesh vertices (CPU)
    void apply(
        const BlendShapeInstance& instance,
        const std::vector<Vertex>& base_vertices,
        std::vector<Vertex>& out_vertices
    );

    // Apply to position-only data
    void apply_positions(
        const BlendShapeInstance& instance,
        const std::vector<Vec3>& base_positions,
        std::vector<Vec3>& out_positions
    );

    // Maximum active blend shapes for optimization
    void set_max_active_shapes(size_t max) { m_max_active_shapes = max; }
    size_t get_max_active_shapes() const { return m_max_active_shapes; }

private:
    size_t m_max_active_shapes = 8;
};

// ECS Component for blend shapes
struct BlendShapeComponent {
    BlendShapeInstance instance;
    BlendShapeDeformer deformer;

    // Cached deformed vertices (updated each frame)
    std::vector<Vertex> deformed_vertices;
    bool vertices_dirty = true;

    // Whether to use GPU or CPU deformation
    bool use_gpu_deformation = false;

    // Preset configurations
    struct Preset {
        std::string name;
        std::unordered_map<std::string, float> weights;
    };
    std::vector<Preset> presets;

    // Apply a preset
    void apply_preset(const std::string& preset_name, float blend = 1.0f);
};

// Facial expression controller (convenience layer for facial animation)
class FacialExpressionController {
public:
    // Common expression types
    enum class Expression {
        Neutral,
        Happy,
        Sad,
        Angry,
        Surprised,
        Disgusted,
        Fearful,
        // Mouth shapes for lip sync
        Viseme_sil,   // Silence
        Viseme_PP,    // P, B, M
        Viseme_FF,    // F, V
        Viseme_TH,    // TH
        Viseme_DD,    // T, D, N
        Viseme_kk,    // K, G
        Viseme_CH,    // CH, J, SH
        Viseme_SS,    // S, Z
        Viseme_nn,    // N, NG
        Viseme_RR,    // R
        Viseme_aa,    // A
        Viseme_E,     // E
        Viseme_I,     // I
        Viseme_O,     // O
        Viseme_U      // U
    };

    FacialExpressionController() = default;
    explicit FacialExpressionController(BlendShapeInstance* instance);

    void set_instance(BlendShapeInstance* instance) { m_instance = instance; }

    // Map blend shape names to expressions
    void map_expression(Expression expr, const std::vector<std::pair<std::string, float>>& shapes);

    // Set expression (blends from current state)
    void set_expression(Expression expr, float weight = 1.0f, float blend_time = 0.3f);

    // Set viseme for lip sync
    void set_viseme(Expression viseme, float weight = 1.0f);

    // Blink control
    void set_blink_enabled(bool enabled) { m_blink_enabled = enabled; }
    void set_blink_shape(const std::string& shape_name) { m_blink_shape = shape_name; }
    void set_blink_interval(float min_seconds, float max_seconds);
    void trigger_blink();

    // Update (call each frame)
    void update(float delta_time);

    // Reset to neutral
    void reset();

private:
    void update_expression_blend(float delta_time);
    void update_blink(float delta_time);

    BlendShapeInstance* m_instance = nullptr;

    // Expression mappings
    std::unordered_map<Expression, std::vector<std::pair<std::string, float>>> m_expression_maps;

    // Current expression state
    Expression m_current_expression = Expression::Neutral;
    Expression m_target_expression = Expression::Neutral;
    float m_expression_weight = 0.0f;
    float m_target_expression_weight = 0.0f;
    float m_expression_blend_time = 0.3f;
    float m_expression_blend_progress = 1.0f;

    // Viseme state (separate from expression)
    Expression m_current_viseme = Expression::Viseme_sil;
    float m_viseme_weight = 0.0f;

    // Blink state
    bool m_blink_enabled = true;
    std::string m_blink_shape = "blink";
    float m_blink_timer = 0.0f;
    float m_next_blink_time = 3.0f;
    float m_blink_min_interval = 2.0f;
    float m_blink_max_interval = 6.0f;
    float m_blink_duration = 0.15f;
    float m_blink_progress = 1.0f;
    bool m_is_blinking = false;
};

} // namespace engine::render
