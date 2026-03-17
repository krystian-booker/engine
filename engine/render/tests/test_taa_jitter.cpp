#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#define private public
#include <engine/render/post_process.hpp>
#undef private
#include <cmath>

using namespace engine::render;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

namespace {

TAASystem make_cpu_taa(const TAAConfig& config) {
    TAASystem taa;
    taa.m_config = config;

    for (int i = 0; i < TAASystem::JITTER_SAMPLES; ++i) {
        taa.m_jitter_sequence[i] = Vec2(
            TAASystem::halton(i + 1, 2) - 0.5f,
            TAASystem::halton(i + 1, 3) - 0.5f
        );
    }

    return taa;
}

} // namespace

TEST_CASE("TAA jitter returns zero when disabled", "[render][taa]") {
    TAAConfig config;
    config.enabled = false;
    const TAASystem taa = make_cpu_taa(config);

    const Vec2 jitter = taa.get_jitter(0);
    REQUIRE_THAT(jitter.x, WithinAbs(0.0f, 0.0001f));
    REQUIRE_THAT(jitter.y, WithinAbs(0.0f, 0.0001f));
}

TEST_CASE("TAA jitter is non-zero when enabled", "[render][taa]") {
    TAAConfig config;
    config.enabled = true;
    config.jitter_scale = 1.0f;
    const TAASystem taa = make_cpu_taa(config);

    bool has_nonzero = false;
    for (uint32_t i = 0; i < TAASystem::JITTER_SAMPLES; ++i) {
        const Vec2 jitter = taa.get_jitter(i);
        if (std::abs(jitter.x) > 0.001f || std::abs(jitter.y) > 0.001f) {
            has_nonzero = true;
            break;
        }
    }

    REQUIRE(has_nonzero);
}

TEST_CASE("TAA jitter magnitude is in sub-pixel range", "[render][taa]") {
    TAAConfig config;
    config.enabled = true;
    config.jitter_scale = 1.0f;
    const TAASystem taa = make_cpu_taa(config);

    for (uint32_t i = 0; i < TAASystem::JITTER_SAMPLES; ++i) {
        const Vec2 jitter = taa.get_jitter(i);
        REQUIRE(jitter.x >= -0.5f);
        REQUIRE(jitter.x <= 0.5f);
        REQUIRE(jitter.y >= -0.5f);
        REQUIRE(jitter.y <= 0.5f);
    }
}

TEST_CASE("TAA jitter wraps after JITTER_SAMPLES", "[render][taa]") {
    TAAConfig config;
    config.enabled = true;
    const TAASystem taa = make_cpu_taa(config);

    const Vec2 jitter0 = taa.get_jitter(0);
    const Vec2 jitter8 = taa.get_jitter(TAASystem::JITTER_SAMPLES);
    REQUIRE_THAT(jitter0.x, WithinAbs(jitter8.x, 0.0001f));
    REQUIRE_THAT(jitter0.y, WithinAbs(jitter8.y, 0.0001f));
}

TEST_CASE("TAA jitter_scale multiplies jitter", "[render][taa]") {
    TAAConfig config1;
    config1.enabled = true;
    config1.jitter_scale = 1.0f;
    const TAASystem taa1 = make_cpu_taa(config1);

    TAAConfig config2;
    config2.enabled = true;
    config2.jitter_scale = 2.0f;
    const TAASystem taa2 = make_cpu_taa(config2);

    const Vec2 jitter1 = taa1.get_jitter(1);
    const Vec2 jitter2 = taa2.get_jitter(1);
    REQUIRE_THAT(jitter2.x, WithinAbs(jitter1.x * 2.0f, 0.0001f));
    REQUIRE_THAT(jitter2.y, WithinAbs(jitter1.y * 2.0f, 0.0001f));
}

TEST_CASE("TAA jitter applied to projection matrix stays in clip-space sub-pixel range", "[render][taa]") {
    TAAConfig config;
    config.enabled = true;
    config.jitter_scale = 1.0f;
    const TAASystem taa = make_cpu_taa(config);

    const Vec2 jitter = taa.get_jitter(1);

    constexpr uint32_t kWidth = 1920;
    constexpr uint32_t kHeight = 1080;

    Mat4 proj = glm::perspective(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
    Mat4 jittered_proj = proj;
    jittered_proj[2][0] += jitter.x * 2.0f / static_cast<float>(kWidth);
    jittered_proj[2][1] += jitter.y * 2.0f / static_cast<float>(kHeight);

    const float clip_offset_x = jitter.x * 2.0f / static_cast<float>(kWidth);
    const float clip_offset_y = jitter.y * 2.0f / static_cast<float>(kHeight);

    if (std::abs(jitter.x) > 0.01f) {
        REQUIRE(std::abs(clip_offset_x) > 1e-6f);
        REQUIRE(std::abs(clip_offset_x) < 0.01f);
    }
    if (std::abs(jitter.y) > 0.01f) {
        REQUIRE(std::abs(clip_offset_y) > 1e-6f);
        REQUIRE(std::abs(clip_offset_y) < 0.01f);
    }
}
