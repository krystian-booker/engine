#include <catch2/catch_test_macros.hpp>
#include <engine/render/bone_mask_presets.hpp>
#include <algorithm>

using namespace engine::render;

// Helper to create a humanoid skeleton for testing
Skeleton create_test_humanoid_skeleton() {
    Skeleton skeleton;

    // Root
    int32_t hips = skeleton.add_bone("Hips", -1);

    // Spine chain
    int32_t spine = skeleton.add_bone("Spine", hips);
    int32_t spine1 = skeleton.add_bone("Spine1", spine);
    int32_t spine2 = skeleton.add_bone("Spine2", spine1);
    int32_t neck = skeleton.add_bone("Neck", spine2);
    int32_t head = skeleton.add_bone("Head", neck);

    // Left arm
    int32_t left_shoulder = skeleton.add_bone("LeftShoulder", spine2);
    int32_t left_arm = skeleton.add_bone("LeftArm", left_shoulder);
    int32_t left_forearm = skeleton.add_bone("LeftForeArm", left_arm);
    int32_t left_hand = skeleton.add_bone("LeftHand", left_forearm);
    skeleton.add_bone("LeftHandThumb1", left_hand);
    skeleton.add_bone("LeftHandIndex1", left_hand);
    skeleton.add_bone("LeftHandMiddle1", left_hand);

    // Right arm
    int32_t right_shoulder = skeleton.add_bone("RightShoulder", spine2);
    int32_t right_arm = skeleton.add_bone("RightArm", right_shoulder);
    int32_t right_forearm = skeleton.add_bone("RightForeArm", right_arm);
    int32_t right_hand = skeleton.add_bone("RightHand", right_forearm);
    skeleton.add_bone("RightHandThumb1", right_hand);
    skeleton.add_bone("RightHandIndex1", right_hand);
    skeleton.add_bone("RightHandMiddle1", right_hand);

    // Left leg
    int32_t left_upleg = skeleton.add_bone("LeftUpLeg", hips);
    int32_t left_leg = skeleton.add_bone("LeftLeg", left_upleg);
    int32_t left_foot = skeleton.add_bone("LeftFoot", left_leg);
    skeleton.add_bone("LeftToeBase", left_foot);

    // Right leg
    int32_t right_upleg = skeleton.add_bone("RightUpLeg", hips);
    int32_t right_leg = skeleton.add_bone("RightLeg", right_upleg);
    int32_t right_foot = skeleton.add_bone("RightFoot", right_leg);
    skeleton.add_bone("RightToeBase", right_foot);

    return skeleton;
}

// Helper to check if bone index is in mask
bool contains_bone(const std::vector<int32_t>& mask, int32_t bone_index) {
    return std::find(mask.begin(), mask.end(), bone_index) != mask.end();
}

