#include <engine/render/blend_tree.hpp>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace engine::render {

// BlendTreeNode implementation

BlendTreeNode::BlendTreeNode(BlendNodeType type)
    : m_type(type)
{
}

void BlendTreeNode::set_clip(std::shared_ptr<AnimationClip> clip) {
    m_type = BlendNodeType::Clip;
    m_clip_node = std::make_unique<ClipNode>(clip);
}

void BlendTreeNode::add_blend_point_1d(std::unique_ptr<IAnimGraphNode> node, float threshold) {
    m_blend_points_1d.emplace_back(std::move(node), threshold);
    // Sort by threshold for proper interpolation
    std::sort(m_blend_points_1d.begin(), m_blend_points_1d.end(),
        [](const BlendPoint1D& a, const BlendPoint1D& b) {
            return a.threshold < b.threshold;
        });
}

void BlendTreeNode::add_blend_point_2d(std::unique_ptr<IAnimGraphNode> node, Vec2 position) {
    m_blend_points_2d.emplace_back(std::move(node), position);
}

void BlendTreeNode::add_layer(Layer layer) {
    m_layers.push_back(std::move(layer));
}

void BlendTreeNode::set_layer_weight(size_t layer_index, float weight) {
    if (layer_index < m_layers.size()) {
        m_layers[layer_index].base_weight = weight;
    }
}

void BlendTreeNode::evaluate(float dt, AnimationContext& ctx, std::vector<BoneTransform>& out_pose) {
    switch (m_type) {
        case BlendNodeType::Clip:
            evaluate_clip(dt, ctx, out_pose);
            break;
        case BlendNodeType::Blend1D:
            evaluate_blend_1d(dt, ctx, out_pose);
            break;
        case BlendNodeType::Blend2D:
            evaluate_blend_2d(dt, ctx, out_pose);
            break;
        case BlendNodeType::Additive:
            evaluate_additive(dt, ctx, out_pose);
            break;
        case BlendNodeType::Layered:
            evaluate_layered(dt, ctx, out_pose);
            break;
    }
}

void BlendTreeNode::evaluate_clip(float dt, AnimationContext& ctx, std::vector<BoneTransform>& out_pose) {
    if (m_clip_node) {
        m_clip_node->evaluate(dt, ctx, out_pose);
        m_current_time = m_clip_node->get_time();
    }
}

void BlendTreeNode::evaluate_blend_1d(float dt, AnimationContext& ctx, std::vector<BoneTransform>& out_pose) {
    if (m_blend_points_1d.empty()) {
        return;
    }

    // Get parameter value
    float param_value = ctx.get_float(m_param_x);

    // Find the two points to blend between
    size_t lower_idx = 0;
    size_t upper_idx = 0;
    float blend_factor = 0.0f;

    if (m_blend_points_1d.size() == 1) {
        // Only one point, use it directly
        m_blend_points_1d[0].node->evaluate(dt, ctx, out_pose);
        return;
    }

    // Find surrounding points
    for (size_t i = 0; i < m_blend_points_1d.size() - 1; ++i) {
        if (param_value <= m_blend_points_1d[i].threshold) {
            // Below or at first point
            lower_idx = i;
            upper_idx = i;
            blend_factor = 0.0f;
            break;
        } else if (param_value < m_blend_points_1d[i + 1].threshold) {
            // Between two points
            lower_idx = i;
            upper_idx = i + 1;
            float range = m_blend_points_1d[i + 1].threshold - m_blend_points_1d[i].threshold;
            if (range > 0.0f) {
                blend_factor = (param_value - m_blend_points_1d[i].threshold) / range;
            }
            break;
        } else if (i == m_blend_points_1d.size() - 2) {
            // Above last point
            lower_idx = m_blend_points_1d.size() - 1;
            upper_idx = lower_idx;
            blend_factor = 0.0f;
        }
    }

    // Sync child nodes if enabled
    if (m_sync_enabled && get_duration() > 0.0f) {
        float normalized = m_current_time / get_duration();
        sync_children(normalized);
    }

    // Evaluate lower node
    m_blend_points_1d[lower_idx].node->evaluate(dt, ctx, m_temp_pose_a);

    if (lower_idx == upper_idx || blend_factor <= 0.0f) {
        out_pose = m_temp_pose_a;
    } else {
        // Evaluate upper node and blend
        m_blend_points_1d[upper_idx].node->evaluate(dt, ctx, m_temp_pose_b);
        blend_poses(m_temp_pose_a, m_temp_pose_b, blend_factor, out_pose);
    }

    // Update current time
    if (!m_blend_points_1d.empty() && m_blend_points_1d[lower_idx].node) {
        m_current_time = m_blend_points_1d[lower_idx].node->get_time();
    }
}

