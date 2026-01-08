#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/asset/types.hpp>

using namespace engine::asset;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("Asset base struct", "[asset][types]") {
    SECTION("Default values") {
        struct TestAsset : Asset {
            int test_value = 42;
        };

        TestAsset asset;
        REQUIRE(asset.id.is_null());
        REQUIRE(asset.path.empty());
        REQUIRE(asset.last_modified == 0);
    }

    SECTION("Can set UUID") {
        struct TestAsset : Asset {};

        TestAsset asset;
        asset.id = UUID::from_u64(0x12345678, 0xABCDEF01);
        REQUIRE_FALSE(asset.id.is_null());
    }
}

TEST_CASE("MeshAsset", "[asset][types]") {
    MeshAsset mesh;

    REQUIRE(mesh.id.is_null());
    REQUIRE(mesh.path.empty());
    REQUIRE(mesh.vertex_count == 0);
    REQUIRE(mesh.index_count == 0);

    mesh.vertex_count = 1000;
    mesh.index_count = 3000;
    mesh.path = "assets/models/player.gltf";

    REQUIRE(mesh.vertex_count == 1000);
    REQUIRE(mesh.index_count == 3000);
    REQUIRE(mesh.path == "assets/models/player.gltf");
}

TEST_CASE("TextureAsset", "[asset][types]") {
    TextureAsset texture;

    REQUIRE(texture.id.is_null());
    REQUIRE(texture.width == 0);
    REQUIRE(texture.height == 0);
    REQUIRE(texture.channels == 0);
    REQUIRE(texture.mip_levels == 1);
    REQUIRE(texture.format == TextureFormat::RGBA8);
    REQUIRE(texture.has_alpha == false);
    REQUIRE(texture.is_hdr == false);
    REQUIRE(texture.is_cubemap == false);

    texture.width = 1024;
    texture.height = 1024;
    texture.channels = 4;
    texture.mip_levels = 10;
    texture.has_alpha = true;
    texture.is_hdr = true;

    REQUIRE(texture.width == 1024);
    REQUIRE(texture.height == 1024);
    REQUIRE(texture.channels == 4);
    REQUIRE(texture.mip_levels == 10);
    REQUIRE(texture.has_alpha == true);
    REQUIRE(texture.is_hdr == true);
}

TEST_CASE("ShaderAsset", "[asset][types]") {
    ShaderAsset shader;

    REQUIRE(shader.id.is_null());
    REQUIRE(shader.path.empty());
}

TEST_CASE("MaterialAsset", "[asset][types]") {
    MaterialAsset material;

    REQUIRE(material.id.is_null());
    REQUIRE(material.textures.empty());

    material.textures.push_back({"albedo", TextureHandle{}});
    material.textures.push_back({"normal", TextureHandle{}});

    REQUIRE(material.textures.size() == 2);
    REQUIRE(material.textures[0].first == "albedo");
    REQUIRE(material.textures[1].first == "normal");
}

TEST_CASE("AudioAsset", "[asset][types]") {
    AudioAsset audio;

    REQUIRE(audio.id.is_null());
    REQUIRE(audio.data.empty());
    REQUIRE(audio.sample_rate == 0);
    REQUIRE(audio.channels == 0);
    REQUIRE(audio.sample_count == 0);
    REQUIRE(audio.is_stream == false);

    audio.sample_rate = 44100;
    audio.channels = 2;
    audio.sample_count = 88200;
    audio.is_stream = true;

    REQUIRE(audio.sample_rate == 44100);
    REQUIRE(audio.channels == 2);
    REQUIRE(audio.sample_count == 88200);
    REQUIRE(audio.is_stream == true);
}

TEST_CASE("SceneAsset", "[asset][types]") {
    SceneAsset scene;

    REQUIRE(scene.id.is_null());
    REQUIRE(scene.json_data.empty());

    scene.json_data = R"({"entities": []})";
    REQUIRE(scene.json_data == R"({"entities": []})");
}