TEST_CASE("BoneMaskBuilder creates valid masks", "[render][animation]") {
    Skeleton skeleton = create_test_humanoid_skeleton();

    SECTION("Include bone by name") {
        BoneMaskBuilder builder(skeleton);
        auto mask = builder.include("Head").build();

        REQUIRE(mask.size() == 1);
        REQUIRE(contains_bone(mask, skeleton.find_bone("Head")));
    }

    SECTION("Include bone and all children") {
        BoneMaskBuilder builder(skeleton);
        auto mask = builder.include_children("LeftHand").build();

        // Should include LeftHand and its finger children
        REQUIRE(mask.size() == 4);  // LeftHand + 3 fingers
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftHand")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftHandThumb1")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftHandIndex1")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftHandMiddle1")));
    }

    SECTION("Exclude specific bones") {
        BoneMaskBuilder builder(skeleton);
        auto mask = builder
            .include("Head")
            .include("Neck")
            .exclude("Head")
            .build();

        REQUIRE(mask.size() == 1);
        REQUIRE(contains_bone(mask, skeleton.find_bone("Neck")));
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("Head")));
    }

    SECTION("Exclude bone and children") {
        BoneMaskBuilder builder(skeleton);
        auto mask = builder
            .include_children("LeftArm")      // Arm, forearm, hand, fingers
            .exclude_children("LeftHand")     // Remove hand and fingers
            .build();

        // Should only have arm bones, not hand
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftArm")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftForeArm")));
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("LeftHand")));
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("LeftHandThumb1")));
    }

    SECTION("Unknown bone names are ignored") {
        BoneMaskBuilder builder(skeleton);
        auto mask = builder
            .include("NonexistentBone")
            .include("Head")
            .build();

        REQUIRE(mask.size() == 1);
        REQUIRE(contains_bone(mask, skeleton.find_bone("Head")));
    }

    SECTION("Clear removes all included bones") {
        BoneMaskBuilder builder(skeleton);
        builder.include("Head").include("Neck");
        REQUIRE(builder.count() == 2);

        builder.clear();
        REQUIRE(builder.count() == 0);

        auto mask = builder.build();
        REQUIRE(mask.empty());
    }

    SECTION("Build returns sorted indices") {
        BoneMaskBuilder builder(skeleton);
        auto mask = builder
            .include("RightArm")
            .include("LeftArm")
            .include("Head")
            .build();

        REQUIRE(mask.size() == 3);
        REQUIRE(std::is_sorted(mask.begin(), mask.end()));
    }
}

TEST_CASE("BoneMaskPresets generate correct indices", "[render][animation]") {
    Skeleton skeleton = create_test_humanoid_skeleton();

    SECTION("upper_body includes spine and above") {
        auto mask = BoneMaskPresets::upper_body(skeleton);

        // Should include spine chain
        REQUIRE(contains_bone(mask, skeleton.find_bone("Spine")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("Spine1")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("Spine2")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("Neck")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("Head")));

        // Should include arms
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftShoulder")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("RightShoulder")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftArm")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("RightArm")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftHand")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("RightHand")));

        // Should NOT include legs
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("LeftUpLeg")));
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("RightUpLeg")));
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("LeftFoot")));
    }

    SECTION("lower_body includes hips and legs") {
        auto mask = BoneMaskPresets::lower_body(skeleton);

        // Should include hips
        REQUIRE(contains_bone(mask, skeleton.find_bone("Hips")));

        // Should include legs
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftUpLeg")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("RightUpLeg")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftLeg")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("RightLeg")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftFoot")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("RightFoot")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftToeBase")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("RightToeBase")));

        // Should NOT include arms
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("LeftArm")));
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("RightArm")));
    }

    SECTION("left_arm includes shoulder to fingers") {
        auto mask = BoneMaskPresets::left_arm(skeleton);

        // Should include left arm bones
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftShoulder")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftArm")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftForeArm")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftHand")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftHandThumb1")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftHandIndex1")));

        // Should NOT include right arm
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("RightShoulder")));
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("RightArm")));
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("RightHand")));
    }

    SECTION("right_arm includes shoulder to fingers") {
        auto mask = BoneMaskPresets::right_arm(skeleton);

        // Should include right arm bones
        REQUIRE(contains_bone(mask, skeleton.find_bone("RightShoulder")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("RightArm")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("RightForeArm")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("RightHand")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("RightHandThumb1")));

        // Should NOT include left arm
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("LeftShoulder")));
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("LeftArm")));
    }

    SECTION("head_only includes head and neck") {
        auto mask = BoneMaskPresets::head_only(skeleton, true);

        REQUIRE(contains_bone(mask, skeleton.find_bone("Head")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("Neck")));

        // Should NOT include spine
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("Spine")));
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("Spine2")));
    }

    SECTION("head_only without neck") {
        auto mask = BoneMaskPresets::head_only(skeleton, false);

        REQUIRE(contains_bone(mask, skeleton.find_bone("Head")));
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("Neck")));
    }

    SECTION("spine_chain includes central chain") {
        auto mask = BoneMaskPresets::spine_chain(skeleton);

        REQUIRE(contains_bone(mask, skeleton.find_bone("Hips")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("Spine")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("Spine1")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("Spine2")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("Neck")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("Head")));

        // Should NOT include limbs
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("LeftArm")));
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("RightArm")));
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("LeftUpLeg")));
    }

    SECTION("full_body includes all bones") {
        auto mask = BoneMaskPresets::full_body(skeleton);

        REQUIRE(mask.size() == static_cast<size_t>(skeleton.get_bone_count()));
    }

    SECTION("hands_only includes finger bones") {
        auto mask = BoneMaskPresets::hands_only(skeleton);

        // Should include hand bones
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftHand")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("RightHand")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftHandThumb1")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("RightHandIndex1")));

        // Should NOT include arms
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("LeftArm")));
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("RightArm")));
    }

    SECTION("left_hand only") {
        auto mask = BoneMaskPresets::left_hand(skeleton);

        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftHand")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftHandThumb1")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("LeftHandIndex1")));

        // Should NOT include right hand
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("RightHand")));
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("RightHandThumb1")));
    }

    SECTION("right_hand only") {
        auto mask = BoneMaskPresets::right_hand(skeleton);

        REQUIRE(contains_bone(mask, skeleton.find_bone("RightHand")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("RightHandThumb1")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("RightHandIndex1")));

        // Should NOT include left hand
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("LeftHand")));
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("LeftHandThumb1")));
    }
}