void BlendTreeNode::evaluate_blend_2d(float dt, AnimationContext& ctx, std::vector<BoneTransform>& out_pose) {
    if (m_blend_points_2d.empty()) {
        return;
    }

    if (m_blend_points_2d.size() == 1) {
        m_blend_points_2d[0].node->evaluate(dt, ctx, out_pose);
        return;
    }

    // Get parameter values
    Vec2 param_value{ctx.get_float(m_param_x), ctx.get_float(m_param_y)};

    // Calculate blend weights for all points
    std::vector<float> weights(m_blend_points_2d.size());
    switch (m_blend_2d_mode) {
        case Blend2DMode::Cartesian:
        case Blend2DMode::FreeformCartesian:
            calculate_blend_weights_cartesian(param_value, weights);
            break;
        case Blend2DMode::Directional:
            calculate_blend_weights_directional(param_value, weights);
            break;
    }

    // Sync children if enabled
    if (m_sync_enabled && get_duration() > 0.0f) {
        float normalized = m_current_time / get_duration();
        sync_children(normalized);
    }

    // Blend all poses weighted
    bool first = true;
    for (size_t i = 0; i < m_blend_points_2d.size(); ++i) {
        if (weights[i] <= 0.001f) continue;

        m_blend_points_2d[i].node->evaluate(dt, ctx, m_temp_pose_a);

        if (first) {
            out_pose.resize(m_temp_pose_a.size());
            for (size_t j = 0; j < m_temp_pose_a.size(); ++j) {
                out_pose[j].position = m_temp_pose_a[j].position * weights[i];
                out_pose[j].rotation = m_temp_pose_a[j].rotation;
                out_pose[j].scale = m_temp_pose_a[j].scale * weights[i];
            }
            first = false;
        } else {
            for (size_t j = 0; j < m_temp_pose_a.size() && j < out_pose.size(); ++j) {
                out_pose[j].position += m_temp_pose_a[j].position * weights[i];
                out_pose[j].rotation = glm::slerp(out_pose[j].rotation, m_temp_pose_a[j].rotation, weights[i]);
                out_pose[j].scale += m_temp_pose_a[j].scale * weights[i];
            }
        }
    }
}

void BlendTreeNode::calculate_blend_weights_cartesian(Vec2 param, std::vector<float>& out_weights) const {
    // Inverse distance weighting
    out_weights.resize(m_blend_points_2d.size());
    float total_weight = 0.0f;

    for (size_t i = 0; i < m_blend_points_2d.size(); ++i) {
        float dist = glm::length(param - m_blend_points_2d[i].position);
        if (dist < 0.0001f) {
            // Exact match - set full weight to this point
            std::fill(out_weights.begin(), out_weights.end(), 0.0f);
            out_weights[i] = 1.0f;
            return;
        }
        out_weights[i] = 1.0f / (dist * dist);  // Inverse square distance
        total_weight += out_weights[i];
    }

    // Normalize weights
    if (total_weight > 0.0f) {
        for (float& w : out_weights) {
            w /= total_weight;
        }
    }
}