TEST_CASE("PrefabAsset", "[asset][types]") {
    PrefabAsset prefab;

    REQUIRE(prefab.id.is_null());
    REQUIRE(prefab.json_data.empty());

    prefab.json_data = R"({"name": "Player"})";
    REQUIRE(prefab.json_data == R"({"name": "Player"})");
}

TEST_CASE("AnimationPath enum", "[asset][types]") {
    REQUIRE(static_cast<int>(AnimationPath::Translation) == 0);
    REQUIRE(static_cast<int>(AnimationPath::Rotation) == 1);
    REQUIRE(static_cast<int>(AnimationPath::Scale) == 2);
}

TEST_CASE("AnimationInterpolation enum", "[asset][types]") {
    REQUIRE(static_cast<int>(AnimationInterpolation::Step) == 0);
    REQUIRE(static_cast<int>(AnimationInterpolation::Linear) == 1);
    REQUIRE(static_cast<int>(AnimationInterpolation::CubicSpline) == 2);
}

TEST_CASE("AnimationChannel", "[asset][types]") {
    AnimationChannel channel;

    REQUIRE(channel.target_joint == -1);
    REQUIRE(channel.path == AnimationPath::Translation);
    REQUIRE(channel.interpolation == AnimationInterpolation::Linear);
    REQUIRE(channel.times.empty());
    REQUIRE(channel.values.empty());

    channel.target_joint = 5;
    channel.path = AnimationPath::Rotation;
    channel.interpolation = AnimationInterpolation::CubicSpline;
    channel.times = {0.0f, 0.5f, 1.0f};
    channel.values = {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};

    REQUIRE(channel.target_joint == 5);
    REQUIRE(channel.path == AnimationPath::Rotation);
    REQUIRE(channel.interpolation == AnimationInterpolation::CubicSpline);
    REQUIRE(channel.times.size() == 3);
    REQUIRE(channel.values.size() == 12);
}

TEST_CASE("AnimationAsset", "[asset][types]") {
    AnimationAsset anim;

    REQUIRE(anim.id.is_null());
    REQUIRE(anim.name.empty());
    REQUIRE_THAT(anim.duration, WithinAbs(0.0f, 0.001f));
    REQUIRE(anim.channels.empty());

    anim.name = "walk";
    anim.duration = 1.5f;
    anim.channels.push_back(AnimationChannel{});

    REQUIRE(anim.name == "walk");
    REQUIRE_THAT(anim.duration, WithinAbs(1.5f, 0.001f));
    REQUIRE(anim.channels.size() == 1);
}

TEST_CASE("SkeletonJoint", "[asset][types]") {
    SkeletonJoint joint;

    REQUIRE(joint.name.empty());
    REQUIRE(joint.parent_index == -1);

    joint.name = "spine";
    joint.parent_index = 0;

    REQUIRE(joint.name == "spine");
    REQUIRE(joint.parent_index == 0);
}

TEST_CASE("SkeletonAsset", "[asset][types]") {
    SkeletonAsset skeleton;

    REQUIRE(skeleton.id.is_null());
    REQUIRE(skeleton.name.empty());
    REQUIRE(skeleton.joints.empty());

    skeleton.name = "humanoid";
    skeleton.joints.push_back(SkeletonJoint{"root", -1});
    skeleton.joints.push_back(SkeletonJoint{"spine", 0});
    skeleton.joints.push_back(SkeletonJoint{"head", 1});

    REQUIRE(skeleton.name == "humanoid");
    REQUIRE(skeleton.joints.size() == 3);
    REQUIRE(skeleton.joints[0].name == "root");
    REQUIRE(skeleton.joints[0].parent_index == -1);
    REQUIRE(skeleton.joints[1].parent_index == 0);
    REQUIRE(skeleton.joints[2].parent_index == 1);
}