TEST_CASE("BoneNamePatterns case insensitive matching", "[render][animation]") {
    SECTION("Spine patterns") {
        REQUIRE(BoneNamePatterns::is_spine_bone("Spine"));
        REQUIRE(BoneNamePatterns::is_spine_bone("SPINE"));
        REQUIRE(BoneNamePatterns::is_spine_bone("spine_01"));
        REQUIRE(BoneNamePatterns::is_spine_bone("Chest"));
        REQUIRE(BoneNamePatterns::is_spine_bone("chest_upper"));
        REQUIRE_FALSE(BoneNamePatterns::is_spine_bone("LeftArm"));
    }

    SECTION("Head patterns") {
        REQUIRE(BoneNamePatterns::is_head_bone("Head"));
        REQUIRE(BoneNamePatterns::is_head_bone("head"));
        REQUIRE(BoneNamePatterns::is_head_bone("Jaw"));
        REQUIRE(BoneNamePatterns::is_head_bone("LeftEye"));
        REQUIRE_FALSE(BoneNamePatterns::is_head_bone("Neck"));
    }

    SECTION("Neck patterns") {
        REQUIRE(BoneNamePatterns::is_neck_bone("Neck"));
        REQUIRE(BoneNamePatterns::is_neck_bone("neck_01"));
        REQUIRE_FALSE(BoneNamePatterns::is_neck_bone("Head"));
    }

    SECTION("Left arm patterns") {
        REQUIRE(BoneNamePatterns::is_left_arm_bone("LeftShoulder"));
        REQUIRE(BoneNamePatterns::is_left_arm_bone("Left_Arm"));
        REQUIRE(BoneNamePatterns::is_left_arm_bone("arm_l"));
        REQUIRE(BoneNamePatterns::is_left_arm_bone("clavicle.l"));
        REQUIRE_FALSE(BoneNamePatterns::is_left_arm_bone("RightShoulder"));
        REQUIRE_FALSE(BoneNamePatterns::is_left_arm_bone("LeftLeg"));
    }

    SECTION("Right arm patterns") {
        REQUIRE(BoneNamePatterns::is_right_arm_bone("RightShoulder"));
        REQUIRE(BoneNamePatterns::is_right_arm_bone("Right_Arm"));
        REQUIRE(BoneNamePatterns::is_right_arm_bone("arm_r"));
        REQUIRE_FALSE(BoneNamePatterns::is_right_arm_bone("LeftShoulder"));
    }

    SECTION("Left leg patterns") {
        REQUIRE(BoneNamePatterns::is_left_leg_bone("LeftUpLeg"));
        REQUIRE(BoneNamePatterns::is_left_leg_bone("Left_Thigh"));
        REQUIRE(BoneNamePatterns::is_left_leg_bone("calf_l"));
        REQUIRE(BoneNamePatterns::is_left_leg_bone("LeftFoot"));
        REQUIRE_FALSE(BoneNamePatterns::is_left_leg_bone("RightLeg"));
    }

    SECTION("Right leg patterns") {
        REQUIRE(BoneNamePatterns::is_right_leg_bone("RightUpLeg"));
        REQUIRE(BoneNamePatterns::is_right_leg_bone("Right_Thigh"));
        REQUIRE(BoneNamePatterns::is_right_leg_bone("foot_r"));
        REQUIRE_FALSE(BoneNamePatterns::is_right_leg_bone("LeftLeg"));
    }

    SECTION("Hip patterns") {
        REQUIRE(BoneNamePatterns::is_hip_bone("Hips"));
        REQUIRE(BoneNamePatterns::is_hip_bone("pelvis"));
        REQUIRE(BoneNamePatterns::is_hip_bone("Root"));
        REQUIRE_FALSE(BoneNamePatterns::is_hip_bone("Spine"));
    }

    SECTION("Shoulder patterns") {
        REQUIRE(BoneNamePatterns::is_shoulder_bone("LeftShoulder"));
        REQUIRE(BoneNamePatterns::is_shoulder_bone("Clavicle_R"));
        REQUIRE_FALSE(BoneNamePatterns::is_shoulder_bone("LeftArm"));
    }

    SECTION("Hand patterns") {
        REQUIRE(BoneNamePatterns::is_hand_bone("LeftHand"));
        REQUIRE(BoneNamePatterns::is_hand_bone("hand_r"));
        REQUIRE(BoneNamePatterns::is_hand_bone("LeftHandThumb1"));
        REQUIRE(BoneNamePatterns::is_hand_bone("Index_Finger_01"));
        REQUIRE_FALSE(BoneNamePatterns::is_hand_bone("LeftForeArm"));
    }
}

