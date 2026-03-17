#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/render/light_probes.hpp>

using namespace engine::render;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

namespace {

void set_probe_ambient(LightProbe& probe, const Vec3& color) {
    probe.sh_coefficients = LightProbeUtils::create_ambient_sh(color);
    probe.valid = true;
    probe.needs_update = false;
}

} // namespace

TEST_CASE("LightProbeVolume sampling falls back to zero without valid probes", "[render][light_probes]") {
    LightProbeVolume volume;
    volume.min_bounds = Vec3(0.0f);
    volume.max_bounds = Vec3(2.0f);
    volume.resolution_x = 2;
    volume.resolution_y = 2;
    volume.resolution_z = 2;
    volume.initialize();

    const Vec3 inside = volume.sample_irradiance(Vec3(1.0f), Vec3(0.0f, 1.0f, 0.0f));
    const Vec3 outside = volume.sample_irradiance(Vec3(3.0f), Vec3(0.0f, 1.0f, 0.0f));

    REQUIRE_THAT(inside.x, WithinAbs(0.0f, 0.0001f));
    REQUIRE_THAT(inside.y, WithinAbs(0.0f, 0.0001f));
    REQUIRE_THAT(inside.z, WithinAbs(0.0f, 0.0001f));
    REQUIRE_THAT(outside.x, WithinAbs(0.0f, 0.0001f));
    REQUIRE_THAT(outside.y, WithinAbs(0.0f, 0.0001f));
    REQUIRE_THAT(outside.z, WithinAbs(0.0f, 0.0001f));
}

TEST_CASE("LightProbeVolume sampling changes when valid probe data exists", "[render][light_probes]") {
    LightProbeVolume volume;
    volume.min_bounds = Vec3(0.0f);
    volume.max_bounds = Vec3(2.0f);
    volume.resolution_x = 2;
    volume.resolution_y = 2;
    volume.resolution_z = 2;
    volume.initialize();

    set_probe_ambient(*volume.get_probe(0, 0, 0), Vec3(1.0f, 0.0f, 0.0f));
    set_probe_ambient(*volume.get_probe(1, 0, 0), Vec3(0.0f, 1.0f, 0.0f));
    set_probe_ambient(*volume.get_probe(0, 1, 0), Vec3(0.0f, 0.0f, 1.0f));
    set_probe_ambient(*volume.get_probe(1, 1, 0), Vec3(1.0f, 1.0f, 0.0f));
    set_probe_ambient(*volume.get_probe(0, 0, 1), Vec3(1.0f, 0.0f, 1.0f));
    set_probe_ambient(*volume.get_probe(1, 0, 1), Vec3(0.0f, 1.0f, 1.0f));
    set_probe_ambient(*volume.get_probe(0, 1, 1), Vec3(0.5f, 0.5f, 0.5f));
    set_probe_ambient(*volume.get_probe(1, 1, 1), Vec3(0.25f, 0.5f, 0.75f));

    const Vec3 sampled = volume.sample_irradiance(Vec3(0.5f), Vec3(0.0f, 1.0f, 0.0f));
    const Vec3 expected =
        (Vec3(1.0f, 0.0f, 0.0f) +
         Vec3(0.0f, 1.0f, 0.0f) +
         Vec3(0.0f, 0.0f, 1.0f) +
         Vec3(1.0f, 1.0f, 0.0f) +
         Vec3(1.0f, 0.0f, 1.0f) +
         Vec3(0.0f, 1.0f, 1.0f) +
         Vec3(0.5f, 0.5f, 0.5f) +
         Vec3(0.25f, 0.5f, 0.75f)) / 8.0f;

    REQUIRE(sampled.x > 0.0f);
    REQUIRE(sampled.y > 0.0f);
    REQUIRE(sampled.z > 0.0f);
    REQUIRE_THAT(sampled.x, WithinAbs(expected.x, 0.05f));
    REQUIRE_THAT(sampled.y, WithinAbs(expected.y, 0.05f));
    REQUIRE_THAT(sampled.z, WithinAbs(expected.z, 0.05f));
}
