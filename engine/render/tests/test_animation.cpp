#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/render/animation.hpp>

using namespace engine::render;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("AnimationInterpolation enum", "[render][animation]") {
    REQUIRE(static_cast<int>(AnimationInterpolation::Step) == 0);
    REQUIRE(static_cast<int>(AnimationInterpolation::Linear) == 1);
    REQUIRE(static_cast<int>(AnimationInterpolation::CubicSpline) == 2);
}

TEST_CASE("AnimationChannel::TargetType enum", "[render][animation]") {
    REQUIRE(static_cast<int>(AnimationChannel::TargetType::Translation) == 0);
    REQUIRE(static_cast<int>(AnimationChannel::TargetType::Rotation) == 1);
    REQUIRE(static_cast<int>(AnimationChannel::TargetType::Scale) == 2);
}

TEST_CASE("AnimationChannel defaults", "[render][animation]") {
    AnimationChannel channel;

    REQUIRE(channel.get_bone_index() == -1);
    REQUIRE(channel.get_target_type() == AnimationChannel::TargetType::Translation);
    REQUIRE(channel.get_interpolation() == AnimationInterpolation::Linear);
    REQUIRE(channel.get_position_keyframe_count() == 0);
    REQUIRE(channel.get_rotation_keyframe_count() == 0);
    REQUIRE(channel.get_scale_keyframe_count() == 0);
}

TEST_CASE("AnimationChannel set_target", "[render][animation]") {
    AnimationChannel channel;

    channel.set_target(5, AnimationChannel::TargetType::Rotation);

    REQUIRE(channel.get_bone_index() == 5);
    REQUIRE(channel.get_target_type() == AnimationChannel::TargetType::Rotation);
}

TEST_CASE("AnimationChannel set_interpolation", "[render][animation]") {
    AnimationChannel channel;

    channel.set_interpolation(AnimationInterpolation::CubicSpline);

    REQUIRE(channel.get_interpolation() == AnimationInterpolation::CubicSpline);
}

TEST_CASE("AnimationChannel add_position_keyframe", "[render][animation]") {
    AnimationChannel channel;

    channel.add_position_keyframe(0.0f, Vec3{0.0f, 0.0f, 0.0f});
    channel.add_position_keyframe(1.0f, Vec3{10.0f, 0.0f, 0.0f});

    REQUIRE(channel.get_position_keyframe_count() == 2);
}

TEST_CASE("AnimationChannel add_rotation_keyframe", "[render][animation]") {
    AnimationChannel channel;

    channel.add_rotation_keyframe(0.0f, Quat{1.0f, 0.0f, 0.0f, 0.0f});
    channel.add_rotation_keyframe(1.0f, Quat{0.707f, 0.707f, 0.0f, 0.0f});

    REQUIRE(channel.get_rotation_keyframe_count() == 2);
}

TEST_CASE("AnimationChannel add_scale_keyframe", "[render][animation]") {
    AnimationChannel channel;

    channel.add_scale_keyframe(0.0f, Vec3{1.0f, 1.0f, 1.0f});
    channel.add_scale_keyframe(1.0f, Vec3{2.0f, 2.0f, 2.0f});

    REQUIRE(channel.get_scale_keyframe_count() == 2);
}

TEST_CASE("AnimationChannel sample_position", "[render][animation]") {
    AnimationChannel channel;
    channel.set_interpolation(AnimationInterpolation::Linear);

    channel.add_position_keyframe(0.0f, Vec3{0.0f, 0.0f, 0.0f});
    channel.add_position_keyframe(1.0f, Vec3{10.0f, 0.0f, 0.0f});

    SECTION("At start") {
        Vec3 pos = channel.sample_position(0.0f);
        REQUIRE_THAT(pos.x, WithinAbs(0.0f, 0.01f));
    }

    SECTION("At middle") {
        Vec3 pos = channel.sample_position(0.5f);
        REQUIRE_THAT(pos.x, WithinAbs(5.0f, 0.01f));
    }

    SECTION("At end") {
        Vec3 pos = channel.sample_position(1.0f);
        REQUIRE_THAT(pos.x, WithinAbs(10.0f, 0.01f));
    }
}

TEST_CASE("AnimationChannel sample_scale", "[render][animation]") {
    AnimationChannel channel;
    channel.set_interpolation(AnimationInterpolation::Linear);

    channel.add_scale_keyframe(0.0f, Vec3{1.0f, 1.0f, 1.0f});
    channel.add_scale_keyframe(1.0f, Vec3{2.0f, 2.0f, 2.0f});

    Vec3 scale = channel.sample_scale(0.5f);
    REQUIRE_THAT(scale.x, WithinAbs(1.5f, 0.01f));
    REQUIRE_THAT(scale.y, WithinAbs(1.5f, 0.01f));
    REQUIRE_THAT(scale.z, WithinAbs(1.5f, 0.01f));
}

TEST_CASE("AnimationChannel get_duration", "[render][animation]") {
    AnimationChannel channel;

    channel.add_position_keyframe(0.0f, Vec3{0.0f});
    channel.add_position_keyframe(2.5f, Vec3{10.0f});

    REQUIRE_THAT(channel.get_duration(), WithinAbs(2.5f, 0.001f));
}