void BlendTreeNode::calculate_blend_weights_directional(Vec2 param, std::vector<float>& out_weights) const {
    out_weights.resize(m_blend_points_2d.size());

    float magnitude = glm::length(param);
    if (magnitude < 0.0001f) {
        // At center - find the idle animation (usually at origin)
        for (size_t i = 0; i < m_blend_points_2d.size(); ++i) {
            if (glm::length(m_blend_points_2d[i].position) < 0.0001f) {
                std::fill(out_weights.begin(), out_weights.end(), 0.0f);
                out_weights[i] = 1.0f;
                return;
            }
        }
        // No origin point, use cartesian
        calculate_blend_weights_cartesian(param, out_weights);
        return;
    }

    // Get parameter angle
    float param_angle = std::atan2(param.y, param.x);

    // Find the two closest angles
    std::fill(out_weights.begin(), out_weights.end(), 0.0f);

    struct AngleDist {
        size_t index;
        float angle;
        float angle_diff;
    };
    std::vector<AngleDist> angles;
    angles.reserve(m_blend_points_2d.size());

    for (size_t i = 0; i < m_blend_points_2d.size(); ++i) {
        Vec2 pos = m_blend_points_2d[i].position;
        if (glm::length(pos) < 0.0001f) continue;  // Skip origin
        float angle = std::atan2(pos.y, pos.x);
        float diff = angle - param_angle;
        // Normalize to [-PI, PI]
        while (diff > 3.14159f) diff -= 2.0f * 3.14159f;
        while (diff < -3.14159f) diff += 2.0f * 3.14159f;
        angles.push_back({i, angle, diff});
    }

    if (angles.empty()) {
        // Only origin point exists
        if (!m_blend_points_2d.empty()) {
            out_weights[0] = 1.0f;
        }
        return;
    }

    // Sort by angle difference
    std::sort(angles.begin(), angles.end(),
        [](const AngleDist& a, const AngleDist& b) { return a.angle_diff < b.angle_diff; });

    // Find the two closest (one positive, one negative angle diff)
    int left_idx = -1, right_idx = -1;
    for (const auto& a : angles) {
        if (a.angle_diff <= 0 && left_idx < 0) left_idx = static_cast<int>(a.index);
        if (a.angle_diff >= 0 && right_idx < 0) right_idx = static_cast<int>(a.index);
        if (left_idx >= 0 && right_idx >= 0) break;
    }

    if (left_idx < 0) left_idx = right_idx;
    if (right_idx < 0) right_idx = left_idx;

    if (left_idx == right_idx) {
        out_weights[left_idx] = 1.0f;
    } else {
        // Interpolate between the two
        float left_angle = std::atan2(m_blend_points_2d[left_idx].position.y, m_blend_points_2d[left_idx].position.x);
        float right_angle = std::atan2(m_blend_points_2d[right_idx].position.y, m_blend_points_2d[right_idx].position.x);
        float span = right_angle - left_angle;
        if (span < 0) span += 2.0f * 3.14159f;
        float t = (param_angle - left_angle);
        if (t < 0) t += 2.0f * 3.14159f;
        t = span > 0.0001f ? t / span : 0.5f;

        out_weights[left_idx] = 1.0f - t;
        out_weights[right_idx] = t;
    }
}

void BlendTreeNode::evaluate_additive(float dt, AnimationContext& ctx, std::vector<BoneTransform>& out_pose) {
    if (!m_base_node) {
        return;
    }

    // Evaluate base pose
    m_base_node->evaluate(dt, ctx, out_pose);

    // If we have an additive node, apply it
    if (m_additive_node) {
        float weight = 1.0f;
        if (!m_additive_weight_param.empty()) {
            weight = ctx.get_float(m_additive_weight_param);
        }

        if (weight > 0.001f) {
            m_additive_node->evaluate(dt, ctx, m_temp_pose_a);
            add_pose(out_pose, m_temp_pose_a, weight, out_pose);
        }
    }

    m_current_time = m_base_node->get_time();
}