TEST_CASE("BoneMaskBuilder chaining", "[render][animation]") {
    Skeleton skeleton = create_test_humanoid_skeleton();

    SECTION("Multiple includes can be chained") {
        BoneMaskBuilder builder(skeleton);
        auto mask = builder
            .include("Head")
            .include("Neck")
            .include("Spine")
            .build();

        REQUIRE(mask.size() == 3);
    }

    SECTION("Include and exclude can be chained") {
        BoneMaskBuilder builder(skeleton);
        auto mask = builder
            .include_children("Spine2")  // Includes shoulders, arms, neck, head
            .exclude_children("LeftShoulder")  // Remove left arm chain
            .exclude_children("RightShoulder")  // Remove right arm chain
            .build();

        // Should have spine2, neck, head
        REQUIRE(contains_bone(mask, skeleton.find_bone("Spine2")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("Neck")));
        REQUIRE(contains_bone(mask, skeleton.find_bone("Head")));

        // Arms should be excluded
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("LeftShoulder")));
        REQUIRE_FALSE(contains_bone(mask, skeleton.find_bone("RightArm")));
    }
}

TEST_CASE("Empty skeleton handling", "[render][animation]") {
    Skeleton empty_skeleton;

    SECTION("BoneMaskBuilder with empty skeleton") {
        BoneMaskBuilder builder(empty_skeleton);
        auto mask = builder.include("NonexistentBone").build();

        REQUIRE(mask.empty());
    }

    SECTION("BoneMaskPresets with empty skeleton return empty") {
        REQUIRE(BoneMaskPresets::upper_body(empty_skeleton).empty());
        REQUIRE(BoneMaskPresets::lower_body(empty_skeleton).empty());
        REQUIRE(BoneMaskPresets::left_arm(empty_skeleton).empty());
        REQUIRE(BoneMaskPresets::right_arm(empty_skeleton).empty());
        REQUIRE(BoneMaskPresets::head_only(empty_skeleton).empty());
        REQUIRE(BoneMaskPresets::full_body(empty_skeleton).empty());
    }
}
