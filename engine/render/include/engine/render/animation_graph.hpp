#pragma once

#include <engine/render/skeleton.hpp>
#include <engine/render/animation.hpp>
#include <engine/core/math.hpp>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <variant>

namespace engine::render {

using namespace engine::core;

// Forward declarations
class AnimationStateMachine;
class BlendTreeNode;

// Animation parameter types
struct AnimationParameter {
    enum class Type {
        Float,
        Int,
        Bool,
        Trigger  // Auto-resets after consumption
    };

    Type type = Type::Float;
    std::variant<float, int, bool> value;

    AnimationParameter() : type(Type::Float), value(0.0f) {}
    explicit AnimationParameter(float v) : type(Type::Float), value(v) {}
    explicit AnimationParameter(int v) : type(Type::Int), value(v) {}
    explicit AnimationParameter(bool v) : type(Type::Bool), value(v) {}

    float as_float() const {
        if (std::holds_alternative<float>(value)) return std::get<float>(value);
        if (std::holds_alternative<int>(value)) return static_cast<float>(std::get<int>(value));
        if (std::holds_alternative<bool>(value)) return std::get<bool>(value) ? 1.0f : 0.0f;
        return 0.0f;
    }

    int as_int() const {
        if (std::holds_alternative<int>(value)) return std::get<int>(value);
        if (std::holds_alternative<float>(value)) return static_cast<int>(std::get<float>(value));
        if (std::holds_alternative<bool>(value)) return std::get<bool>(value) ? 1 : 0;
        return 0;
    }

    bool as_bool() const {
        if (std::holds_alternative<bool>(value)) return std::get<bool>(value);
        if (std::holds_alternative<float>(value)) return std::get<float>(value) != 0.0f;
        if (std::holds_alternative<int>(value)) return std::get<int>(value) != 0;
        return false;
    }
};

// Context passed to animation graph nodes during evaluation
struct AnimationContext {
    // Parameter access
    std::unordered_map<std::string, AnimationParameter>* parameters = nullptr;

    // Skeleton for bone lookups
    const Skeleton* skeleton = nullptr;

    // Delta time for this frame
    float delta_time = 0.0f;

    // Helper to get parameter value
    float get_float(const std::string& name) const {
        if (parameters) {
            auto it = parameters->find(name);
            if (it != parameters->end()) {
                return it->second.as_float();
            }
        }
        return 0.0f;
    }

    int get_int(const std::string& name) const {
        if (parameters) {
            auto it = parameters->find(name);
            if (it != parameters->end()) {
                return it->second.as_int();
            }
        }
        return 0;
    }

    bool get_bool(const std::string& name) const {
        if (parameters) {
            auto it = parameters->find(name);
            if (it != parameters->end()) {
                return it->second.as_bool();
            }
        }
        return false;
    }
};

// Base interface for animation graph nodes
class IAnimGraphNode {
public:
    virtual ~IAnimGraphNode() = default;

    // Evaluate the node and output a pose
    virtual void evaluate(float dt, AnimationContext& ctx, std::vector<BoneTransform>& out_pose) = 0;

    // Get the duration of this node's animation (for looping/sync)
    virtual float get_duration() const = 0;

    // Get current playback time
    virtual float get_time() const = 0;

    // Set current playback time
    virtual void set_time(float time) = 0;

    // Reset the node to initial state
    virtual void reset() = 0;

    // Clone the node (for instancing)
    virtual std::unique_ptr<IAnimGraphNode> clone() const = 0;
};

// Simple clip node - plays a single animation clip
class ClipNode : public IAnimGraphNode {
public:
    ClipNode() = default;
    explicit ClipNode(std::shared_ptr<AnimationClip> clip);

    void set_clip(std::shared_ptr<AnimationClip> clip) { m_clip = clip; }
    std::shared_ptr<AnimationClip> get_clip() const { return m_clip; }

    void set_speed(float speed) { m_speed = speed; }
    float get_speed() const { return m_speed; }

    void set_looping(bool loop) { m_looping = loop; }
    bool is_looping() const { return m_looping; }

    // IAnimGraphNode interface
    void evaluate(float dt, AnimationContext& ctx, std::vector<BoneTransform>& out_pose) override;
    float get_duration() const override;
    float get_time() const override { return m_time; }
    void set_time(float time) override { m_time = time; }
    void reset() override;
    std::unique_ptr<IAnimGraphNode> clone() const override;

private:
    std::shared_ptr<AnimationClip> m_clip;
    float m_time = 0.0f;
    float m_speed = 1.0f;
    bool m_looping = true;
};

// Blend two poses together
void blend_poses(
    const std::vector<BoneTransform>& pose_a,
    const std::vector<BoneTransform>& pose_b,
    float blend_factor,  // 0 = pose_a, 1 = pose_b
    std::vector<BoneTransform>& out_pose
);

// Add a pose additively to a base pose
void add_pose(
    const std::vector<BoneTransform>& base_pose,
    const std::vector<BoneTransform>& additive_pose,
    float weight,
    std::vector<BoneTransform>& out_pose
);

// Apply a bone mask to blending (0 = use pose_a, 1 = use pose_b)
void blend_poses_masked(
    const std::vector<BoneTransform>& pose_a,
    const std::vector<BoneTransform>& pose_b,
    float blend_factor,
    const std::vector<float>& bone_mask,  // Per-bone blend weight
    std::vector<BoneTransform>& out_pose
);

} // namespace engine::render