void BlendTreeNode::evaluate_layered(float dt, AnimationContext& ctx, std::vector<BoneTransform>& out_pose) {
    if (m_layers.empty()) {
        return;
    }

    // Evaluate first layer as base
    m_layers[0].node->evaluate(dt, ctx, out_pose);

    // Apply subsequent layers
    for (size_t i = 1; i < m_layers.size(); ++i) {
        Layer& layer = m_layers[i];
        if (layer.base_weight <= 0.001f || !layer.node) continue;

        float weight = layer.base_weight;
        if (!layer.weight_parameter.empty()) {
            weight *= ctx.get_float(layer.weight_parameter);
        }

        if (weight <= 0.001f) continue;

        layer.node->evaluate(dt, ctx, m_temp_pose_a);

        switch (layer.blend_mode) {
            case AnimationBlendMode::Override:
                if (layer.bone_mask.empty()) {
                    blend_poses(out_pose, m_temp_pose_a, weight, out_pose);
                } else {
                    blend_poses_masked(out_pose, m_temp_pose_a, weight, layer.bone_mask, out_pose);
                }
                break;
            case AnimationBlendMode::Additive:
                add_pose(out_pose, m_temp_pose_a, weight, out_pose);
                break;
            case AnimationBlendMode::Blend:
                blend_poses(out_pose, m_temp_pose_a, weight, out_pose);
                break;
        }
    }

    if (!m_layers.empty() && m_layers[0].node) {
        m_current_time = m_layers[0].node->get_time();
    }
}

void BlendTreeNode::sync_children(float normalized_time) {
    for (auto& point : m_blend_points_1d) {
        if (point.node) {
            float duration = point.node->get_duration();
            if (duration > 0.0f) {
                point.node->set_time(normalized_time * duration);
            }
        }
    }
    for (auto& point : m_blend_points_2d) {
        if (point.node) {
            float duration = point.node->get_duration();
            if (duration > 0.0f) {
                point.node->set_time(normalized_time * duration);
            }
        }
    }
}

float BlendTreeNode::get_duration() const {
    switch (m_type) {
        case BlendNodeType::Clip:
            return m_clip_node ? m_clip_node->get_duration() : 0.0f;
        case BlendNodeType::Blend1D:
            if (!m_blend_points_1d.empty() && m_blend_points_1d[0].node) {
                return m_blend_points_1d[0].node->get_duration();
            }
            return 0.0f;
        case BlendNodeType::Blend2D:
            if (!m_blend_points_2d.empty() && m_blend_points_2d[0].node) {
                return m_blend_points_2d[0].node->get_duration();
            }
            return 0.0f;
        case BlendNodeType::Additive:
            return m_base_node ? m_base_node->get_duration() : 0.0f;
        case BlendNodeType::Layered:
            if (!m_layers.empty() && m_layers[0].node) {
                return m_layers[0].node->get_duration();
            }
            return 0.0f;
    }
    return 0.0f;
}

float BlendTreeNode::get_time() const {
    return m_current_time;
}

void BlendTreeNode::set_time(float time) {
    m_current_time = time;
    if (m_clip_node) {
        m_clip_node->set_time(time);
    }
    for (auto& point : m_blend_points_1d) {
        if (point.node) point.node->set_time(time);
    }
    for (auto& point : m_blend_points_2d) {
        if (point.node) point.node->set_time(time);
    }
    if (m_base_node) m_base_node->set_time(time);
    if (m_additive_node) m_additive_node->set_time(time);
    for (auto& layer : m_layers) {
        if (layer.node) layer.node->set_time(time);
    }
}

void BlendTreeNode::reset() {
    m_current_time = 0.0f;
    if (m_clip_node) m_clip_node->reset();
    for (auto& point : m_blend_points_1d) {
        if (point.node) point.node->reset();
    }
    for (auto& point : m_blend_points_2d) {
        if (point.node) point.node->reset();
    }
    if (m_base_node) m_base_node->reset();
    if (m_additive_node) m_additive_node->reset();
    for (auto& layer : m_layers) {
        if (layer.node) layer.node->reset();
    }
}

