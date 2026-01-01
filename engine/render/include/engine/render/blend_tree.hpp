#pragma once

#include <engine/render/animation_graph.hpp>
#include <engine/core/math.hpp>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

namespace engine::render {

using namespace engine::core;

// Blend tree node types
enum class BlendNodeType {
    Clip,           // Single animation clip
    Blend1D,        // Blend between clips based on single parameter
    Blend2D,        // 2D blend space (cartesian or directional)
    Additive,       // Additive blend on top of base
    Layered         // Layer-based blending with masks
};

// Point in a 1D blend space
struct BlendPoint1D {
    std::unique_ptr<IAnimGraphNode> node;
    float threshold = 0.0f;  // Parameter value at this point

    BlendPoint1D() = default;
    BlendPoint1D(std::unique_ptr<IAnimGraphNode> n, float t)
        : node(std::move(n)), threshold(t) {}
};

// Point in a 2D blend space
struct BlendPoint2D {
    std::unique_ptr<IAnimGraphNode> node;
    Vec2 position{0.0f};  // Position in 2D parameter space

    BlendPoint2D() = default;
    BlendPoint2D(std::unique_ptr<IAnimGraphNode> n, Vec2 pos)
        : node(std::move(n)), position(pos) {}
};

// Blend2D mode
enum class Blend2DMode {
    Cartesian,      // Simple 2D interpolation
    Directional,    // Freeform directional (angle + magnitude)
    FreeformCartesian  // Gradient band interpolation
};

// Blend tree node - can be composed hierarchically
class BlendTreeNode : public IAnimGraphNode {
public:
    BlendTreeNode() = default;
    explicit BlendTreeNode(BlendNodeType type);

    // Set node type
    void set_type(BlendNodeType type) { m_type = type; }
    BlendNodeType get_type() const { return m_type; }

    // For Clip type - set the clip directly
    void set_clip(std::shared_ptr<AnimationClip> clip);

    // For Blend1D - add blend points
    void add_blend_point_1d(std::unique_ptr<IAnimGraphNode> node, float threshold);
    void set_blend_parameter_x(const std::string& param_name) { m_param_x = param_name; }
    const std::string& get_blend_parameter_x() const { return m_param_x; }

    // For Blend2D - add blend points and set mode
    void add_blend_point_2d(std::unique_ptr<IAnimGraphNode> node, Vec2 position);
    void set_blend_parameter_y(const std::string& param_name) { m_param_y = param_name; }
    const std::string& get_blend_parameter_y() const { return m_param_y; }
    void set_blend_2d_mode(Blend2DMode mode) { m_blend_2d_mode = mode; }
    Blend2DMode get_blend_2d_mode() const { return m_blend_2d_mode; }

    // For Additive - set base and additive nodes
    void set_base_node(std::unique_ptr<IAnimGraphNode> node) { m_base_node = std::move(node); }
    void set_additive_node(std::unique_ptr<IAnimGraphNode> node) { m_additive_node = std::move(node); }
    void set_additive_weight_parameter(const std::string& param_name) { m_additive_weight_param = param_name; }

    // For Layered - add layers with masks
    struct Layer {
        std::unique_ptr<IAnimGraphNode> node;
        std::vector<float> bone_mask;  // Per-bone mask (0-1)
        std::string weight_parameter;
        float base_weight = 1.0f;
        AnimationBlendMode blend_mode = AnimationBlendMode::Override;
    };
    void add_layer(Layer layer);
    void set_layer_weight(size_t layer_index, float weight);

    // Sync settings
    void set_sync_enabled(bool sync) { m_sync_enabled = sync; }
    bool is_sync_enabled() const { return m_sync_enabled; }

    // IAnimGraphNode interface
    void evaluate(float dt, AnimationContext& ctx, std::vector<BoneTransform>& out_pose) override;
    float get_duration() const override;
    float get_time() const override;
    void set_time(float time) override;
    void reset() override;
    std::unique_ptr<IAnimGraphNode> clone() const override;

private:
    // Evaluate different blend types
    void evaluate_clip(float dt, AnimationContext& ctx, std::vector<BoneTransform>& out_pose);
    void evaluate_blend_1d(float dt, AnimationContext& ctx, std::vector<BoneTransform>& out_pose);
    void evaluate_blend_2d(float dt, AnimationContext& ctx, std::vector<BoneTransform>& out_pose);
    void evaluate_additive(float dt, AnimationContext& ctx, std::vector<BoneTransform>& out_pose);
    void evaluate_layered(float dt, AnimationContext& ctx, std::vector<BoneTransform>& out_pose);

    // 2D blend helpers
    void calculate_blend_weights_cartesian(Vec2 param, std::vector<float>& out_weights) const;
    void calculate_blend_weights_directional(Vec2 param, std::vector<float>& out_weights) const;

    // Sync time across child nodes based on normalized time
    void sync_children(float normalized_time);

    BlendNodeType m_type = BlendNodeType::Clip;

    // For Clip type
    std::unique_ptr<ClipNode> m_clip_node;

    // For Blend1D
    std::vector<BlendPoint1D> m_blend_points_1d;
    std::string m_param_x;

    // For Blend2D
    std::vector<BlendPoint2D> m_blend_points_2d;
    std::string m_param_y;
    Blend2DMode m_blend_2d_mode = Blend2DMode::Cartesian;

    // For Additive
    std::unique_ptr<IAnimGraphNode> m_base_node;
    std::unique_ptr<IAnimGraphNode> m_additive_node;
    std::string m_additive_weight_param;

    // For Layered
    std::vector<Layer> m_layers;

    // Sync settings
    bool m_sync_enabled = false;

    // Cached values
    float m_current_time = 0.0f;
    std::vector<BoneTransform> m_temp_pose_a;
    std::vector<BoneTransform> m_temp_pose_b;
};

// Factory functions for creating common blend tree configurations
namespace BlendTreeFactory {

// Create a simple clip node
std::unique_ptr<BlendTreeNode> create_clip(std::shared_ptr<AnimationClip> clip);

// Create a 1D blend between two clips (e.g., walk to run)
std::unique_ptr<BlendTreeNode> create_blend_1d(
    const std::string& parameter_name,
    std::shared_ptr<AnimationClip> clip_a, float threshold_a,
    std::shared_ptr<AnimationClip> clip_b, float threshold_b
);

// Create a 1D blend between multiple clips
std::unique_ptr<BlendTreeNode> create_blend_1d(
    const std::string& parameter_name,
    const std::vector<std::pair<std::shared_ptr<AnimationClip>, float>>& clips
);

// Create a 2D blend space (e.g., directional movement)
std::unique_ptr<BlendTreeNode> create_blend_2d(
    const std::string& param_x, const std::string& param_y,
    const std::vector<std::pair<std::shared_ptr<AnimationClip>, Vec2>>& clips,
    Blend2DMode mode = Blend2DMode::Cartesian
);

// Create an additive layer (e.g., additive hit reaction)
std::unique_ptr<BlendTreeNode> create_additive(
    std::unique_ptr<IAnimGraphNode> base,
    std::unique_ptr<IAnimGraphNode> additive,
    const std::string& weight_parameter
);

} // namespace BlendTreeFactory

} // namespace engine::render
