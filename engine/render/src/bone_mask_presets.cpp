#include <engine/render/bone_mask_presets.hpp>
#include <algorithm>
#include <cctype>

namespace engine::render {

namespace {

// Convert string to lowercase for case-insensitive comparison
std::string to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

} // anonymous namespace

// ============================================================================
// BoneMaskBuilder
// ============================================================================

BoneMaskBuilder::BoneMaskBuilder(const Skeleton& skeleton)
    : m_skeleton(skeleton) {
}

BoneMaskBuilder& BoneMaskBuilder::include(const std::string& bone_name) {
    int32_t index = m_skeleton.find_bone(bone_name);
    if (index >= 0) {
        m_included_bones.insert(index);
    }
    return *this;
}

BoneMaskBuilder& BoneMaskBuilder::include_children(const std::string& bone_name) {
    int32_t index = m_skeleton.find_bone(bone_name);
    if (index >= 0) {
        m_included_bones.insert(index);
        collect_children(index, m_included_bones);
    }
    return *this;
}

BoneMaskBuilder& BoneMaskBuilder::exclude(const std::string& bone_name) {
    int32_t index = m_skeleton.find_bone(bone_name);
    if (index >= 0) {
        m_included_bones.erase(index);
    }
    return *this;
}

BoneMaskBuilder& BoneMaskBuilder::exclude_children(const std::string& bone_name) {
    int32_t index = m_skeleton.find_bone(bone_name);
    if (index >= 0) {
        m_included_bones.erase(index);

        std::unordered_set<int32_t> children_to_remove;
        collect_children(index, children_to_remove);
        for (int32_t child : children_to_remove) {
            m_included_bones.erase(child);
        }
    }
    return *this;
}

BoneMaskBuilder& BoneMaskBuilder::clear() {
    m_included_bones.clear();
    return *this;
}

std::vector<int32_t> BoneMaskBuilder::build() const {
    std::vector<int32_t> result(m_included_bones.begin(), m_included_bones.end());
    std::sort(result.begin(), result.end());
    return result;
}

void BoneMaskBuilder::collect_children(int32_t bone_index, std::unordered_set<int32_t>& out) const {
    const auto& bone = m_skeleton.get_bone(bone_index);
    for (int32_t child_index : bone.children) {
        out.insert(child_index);
        collect_children(child_index, out);
    }
}

// ============================================================================
// BoneNamePatterns
// ============================================================================

namespace BoneNamePatterns {

bool contains_any(const std::string& name, const std::vector<std::string>& patterns) {
    std::string lower_name = to_lower(name);
    for (const auto& pattern : patterns) {
        if (lower_name.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool is_spine_bone(const std::string& name) {
    static const std::vector<std::string> patterns = {
        "spine", "chest", "abdomen", "torso", "back", "pelvis"
    };
    return contains_any(name, patterns);
}

bool is_head_bone(const std::string& name) {
    static const std::vector<std::string> patterns = {
        "head", "skull", "jaw", "eye", "brow"
    };
    return contains_any(name, patterns);
}

bool is_neck_bone(const std::string& name) {
    static const std::vector<std::string> patterns = {
        "neck"
    };
    return contains_any(name, patterns);
}

bool is_left_arm_bone(const std::string& name) {
    std::string lower = to_lower(name);
    // Must contain left indicator AND arm-related part
    bool is_left = lower.find("left") != std::string::npos ||
                   lower.find("_l_") != std::string::npos ||
                   lower.find(".l") != std::string::npos ||
                   (lower.length() > 2 && lower.substr(lower.length() - 2) == "_l");

    static const std::vector<std::string> arm_patterns = {
        "shoulder", "clavicle", "arm", "elbow", "forearm", "wrist", "hand", "finger", "thumb"
    };

    return is_left && contains_any(name, arm_patterns);
}

bool is_right_arm_bone(const std::string& name) {
    std::string lower = to_lower(name);
    // Must contain right indicator AND arm-related part
    bool is_right = lower.find("right") != std::string::npos ||
                    lower.find("_r_") != std::string::npos ||
                    lower.find(".r") != std::string::npos ||
                    (lower.length() > 2 && lower.substr(lower.length() - 2) == "_r");

    static const std::vector<std::string> arm_patterns = {
        "shoulder", "clavicle", "arm", "elbow", "forearm", "wrist", "hand", "finger", "thumb"
    };

    return is_right && contains_any(name, arm_patterns);
}

bool is_left_leg_bone(const std::string& name) {
    std::string lower = to_lower(name);
    bool is_left = lower.find("left") != std::string::npos ||
                   lower.find("_l_") != std::string::npos ||
                   lower.find(".l") != std::string::npos ||
                   (lower.length() > 2 && lower.substr(lower.length() - 2) == "_l");

    static const std::vector<std::string> leg_patterns = {
        "thigh", "upleg", "leg", "knee", "shin", "calf", "ankle", "foot", "toe", "ball"
    };

    return is_left && contains_any(name, leg_patterns);
}

bool is_right_leg_bone(const std::string& name) {
    std::string lower = to_lower(name);
    bool is_right = lower.find("right") != std::string::npos ||
                    lower.find("_r_") != std::string::npos ||
                    lower.find(".r") != std::string::npos ||
                    (lower.length() > 2 && lower.substr(lower.length() - 2) == "_r");

    static const std::vector<std::string> leg_patterns = {
        "thigh", "upleg", "leg", "knee", "shin", "calf", "ankle", "foot", "toe", "ball"
    };

    return is_right && contains_any(name, leg_patterns);
}

bool is_hip_bone(const std::string& name) {
    static const std::vector<std::string> patterns = {
        "hip", "pelvis", "root"
    };
    return contains_any(name, patterns);
}

bool is_shoulder_bone(const std::string& name) {
    static const std::vector<std::string> patterns = {
        "shoulder", "clavicle"
    };
    return contains_any(name, patterns);
}

bool is_hand_bone(const std::string& name) {
    static const std::vector<std::string> patterns = {
        "hand", "finger", "thumb", "index", "middle", "ring", "pinky", "metacarpal", "phalange"
    };
    return contains_any(name, patterns);
}

} // namespace BoneNamePatterns

// ============================================================================
// BoneMaskPresets
// ============================================================================

namespace BoneMaskPresets {

std::vector<int32_t> upper_body(const Skeleton& skeleton) {
    std::vector<int32_t> result;

    for (int32_t i = 0; i < skeleton.get_bone_count(); ++i) {
        const auto& bone = skeleton.get_bone(i);
        const std::string& name = bone.name;

        // Include spine (except hips), chest, arms, hands, neck, head
        if (BoneNamePatterns::is_spine_bone(name) ||
            BoneNamePatterns::is_left_arm_bone(name) ||
            BoneNamePatterns::is_right_arm_bone(name) ||
            BoneNamePatterns::is_neck_bone(name) ||
            BoneNamePatterns::is_head_bone(name) ||
            BoneNamePatterns::is_shoulder_bone(name) ||
            BoneNamePatterns::is_hand_bone(name)) {
            result.push_back(i);
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::vector<int32_t> lower_body(const Skeleton& skeleton) {
    std::vector<int32_t> result;

    for (int32_t i = 0; i < skeleton.get_bone_count(); ++i) {
        const auto& bone = skeleton.get_bone(i);
        const std::string& name = bone.name;

        // Include hips/pelvis and legs
        if (BoneNamePatterns::is_hip_bone(name) ||
            BoneNamePatterns::is_left_leg_bone(name) ||
            BoneNamePatterns::is_right_leg_bone(name)) {
            result.push_back(i);
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::vector<int32_t> left_arm(const Skeleton& skeleton) {
    std::vector<int32_t> result;

    for (int32_t i = 0; i < skeleton.get_bone_count(); ++i) {
        const auto& bone = skeleton.get_bone(i);
        if (BoneNamePatterns::is_left_arm_bone(bone.name)) {
            result.push_back(i);
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::vector<int32_t> right_arm(const Skeleton& skeleton) {
    std::vector<int32_t> result;

    for (int32_t i = 0; i < skeleton.get_bone_count(); ++i) {
        const auto& bone = skeleton.get_bone(i);
        if (BoneNamePatterns::is_right_arm_bone(bone.name)) {
            result.push_back(i);
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::vector<int32_t> head_only(const Skeleton& skeleton, bool include_neck) {
    std::vector<int32_t> result;

    for (int32_t i = 0; i < skeleton.get_bone_count(); ++i) {
        const auto& bone = skeleton.get_bone(i);
        if (BoneNamePatterns::is_head_bone(bone.name)) {
            result.push_back(i);
        }
        if (include_neck && BoneNamePatterns::is_neck_bone(bone.name)) {
            result.push_back(i);
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::vector<int32_t> spine_chain(const Skeleton& skeleton) {
    std::vector<int32_t> result;

    for (int32_t i = 0; i < skeleton.get_bone_count(); ++i) {
        const auto& bone = skeleton.get_bone(i);
        const std::string& name = bone.name;

        // Include spine, neck, head (the central chain without limbs)
        if (BoneNamePatterns::is_spine_bone(name) ||
            BoneNamePatterns::is_neck_bone(name) ||
            BoneNamePatterns::is_head_bone(name) ||
            BoneNamePatterns::is_hip_bone(name)) {
            result.push_back(i);
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::vector<int32_t> full_body(const Skeleton& skeleton) {
    std::vector<int32_t> result;
    result.reserve(skeleton.get_bone_count());

    for (int32_t i = 0; i < skeleton.get_bone_count(); ++i) {
        result.push_back(i);
    }

    return result;  // Already sorted
}

std::vector<int32_t> hands_only(const Skeleton& skeleton) {
    std::vector<int32_t> result;

    for (int32_t i = 0; i < skeleton.get_bone_count(); ++i) {
        const auto& bone = skeleton.get_bone(i);
        if (BoneNamePatterns::is_hand_bone(bone.name)) {
            result.push_back(i);
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::vector<int32_t> left_hand(const Skeleton& skeleton) {
    std::vector<int32_t> result;

    for (int32_t i = 0; i < skeleton.get_bone_count(); ++i) {
        const auto& bone = skeleton.get_bone(i);
        std::string lower = to_lower(bone.name);

        bool is_left = lower.find("left") != std::string::npos ||
                       lower.find("_l_") != std::string::npos ||
                       lower.find(".l") != std::string::npos ||
                       (lower.length() > 2 && lower.substr(lower.length() - 2) == "_l");

        if (is_left && BoneNamePatterns::is_hand_bone(bone.name)) {
            result.push_back(i);
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::vector<int32_t> right_hand(const Skeleton& skeleton) {
    std::vector<int32_t> result;

    for (int32_t i = 0; i < skeleton.get_bone_count(); ++i) {
        const auto& bone = skeleton.get_bone(i);
        std::string lower = to_lower(bone.name);

        bool is_right = lower.find("right") != std::string::npos ||
                        lower.find("_r_") != std::string::npos ||
                        lower.find(".r") != std::string::npos ||
                        (lower.length() > 2 && lower.substr(lower.length() - 2) == "_r");

        if (is_right && BoneNamePatterns::is_hand_bone(bone.name)) {
            result.push_back(i);
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

} // namespace BoneMaskPresets

} // namespace engine::render