std::unique_ptr<IAnimGraphNode> BlendTreeNode::clone() const {
    auto cloned = std::make_unique<BlendTreeNode>(m_type);
    cloned->m_param_x = m_param_x;
    cloned->m_param_y = m_param_y;
    cloned->m_blend_2d_mode = m_blend_2d_mode;
    cloned->m_additive_weight_param = m_additive_weight_param;
    cloned->m_sync_enabled = m_sync_enabled;

    if (m_clip_node) {
        cloned->m_clip_node = std::make_unique<ClipNode>(*m_clip_node);
    }

    for (const auto& point : m_blend_points_1d) {
        if (point.node) {
            cloned->m_blend_points_1d.emplace_back(point.node->clone(), point.threshold);
        }
    }

    for (const auto& point : m_blend_points_2d) {
        if (point.node) {
            cloned->m_blend_points_2d.emplace_back(point.node->clone(), point.position);
        }
    }

    if (m_base_node) cloned->m_base_node = m_base_node->clone();
    if (m_additive_node) cloned->m_additive_node = m_additive_node->clone();

    for (const auto& layer : m_layers) {
        Layer cloned_layer;
        if (layer.node) cloned_layer.node = layer.node->clone();
        cloned_layer.bone_mask = layer.bone_mask;
        cloned_layer.weight_parameter = layer.weight_parameter;
        cloned_layer.base_weight = layer.base_weight;
        cloned_layer.blend_mode = layer.blend_mode;
        cloned->m_layers.push_back(std::move(cloned_layer));
    }

    return cloned;
}

// Factory functions

namespace BlendTreeFactory {

std::unique_ptr<BlendTreeNode> create_clip(std::shared_ptr<AnimationClip> clip) {
    auto node = std::make_unique<BlendTreeNode>(BlendNodeType::Clip);
    node->set_clip(clip);
    return node;
}

std::unique_ptr<BlendTreeNode> create_blend_1d(
    const std::string& parameter_name,
    std::shared_ptr<AnimationClip> clip_a, float threshold_a,
    std::shared_ptr<AnimationClip> clip_b, float threshold_b
) {
    auto node = std::make_unique<BlendTreeNode>(BlendNodeType::Blend1D);
    node->set_blend_parameter_x(parameter_name);
    node->add_blend_point_1d(std::make_unique<ClipNode>(clip_a), threshold_a);
    node->add_blend_point_1d(std::make_unique<ClipNode>(clip_b), threshold_b);
    return node;
}

std::unique_ptr<BlendTreeNode> create_blend_1d(
    const std::string& parameter_name,
    const std::vector<std::pair<std::shared_ptr<AnimationClip>, float>>& clips
) {
    auto node = std::make_unique<BlendTreeNode>(BlendNodeType::Blend1D);
    node->set_blend_parameter_x(parameter_name);
    for (const auto& [clip, threshold] : clips) {
        node->add_blend_point_1d(std::make_unique<ClipNode>(clip), threshold);
    }
    return node;
}

std::unique_ptr<BlendTreeNode> create_blend_2d(
    const std::string& param_x, const std::string& param_y,
    const std::vector<std::pair<std::shared_ptr<AnimationClip>, Vec2>>& clips,
    Blend2DMode mode
) {
    auto node = std::make_unique<BlendTreeNode>(BlendNodeType::Blend2D);
    node->set_blend_parameter_x(param_x);
    node->set_blend_parameter_y(param_y);
    node->set_blend_2d_mode(mode);
    for (const auto& [clip, pos] : clips) {
        node->add_blend_point_2d(std::make_unique<ClipNode>(clip), pos);
    }
    return node;
}

std::unique_ptr<BlendTreeNode> create_additive(
    std::unique_ptr<IAnimGraphNode> base,
    std::unique_ptr<IAnimGraphNode> additive,
    const std::string& weight_parameter
) {
    auto node = std::make_unique<BlendTreeNode>(BlendNodeType::Additive);
    node->set_base_node(std::move(base));
    node->set_additive_node(std::move(additive));
    node->set_additive_weight_parameter(weight_parameter);
    return node;
}

} // namespace BlendTreeFactory

} // namespace engine::render
