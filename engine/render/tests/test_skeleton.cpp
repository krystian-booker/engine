#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/render/skeleton.hpp>

using namespace engine::render;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("Skeleton constants", "[render][skeleton]") {
    REQUIRE(MAX_BONES == 128);
    REQUIRE(MAX_BONE_INFLUENCES == 4);
}

TEST_CASE("Bone defaults", "[render][skeleton]") {
    Bone bone;

    REQUIRE(bone.name.empty());
    REQUIRE(bone.parent_index == -1);
    REQUIRE(bone.children.empty());
}

TEST_CASE("Bone with values", "[render][skeleton]") {
    Bone bone;
    bone.name = "spine";
    bone.parent_index = 0;
    bone.children = {2, 3, 4};

    REQUIRE(bone.name == "spine");
    REQUIRE(bone.parent_index == 0);
    REQUIRE(bone.children.size() == 3);
}

TEST_CASE("BoneTransform defaults", "[render][skeleton]") {
    BoneTransform transform;

    REQUIRE_THAT(transform.position.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(transform.position.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(transform.position.z, WithinAbs(0.0f, 0.001f));

    // Identity quaternion
    REQUIRE_THAT(transform.rotation.w, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(transform.rotation.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(transform.rotation.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(transform.rotation.z, WithinAbs(0.0f, 0.001f));

    // Unit scale
    REQUIRE_THAT(transform.scale.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(transform.scale.y, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(transform.scale.z, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("BoneTransform to_matrix", "[render][skeleton]") {
    SECTION("Identity transform") {
        BoneTransform transform;
        Mat4 matrix = transform.to_matrix();

        // Should be identity matrix
        REQUIRE_THAT(matrix[0][0], WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(matrix[1][1], WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(matrix[2][2], WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(matrix[3][3], WithinAbs(1.0f, 0.001f));
    }

    SECTION("Translation") {
        BoneTransform transform;
        transform.position = Vec3{1.0f, 2.0f, 3.0f};
        Mat4 matrix = transform.to_matrix();

        // Translation in column 3
        REQUIRE_THAT(matrix[3][0], WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(matrix[3][1], WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(matrix[3][2], WithinAbs(3.0f, 0.001f));
    }

    SECTION("Scale") {
        BoneTransform transform;
        transform.scale = Vec3{2.0f, 2.0f, 2.0f};
        Mat4 matrix = transform.to_matrix();

        // Diagonal should be scaled
        REQUIRE_THAT(matrix[0][0], WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(matrix[1][1], WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(matrix[2][2], WithinAbs(2.0f, 0.001f));
    }
}

TEST_CASE("BoneTransform lerp", "[render][skeleton]") {
    BoneTransform a;
    a.position = Vec3{0.0f, 0.0f, 0.0f};
    a.scale = Vec3{1.0f, 1.0f, 1.0f};

    BoneTransform b;
    b.position = Vec3{10.0f, 10.0f, 10.0f};
    b.scale = Vec3{2.0f, 2.0f, 2.0f};

    SECTION("t = 0") {
        auto result = BoneTransform::lerp(a, b, 0.0f);
        REQUIRE_THAT(result.position.x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(result.scale.x, WithinAbs(1.0f, 0.001f));
    }

    SECTION("t = 0.5") {
        auto result = BoneTransform::lerp(a, b, 0.5f);
        REQUIRE_THAT(result.position.x, WithinAbs(5.0f, 0.001f));
        REQUIRE_THAT(result.scale.x, WithinAbs(1.5f, 0.001f));
    }

    SECTION("t = 1") {
        auto result = BoneTransform::lerp(a, b, 1.0f);
        REQUIRE_THAT(result.position.x, WithinAbs(10.0f, 0.001f));
        REQUIRE_THAT(result.scale.x, WithinAbs(2.0f, 0.001f));
    }
}

TEST_CASE("Skeleton add_bone", "[render][skeleton]") {
    Skeleton skeleton;

    int32_t root = skeleton.add_bone("root", -1);
    int32_t spine = skeleton.add_bone("spine", root);
    int32_t head = skeleton.add_bone("head", spine);

    REQUIRE(root == 0);
    REQUIRE(spine == 1);
    REQUIRE(head == 2);
    REQUIRE(skeleton.get_bone_count() == 3);
}

TEST_CASE("Skeleton find_bone", "[render][skeleton]") {
    Skeleton skeleton;
    skeleton.add_bone("root", -1);
    skeleton.add_bone("spine", 0);
    skeleton.add_bone("head", 1);

    REQUIRE(skeleton.find_bone("root") == 0);
    REQUIRE(skeleton.find_bone("spine") == 1);
    REQUIRE(skeleton.find_bone("head") == 2);
    REQUIRE(skeleton.find_bone("nonexistent") == -1);
}

TEST_CASE("Skeleton get_bone", "[render][skeleton]") {
    Skeleton skeleton;
    skeleton.add_bone("root", -1);
    skeleton.add_bone("spine", 0);

    const Bone& root = skeleton.get_bone(0);
    REQUIRE(root.name == "root");
    REQUIRE(root.parent_index == -1);

    const Bone& spine = skeleton.get_bone(1);
    REQUIRE(spine.name == "spine");
    REQUIRE(spine.parent_index == 0);
}

TEST_CASE("Skeleton get_bones", "[render][skeleton]") {
    Skeleton skeleton;
    skeleton.add_bone("root", -1);
    skeleton.add_bone("spine", 0);
    skeleton.add_bone("head", 1);

    const auto& bones = skeleton.get_bones();
    REQUIRE(bones.size() == 3);
    REQUIRE(bones[0].name == "root");
    REQUIRE(bones[1].name == "spine");
    REQUIRE(bones[2].name == "head");
}

TEST_CASE("Skeleton get_bind_pose", "[render][skeleton]") {
    Skeleton skeleton;
    skeleton.add_bone("root", -1);
    skeleton.add_bone("spine", 0);

    auto bind_pose = skeleton.get_bind_pose();
    REQUIRE(bind_pose.size() == 2);
}

TEST_CASE("SkeletonInstance default", "[render][skeleton]") {
    SkeletonInstance instance;

    REQUIRE(instance.get_skeleton() == nullptr);
    REQUIRE(instance.get_pose().empty());
}

TEST_CASE("SkeletonInstance with skeleton", "[render][skeleton]") {
    Skeleton skeleton;
    skeleton.add_bone("root", -1);
    skeleton.add_bone("spine", 0);

    SkeletonInstance instance(&skeleton);

    REQUIRE(instance.get_skeleton() == &skeleton);
    REQUIRE(instance.get_pose().size() == 2);
}

TEST_CASE("SkeletonInstance set_skeleton", "[render][skeleton]") {
    Skeleton skeleton;
    skeleton.add_bone("root", -1);
    skeleton.add_bone("spine", 0);

    SkeletonInstance instance;
    instance.set_skeleton(&skeleton);

    REQUIRE(instance.get_skeleton() == &skeleton);
}

TEST_CASE("SkeletonInstance set_bone_transform", "[render][skeleton]") {
    Skeleton skeleton;
    skeleton.add_bone("root", -1);
    skeleton.add_bone("spine", 0);

    SkeletonInstance instance(&skeleton);

    BoneTransform transform;
    transform.position = Vec3{1.0f, 2.0f, 3.0f};

    instance.set_bone_transform(0, transform);

    const auto& pose = instance.get_pose();
    REQUIRE_THAT(pose[0].position.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(pose[0].position.y, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(pose[0].position.z, WithinAbs(3.0f, 0.001f));
}

TEST_CASE("SkeletonInstance set_bone_transform by name", "[render][skeleton]") {
    Skeleton skeleton;
    skeleton.add_bone("root", -1);
    skeleton.add_bone("spine", 0);

    SkeletonInstance instance(&skeleton);

    BoneTransform transform;
    transform.position = Vec3{5.0f, 5.0f, 5.0f};

    instance.set_bone_transform("spine", transform);

    const auto& pose = instance.get_pose();
    REQUIRE_THAT(pose[1].position.x, WithinAbs(5.0f, 0.001f));
}

TEST_CASE("SkeletonInstance reset_to_bind_pose", "[render][skeleton]") {
    Skeleton skeleton;
    skeleton.add_bone("root", -1);
    skeleton.add_bone("spine", 0);

    SkeletonInstance instance(&skeleton);

    // Modify pose
    BoneTransform transform;
    transform.position = Vec3{100.0f, 100.0f, 100.0f};
    instance.set_bone_transform(0, transform);

    // Reset to bind pose
    instance.reset_to_bind_pose();

    // Should be back to default
    const auto& pose = instance.get_pose();
    REQUIRE_THAT(pose[0].position.x, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("SkinningData defaults", "[render][skeleton]") {
    SkinningData data;

    REQUIRE(data.bone_indices.x == 0);
    REQUIRE(data.bone_indices.y == 0);
    REQUIRE(data.bone_indices.z == 0);
    REQUIRE(data.bone_indices.w == 0);

    REQUIRE_THAT(data.bone_weights.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(data.bone_weights.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(data.bone_weights.z, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(data.bone_weights.w, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("SkinningData custom values", "[render][skeleton]") {
    SkinningData data;
    data.bone_indices = IVec4{0, 5, 10, 15};
    data.bone_weights = Vec4{0.6f, 0.3f, 0.08f, 0.02f};

    REQUIRE(data.bone_indices.x == 0);
    REQUIRE(data.bone_indices.y == 5);
    REQUIRE(data.bone_indices.z == 10);
    REQUIRE(data.bone_indices.w == 15);

    float sum = data.bone_weights.x + data.bone_weights.y +
                data.bone_weights.z + data.bone_weights.w;
    REQUIRE_THAT(sum, WithinAbs(1.0f, 0.001f));
}
