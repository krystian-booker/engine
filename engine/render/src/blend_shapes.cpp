#include <engine/render/blend_shapes.hpp>
#include <algorithm>
#include <cmath>
#include <random>

namespace engine::render {

// MorphTarget implementation

MorphTarget::MorphTarget(const std::string& name)
    : m_name(name)
{
}

void MorphTarget::add_delta(const MorphTargetDelta& delta) {
    m_vertex_to_delta_index[delta.vertex_index] = m_deltas.size();
    m_deltas.push_back(delta);
}

void MorphTarget::add_delta(uint32_t vertex_index, const Vec3& position_delta,
                            const Vec3& normal_delta, const Vec3& tangent_delta) {
    add_delta(MorphTargetDelta{vertex_index, position_delta, normal_delta, tangent_delta});
}

void MorphTarget::set_deltas(std::vector<MorphTargetDelta> deltas) {
    m_deltas = std::move(deltas);
    m_vertex_to_delta_index.clear();
    for (size_t i = 0; i < m_deltas.size(); ++i) {
        m_vertex_to_delta_index[m_deltas[i].vertex_index] = i;
    }
}

void MorphTarget::clear() {
    m_deltas.clear();
    m_vertex_to_delta_index.clear();
}

bool MorphTarget::affects_vertex(uint32_t vertex_index) const {
    return m_vertex_to_delta_index.find(vertex_index) != m_vertex_to_delta_index.end();
}

const MorphTargetDelta* MorphTarget::get_delta_for_vertex(uint32_t vertex_index) const {
    auto it = m_vertex_to_delta_index.find(vertex_index);
    if (it != m_vertex_to_delta_index.end()) {
        return &m_deltas[it->second];
    }
    return nullptr;
}

// BlendShapeSet implementation

BlendShapeSet::~BlendShapeSet() = default;

void BlendShapeSet::add_morph_target(MorphTarget target) {
    const std::string& name = target.get_name();
    m_name_to_index[name] = m_targets.size();
    m_targets.push_back(std::move(target));
}

void BlendShapeSet::add_morph_target(const std::string& name, std::vector<MorphTargetDelta> deltas) {
    MorphTarget target(name);
    target.set_deltas(std::move(deltas));
    add_morph_target(std::move(target));
}

void BlendShapeSet::remove_morph_target(const std::string& name) {
    auto it = m_name_to_index.find(name);
    if (it == m_name_to_index.end()) return;

    size_t index = it->second;
    m_targets.erase(m_targets.begin() + index);
    m_name_to_index.erase(it);

    // Rebuild index map
    m_name_to_index.clear();
    for (size_t i = 0; i < m_targets.size(); ++i) {
        m_name_to_index[m_targets[i].get_name()] = i;
    }
}

void BlendShapeSet::clear() {
    m_targets.clear();
    m_name_to_index.clear();
}

MorphTarget* BlendShapeSet::get_target(const std::string& name) {
    auto it = m_name_to_index.find(name);
    return it != m_name_to_index.end() ? &m_targets[it->second] : nullptr;
}

const MorphTarget* BlendShapeSet::get_target(const std::string& name) const {
    auto it = m_name_to_index.find(name);
    return it != m_name_to_index.end() ? &m_targets[it->second] : nullptr;
}

MorphTarget* BlendShapeSet::get_target(size_t index) {
    return index < m_targets.size() ? &m_targets[index] : nullptr;
}

const MorphTarget* BlendShapeSet::get_target(size_t index) const {
    return index < m_targets.size() ? &m_targets[index] : nullptr;
}

int BlendShapeSet::find_target_index(const std::string& name) const {
    auto it = m_name_to_index.find(name);
    return it != m_name_to_index.end() ? static_cast<int>(it->second) : -1;
}

std::vector<std::string> BlendShapeSet::get_target_names() const {
    std::vector<std::string> names;
    names.reserve(m_targets.size());
    for (const auto& target : m_targets) {
        names.push_back(target.get_name());
    }
    return names;
}

// BlendShapeInstance implementation

BlendShapeInstance::BlendShapeInstance(std::shared_ptr<BlendShapeSet> shape_set)
    : m_shape_set(shape_set)
{
    if (m_shape_set) {
        size_t count = m_shape_set->get_target_count();
        m_weights.resize(count, 0.0f);
        m_target_weights.resize(count, 0.0f);
        m_weight_speeds.resize(count, 5.0f);
    }
}

void BlendShapeInstance::set_shape_set(std::shared_ptr<BlendShapeSet> shape_set) {
    m_shape_set = shape_set;
    if (m_shape_set) {
        size_t count = m_shape_set->get_target_count();
        m_weights.resize(count, 0.0f);
        m_target_weights.resize(count, 0.0f);
        m_weight_speeds.resize(count, 5.0f);
    } else {
        m_weights.clear();
        m_target_weights.clear();
        m_weight_speeds.clear();
    }
}

void BlendShapeInstance::set_weight(const std::string& name, float weight) {
    if (!m_shape_set) return;
    int index = m_shape_set->find_target_index(name);
    if (index >= 0) {
        set_weight(static_cast<size_t>(index), weight);
    }
}

void BlendShapeInstance::set_weight(size_t index, float weight) {
    if (index < m_weights.size()) {
        m_weights[index] = weight;
        m_target_weights[index] = weight;
    }
}

float BlendShapeInstance::get_weight(const std::string& name) const {
    if (!m_shape_set) return 0.0f;
    int index = m_shape_set->find_target_index(name);
    return index >= 0 ? get_weight(static_cast<size_t>(index)) : 0.0f;
}

float BlendShapeInstance::get_weight(size_t index) const {
    return index < m_weights.size() ? m_weights[index] : 0.0f;
}

void BlendShapeInstance::set_target_weight(const std::string& name, float target, float speed) {
    if (!m_shape_set) return;
    int index = m_shape_set->find_target_index(name);
    if (index >= 0 && static_cast<size_t>(index) < m_target_weights.size()) {
        m_target_weights[index] = target;
        m_weight_speeds[index] = speed;
    }
}

void BlendShapeInstance::update_weights(float delta_time) {
    for (size_t i = 0; i < m_weights.size(); ++i) {
        if (std::abs(m_weights[i] - m_target_weights[i]) > 0.001f) {
            float diff = m_target_weights[i] - m_weights[i];
            float change = m_weight_speeds[i] * delta_time;
            if (std::abs(diff) < change) {
                m_weights[i] = m_target_weights[i];
            } else {
                m_weights[i] += (diff > 0 ? change : -change);
            }
        }
    }
}

void BlendShapeInstance::reset_all_weights() {
    std::fill(m_weights.begin(), m_weights.end(), 0.0f);
    std::fill(m_target_weights.begin(), m_target_weights.end(), 0.0f);
}

bool BlendShapeInstance::has_active_weights() const {
    for (float w : m_weights) {
        if (std::abs(w) > 0.001f) return true;
    }
    return false;
}

size_t BlendShapeInstance::get_active_weight_count() const {
    size_t count = 0;
    for (float w : m_weights) {
        if (std::abs(w) > 0.001f) ++count;
    }
    return count;
}

// BlendShapeDeformer implementation

void BlendShapeDeformer::apply(
    const BlendShapeInstance& instance,
    const std::vector<Vertex>& base_vertices,
    std::vector<Vertex>& out_vertices
) {
    // Copy base vertices
    out_vertices = base_vertices;

    if (!instance.has_active_weights()) {
        return;
    }

    auto shape_set = instance.get_shape_set();
    if (!shape_set) return;

    const auto& weights = instance.get_weights();

    // Collect active shapes (non-zero weight)
    std::vector<std::pair<size_t, float>> active_shapes;
    for (size_t i = 0; i < weights.size() && i < shape_set->get_target_count(); ++i) {
        if (std::abs(weights[i]) > 0.001f) {
            active_shapes.emplace_back(i, weights[i]);
        }
    }

    // Sort by weight magnitude (largest first for better accuracy)
    std::sort(active_shapes.begin(), active_shapes.end(),
        [](const auto& a, const auto& b) { return std::abs(a.second) > std::abs(b.second); });

    // Limit to max active shapes
    if (active_shapes.size() > m_max_active_shapes) {
        active_shapes.resize(m_max_active_shapes);
    }

    // Apply each active morph target
    for (const auto& [shape_index, weight] : active_shapes) {
        const MorphTarget* target = shape_set->get_target(shape_index);
        if (!target) continue;

        for (const auto& delta : target->get_deltas()) {
            if (delta.vertex_index >= out_vertices.size()) continue;

            Vertex& v = out_vertices[delta.vertex_index];
            v.position += delta.position_delta * weight;
            v.normal += delta.normal_delta * weight;
            v.tangent += delta.tangent_delta * weight;
        }
    }

    // Re-normalize normals and tangents
    for (auto& v : out_vertices) {
        float normal_len = glm::length(v.normal);
        if (normal_len > 0.001f) {
            v.normal /= normal_len;
        }
        float tangent_len = glm::length(v.tangent);
        if (tangent_len > 0.001f) {
            v.tangent /= tangent_len;
        }
    }
}

void BlendShapeDeformer::apply_positions(
    const BlendShapeInstance& instance,
    const std::vector<Vec3>& base_positions,
    std::vector<Vec3>& out_positions
) {
    out_positions = base_positions;

    if (!instance.has_active_weights()) {
        return;
    }

    auto shape_set = instance.get_shape_set();
    if (!shape_set) return;

    const auto& weights = instance.get_weights();

    for (size_t i = 0; i < weights.size() && i < shape_set->get_target_count(); ++i) {
        if (std::abs(weights[i]) < 0.001f) continue;

        const MorphTarget* target = shape_set->get_target(i);
        if (!target) continue;

        for (const auto& delta : target->get_deltas()) {
            if (delta.vertex_index >= out_positions.size()) continue;
            out_positions[delta.vertex_index] += delta.position_delta * weights[i];
        }
    }
}

// BlendShapeComponent implementation

void BlendShapeComponent::apply_preset(const std::string& preset_name, float blend) {
    for (const auto& preset : presets) {
        if (preset.name == preset_name) {
            for (const auto& [shape_name, weight] : preset.weights) {
                float current = instance.get_weight(shape_name);
                instance.set_weight(shape_name, glm::mix(current, weight, blend));
            }
            vertices_dirty = true;
            return;
        }
    }
}

// FacialExpressionController implementation

FacialExpressionController::FacialExpressionController(BlendShapeInstance* instance)
    : m_instance(instance)
{
    // Initialize random blink timing
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(m_blink_min_interval, m_blink_max_interval);
    m_next_blink_time = dist(gen);
}

void FacialExpressionController::map_expression(
    Expression expr,
    const std::vector<std::pair<std::string, float>>& shapes
) {
    m_expression_maps[expr] = shapes;
}

void FacialExpressionController::set_expression(Expression expr, float weight, float blend_time) {
    if (expr == m_target_expression && std::abs(weight - m_target_expression_weight) < 0.001f) {
        return;
    }

    m_target_expression = expr;
    m_target_expression_weight = weight;
    m_expression_blend_time = blend_time;
    m_expression_blend_progress = 0.0f;
}

void FacialExpressionController::set_viseme(Expression viseme, float weight) {
    m_current_viseme = viseme;
    m_viseme_weight = weight;
}

void FacialExpressionController::set_blink_interval(float min_seconds, float max_seconds) {
    m_blink_min_interval = min_seconds;
    m_blink_max_interval = max_seconds;
}

void FacialExpressionController::trigger_blink() {
    if (!m_is_blinking) {
        m_is_blinking = true;
        m_blink_progress = 0.0f;
    }
}

void FacialExpressionController::update(float delta_time) {
    if (!m_instance) return;

    update_expression_blend(delta_time);
    update_blink(delta_time);
}

void FacialExpressionController::update_expression_blend(float delta_time) {
    // Update expression blend progress
    if (m_expression_blend_progress < 1.0f) {
        if (m_expression_blend_time > 0.0f) {
            m_expression_blend_progress += delta_time / m_expression_blend_time;
            m_expression_blend_progress = std::min(m_expression_blend_progress, 1.0f);
        } else {
            m_expression_blend_progress = 1.0f;
        }
    }

    // Smoothstep for nicer blending
    float t = m_expression_blend_progress;
    float smooth_t = t * t * (3.0f - 2.0f * t);

    // Clear previous expression shapes
    auto it = m_expression_maps.find(m_current_expression);
    if (it != m_expression_maps.end()) {
        for (const auto& [shape, target_weight] : it->second) {
            float current = m_instance->get_weight(shape);
            float blended = glm::mix(current, 0.0f, smooth_t);
            m_instance->set_weight(shape, blended);
        }
    }

    // Apply target expression shapes
    it = m_expression_maps.find(m_target_expression);
    if (it != m_expression_maps.end()) {
        for (const auto& [shape, base_weight] : it->second) {
            float target = base_weight * m_target_expression_weight;
            float current = m_instance->get_weight(shape);
            float blended = glm::mix(current, target, smooth_t);
            m_instance->set_weight(shape, blended);
        }
    }

    // Apply viseme (additive to expression)
    it = m_expression_maps.find(m_current_viseme);
    if (it != m_expression_maps.end()) {
        for (const auto& [shape, base_weight] : it->second) {
            float current = m_instance->get_weight(shape);
            m_instance->set_weight(shape, current + base_weight * m_viseme_weight);
        }
    }

    // Update current expression when blend is complete
    if (m_expression_blend_progress >= 1.0f) {
        m_current_expression = m_target_expression;
        m_expression_weight = m_target_expression_weight;
    }
}

void FacialExpressionController::update_blink(float delta_time) {
    if (!m_blink_enabled) return;

    // Auto-blink timer
    if (!m_is_blinking) {
        m_blink_timer += delta_time;
        if (m_blink_timer >= m_next_blink_time) {
            trigger_blink();
            m_blink_timer = 0.0f;

            // Randomize next blink time
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::uniform_real_distribution<float> dist(m_blink_min_interval, m_blink_max_interval);
            m_next_blink_time = dist(gen);
        }
    }

    // Animate blink
    if (m_is_blinking) {
        m_blink_progress += delta_time / m_blink_duration;

        float blink_weight = 0.0f;
        if (m_blink_progress < 0.5f) {
            // Closing
            blink_weight = m_blink_progress * 2.0f;
        } else if (m_blink_progress < 1.0f) {
            // Opening
            blink_weight = 1.0f - (m_blink_progress - 0.5f) * 2.0f;
        } else {
            // Done
            m_is_blinking = false;
            blink_weight = 0.0f;
        }

        // Apply blink shape
        m_instance->set_weight(m_blink_shape, blink_weight);
    }
}

void FacialExpressionController::reset() {
    m_current_expression = Expression::Neutral;
    m_target_expression = Expression::Neutral;
    m_expression_weight = 0.0f;
    m_target_expression_weight = 0.0f;
    m_expression_blend_progress = 1.0f;
    m_current_viseme = Expression::Viseme_sil;
    m_viseme_weight = 0.0f;
    m_is_blinking = false;

    if (m_instance) {
        m_instance->reset_all_weights();
    }
}

} // namespace engine::render