TEST_CASE("AnimationClip default", "[render][animation]") {
    AnimationClip clip;

    REQUIRE(clip.get_name().empty());
    REQUIRE_THAT(clip.get_duration(), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(clip.get_ticks_per_second(), WithinAbs(25.0f, 0.001f));
    REQUIRE(clip.get_channels().empty());
}

TEST_CASE("AnimationClip with name", "[render][animation]") {
    AnimationClip clip("walk");

    REQUIRE(clip.get_name() == "walk");
}

TEST_CASE("AnimationClip set_name", "[render][animation]") {
    AnimationClip clip;
    clip.set_name("run");

    REQUIRE(clip.get_name() == "run");
}

TEST_CASE("AnimationClip set_duration", "[render][animation]") {
    AnimationClip clip;
    clip.set_duration(2.5f);

    REQUIRE_THAT(clip.get_duration(), WithinAbs(2.5f, 0.001f));
}

TEST_CASE("AnimationClip set_ticks_per_second", "[render][animation]") {
    AnimationClip clip;
    clip.set_ticks_per_second(30.0f);

    REQUIRE_THAT(clip.get_ticks_per_second(), WithinAbs(30.0f, 0.001f));
}

TEST_CASE("AnimationClip add_channel", "[render][animation]") {
    AnimationClip clip;

    auto& channel1 = clip.add_channel();
    channel1.set_target(0, AnimationChannel::TargetType::Translation);

    auto& channel2 = clip.add_channel();
    channel2.set_target(0, AnimationChannel::TargetType::Rotation);

    REQUIRE(clip.get_channels().size() == 2);
}

TEST_CASE("AnimationBlendMode enum", "[render][animation]") {
    REQUIRE(static_cast<int>(AnimationBlendMode::Override) == 0);
    REQUIRE(static_cast<int>(AnimationBlendMode::Additive) == 1);
    REQUIRE(static_cast<int>(AnimationBlendMode::Blend) == 2);
}

TEST_CASE("AnimationState defaults", "[render][animation]") {
    AnimationState state;

    REQUIRE(state.clip == nullptr);
    REQUIRE_THAT(state.time, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(state.speed, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(state.weight, WithinAbs(1.0f, 0.001f));
    REQUIRE(state.looping == true);
    REQUIRE(state.playing == false);
    REQUIRE(state.blend_mode == AnimationBlendMode::Override);
}

TEST_CASE("AnimationState custom values", "[render][animation]") {
    AnimationState state;
    state.time = 0.5f;
    state.speed = 2.0f;
    state.weight = 0.5f;
    state.looping = false;
    state.playing = true;
    state.blend_mode = AnimationBlendMode::Additive;

    REQUIRE_THAT(state.time, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(state.speed, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(state.weight, WithinAbs(0.5f, 0.001f));
    REQUIRE(state.looping == false);
    REQUIRE(state.playing == true);
    REQUIRE(state.blend_mode == AnimationBlendMode::Additive);
}

TEST_CASE("AnimationEvent", "[render][animation]") {
    AnimationEvent event;
    event.time = 0.5f;
    event.name = "footstep";

    REQUIRE_THAT(event.time, WithinAbs(0.5f, 0.001f));
    REQUIRE(event.name == "footstep");
}

TEST_CASE("Animator default", "[render][animation]") {
    Animator animator;

    REQUIRE(animator.get_skeleton() == nullptr);
    REQUIRE_FALSE(animator.is_playing());
    REQUIRE_FALSE(animator.is_paused());
    REQUIRE_THAT(animator.get_speed(), WithinAbs(1.0f, 0.001f));
    REQUIRE(animator.is_looping() == true);
}

TEST_CASE("Animator set_skeleton", "[render][animation]") {
    Skeleton skeleton;
    skeleton.add_bone("root", -1);

    SkeletonInstance instance(&skeleton);
    Animator animator;
    animator.set_skeleton(&instance);

    REQUIRE(animator.get_skeleton() == &instance);
}

TEST_CASE("Animator speed control", "[render][animation]") {
    Animator animator;

    animator.set_speed(2.0f);
    REQUIRE_THAT(animator.get_speed(), WithinAbs(2.0f, 0.001f));

    animator.set_speed(0.5f);
    REQUIRE_THAT(animator.get_speed(), WithinAbs(0.5f, 0.001f));
}

TEST_CASE("Animator looping control", "[render][animation]") {
    Animator animator;

    REQUIRE(animator.is_looping() == true);

    animator.set_looping(false);
    REQUIRE(animator.is_looping() == false);

    animator.set_looping(true);
    REQUIRE(animator.is_looping() == true);
}

TEST_CASE("Animator pause/resume", "[render][animation]") {
    Animator animator;

    REQUIRE_FALSE(animator.is_paused());

    animator.pause();
    REQUIRE(animator.is_paused());

    animator.resume();
    REQUIRE_FALSE(animator.is_paused());
}

TEST_CASE("Animator add_clip and get_clip", "[render][animation]") {
    Animator animator;
    auto clip = std::make_shared<AnimationClip>("walk");

    animator.add_clip("walk", clip);

    auto retrieved = animator.get_clip("walk");
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->get_name() == "walk");

    auto not_found = animator.get_clip("nonexistent");
    REQUIRE(not_found == nullptr);
}

TEST_CASE("Animator remove_clip", "[render][animation]") {
    Animator animator;
    auto clip = std::make_shared<AnimationClip>("walk");

    animator.add_clip("walk", clip);
    REQUIRE(animator.get_clip("walk") != nullptr);

    animator.remove_clip("walk");
    REQUIRE(animator.get_clip("walk") == nullptr);
}

TEST_CASE("Animator stop", "[render][animation]") {
    Skeleton skeleton;
    skeleton.add_bone("root", -1);

    SkeletonInstance instance(&skeleton);
    Animator animator(&instance);

    auto clip = std::make_shared<AnimationClip>("walk");
    clip->set_duration(1.0f);
    animator.add_clip("walk", clip);

    animator.play("walk");
    REQUIRE(animator.is_playing());

    animator.stop();
    REQUIRE_FALSE(animator.is_playing());
}
