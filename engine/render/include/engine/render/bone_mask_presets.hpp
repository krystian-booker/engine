#pragma once

#include <engine/render/skeleton.hpp>
#include <string>
#include <vector>
#include <unordered_set>

namespace engine::render {

// Builder for creating bone masks programmatically
class BoneMaskBuilder {
public:
    explicit BoneMaskBuilder(const Skeleton& skeleton);

    // Include a specific bone by name (returns *this for chaining)
    BoneMaskBuilder& include(const std::string& bone_name);

    // Include a bone and all its children recursively
    BoneMaskBuilder& include_children(const std::string& bone_name);

    // Exclude a specific bone (removes from included set)
    BoneMaskBuilder& exclude(const std::string& bone_name);

    // Exclude a bone and all its children recursively
    BoneMaskBuilder& exclude_children(const std::string& bone_name);

    // Clear all included bones
    BoneMaskBuilder& clear();

    // Build the final mask as a sorted vector of bone indices
    std::vector<int32_t> build() const;

    // Get count of included bones
    size_t count() const { return m_included_bones.size(); }

private:
    void collect_children(int32_t bone_index, std::unordered_set<int32_t>& out) const;

    const Skeleton& m_skeleton;
    std::unordered_set<int32_t> m_included_bones;
};

// Pre-built bone mask presets for common humanoid skeletons
// These work with standard bone naming conventions (Mixamo, UE4, Unity style)
namespace BoneMaskPresets {

    // Upper body: spine and everything above (arms, head, neck)
    // Includes: spine, chest, shoulders, arms, hands, neck, head
    std::vector<int32_t> upper_body(const Skeleton& skeleton);

    // Lower body: hips/pelvis and legs
    // Includes: hips/pelvis, thighs, knees, feet, toes
    std::vector<int32_t> lower_body(const Skeleton& skeleton);

    // Left arm only: shoulder to fingertips
    std::vector<int32_t> left_arm(const Skeleton& skeleton);

    // Right arm only: shoulder to fingertips
    std::vector<int32_t> right_arm(const Skeleton& skeleton);

    // Head only: head and optionally neck
    std::vector<int32_t> head_only(const Skeleton& skeleton, bool include_neck = true);

    // Spine chain: all spine bones from pelvis to head (not including limbs)
    std::vector<int32_t> spine_chain(const Skeleton& skeleton);

    // Full body (all bones) - useful as a starting point for exclusions
    std::vector<int32_t> full_body(const Skeleton& skeleton);

    // Hands only (fingers)
    std::vector<int32_t> hands_only(const Skeleton& skeleton);

    // Left hand only
    std::vector<int32_t> left_hand(const Skeleton& skeleton);

    // Right hand only
    std::vector<int32_t> right_hand(const Skeleton& skeleton);

} // namespace BoneMaskPresets

// Common bone name patterns used for detection
// Skeletons may use different conventions (Mixamo, UE4, Unity, custom)
namespace BoneNamePatterns {

    // Check if bone name matches any of the common patterns for a body part
    bool is_spine_bone(const std::string& name);
    bool is_head_bone(const std::string& name);
    bool is_neck_bone(const std::string& name);
    bool is_left_arm_bone(const std::string& name);
    bool is_right_arm_bone(const std::string& name);
    bool is_left_leg_bone(const std::string& name);
    bool is_right_leg_bone(const std::string& name);
    bool is_hip_bone(const std::string& name);
    bool is_shoulder_bone(const std::string& name);
    bool is_hand_bone(const std::string& name);

    // Case-insensitive check if name contains any of the patterns
    bool contains_any(const std::string& name, const std::vector<std::string>& patterns);

} // namespace BoneNamePatterns

} // namespace engine::render
