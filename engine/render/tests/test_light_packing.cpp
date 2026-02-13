#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/render/pbr_material.hpp>

using namespace engine::render;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// --- packLightForGPU ---

TEST_CASE("packLightForGPU directional light", "[render][light_packing]") {
    LightData light;
    light.type = 0;
    light.position = Vec3(0.0f);
    light.direction = Vec3(0.0f, -1.0f, 0.0f);
    light.color = Vec3(1.0f, 0.9f, 0.8f);
    light.intensity = 2.0f;
    light.range = 0.0f;
    light.cast_shadows = true;

    GPULightData gpu = packLightForGPU(light);

    // Type in w component
    REQUIRE_THAT(gpu.position_type.w, WithinAbs(0.0f, 0.001f));

    // Direction
    REQUIRE_THAT(gpu.direction_range.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(gpu.direction_range.y, WithinAbs(-1.0f, 0.001f));
    REQUIRE_THAT(gpu.direction_range.z, WithinAbs(0.0f, 0.001f));

    // Range
    REQUIRE_THAT(gpu.direction_range.w, WithinAbs(0.0f, 0.001f));

    // Color and intensity
    REQUIRE_THAT(gpu.color_intensity.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(gpu.color_intensity.y, WithinAbs(0.9f, 0.001f));
    REQUIRE_THAT(gpu.color_intensity.z, WithinAbs(0.8f, 0.001f));
    REQUIRE_THAT(gpu.color_intensity.w, WithinAbs(2.0f, 0.001f));

    // Shadow index: 0.0 when cast_shadows is true
    REQUIRE_THAT(gpu.spot_params.z, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("packLightForGPU point light", "[render][light_packing]") {
    LightData light;
    light.type = 1;
    light.position = Vec3(5.0f, 3.0f, -2.0f);
    light.direction = Vec3(0.0f);
    light.color = Vec3(0.0f, 1.0f, 0.0f);
    light.intensity = 10.0f;
    light.range = 25.0f;
    light.cast_shadows = false;

    GPULightData gpu = packLightForGPU(light);

    // Type = 1.0 in w
    REQUIRE_THAT(gpu.position_type.w, WithinAbs(1.0f, 0.001f));

    // Position
    REQUIRE_THAT(gpu.position_type.x, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(gpu.position_type.y, WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(gpu.position_type.z, WithinAbs(-2.0f, 0.001f));

    // Range
    REQUIRE_THAT(gpu.direction_range.w, WithinAbs(25.0f, 0.001f));

    // Shadow index: -1.0 when no shadows
    REQUIRE_THAT(gpu.spot_params.z, WithinAbs(-1.0f, 0.001f));
}

TEST_CASE("packLightForGPU spot light", "[render][light_packing]") {
    LightData light;
    light.type = 2;
    light.position = Vec3(0.0f, 10.0f, 0.0f);
    light.direction = Vec3(0.0f, -1.0f, 0.0f);
    light.color = Vec3(1.0f, 1.0f, 1.0f);
    light.intensity = 5.0f;
    light.range = 30.0f;
    light.inner_angle = 15.0f;
    light.outer_angle = 30.0f;
    light.cast_shadows = true;

    GPULightData gpu = packLightForGPU(light);

    // Type = 2.0 in w
    REQUIRE_THAT(gpu.position_type.w, WithinAbs(2.0f, 0.001f));

    // Spot angles in spot_params
    REQUIRE_THAT(gpu.spot_params.x, WithinAbs(15.0f, 0.001f));
    REQUIRE_THAT(gpu.spot_params.y, WithinAbs(30.0f, 0.001f));

    // Shadow index: 0.0 when casting shadows
    REQUIRE_THAT(gpu.spot_params.z, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("packLightForGPU shadow index values", "[render][light_packing]") {
    LightData with_shadows;
    with_shadows.cast_shadows = true;
    GPULightData gpu1 = packLightForGPU(with_shadows);
    REQUIRE_THAT(gpu1.spot_params.z, WithinAbs(0.0f, 0.001f));

    LightData without_shadows;
    without_shadows.cast_shadows = false;
    GPULightData gpu2 = packLightForGPU(without_shadows);
    REQUIRE_THAT(gpu2.spot_params.z, WithinAbs(-1.0f, 0.001f));
}

TEST_CASE("packLightForGPU position passthrough", "[render][light_packing]") {
    LightData light;
    light.position = Vec3(100.0f, -50.0f, 0.5f);
    GPULightData gpu = packLightForGPU(light);

    REQUIRE_THAT(gpu.position_type.x, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(gpu.position_type.y, WithinAbs(-50.0f, 0.001f));
    REQUIRE_THAT(gpu.position_type.z, WithinAbs(0.5f, 0.001f));
}

TEST_CASE("packLightForGPU color passthrough", "[render][light_packing]") {
    LightData light;
    light.color = Vec3(0.5f, 0.25f, 0.75f);
    light.intensity = 3.14f;
    GPULightData gpu = packLightForGPU(light);

    REQUIRE_THAT(gpu.color_intensity.x, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(gpu.color_intensity.y, WithinAbs(0.25f, 0.001f));
    REQUIRE_THAT(gpu.color_intensity.z, WithinAbs(0.75f, 0.001f));
    REQUIRE_THAT(gpu.color_intensity.w, WithinAbs(3.14f, 0.01f));
}

// --- packMaterialForGPU ---

TEST_CASE("packMaterialForGPU albedo passthrough", "[render][light_packing]") {
    PBRMaterial mat;
    mat.albedo_color = Vec4(0.8f, 0.2f, 0.1f, 0.9f);
    GPUMaterialData gpu = packMaterialForGPU(mat);

    REQUIRE_THAT(gpu.albedo_color.x, WithinAbs(0.8f, 0.001f));
    REQUIRE_THAT(gpu.albedo_color.y, WithinAbs(0.2f, 0.001f));
    REQUIRE_THAT(gpu.albedo_color.z, WithinAbs(0.1f, 0.001f));
    REQUIRE_THAT(gpu.albedo_color.w, WithinAbs(0.9f, 0.001f));
}

TEST_CASE("packMaterialForGPU pbr params", "[render][light_packing]") {
    PBRMaterial mat;
    mat.metallic = 0.9f;
    mat.roughness = 0.1f;
    mat.ao = 0.75f;
    mat.alpha_cutoff = 0.3f;
    GPUMaterialData gpu = packMaterialForGPU(mat);

    REQUIRE_THAT(gpu.pbr_params.x, WithinAbs(0.9f, 0.001f));   // metallic
    REQUIRE_THAT(gpu.pbr_params.y, WithinAbs(0.1f, 0.001f));   // roughness
    REQUIRE_THAT(gpu.pbr_params.z, WithinAbs(0.75f, 0.001f));  // ao
    REQUIRE_THAT(gpu.pbr_params.w, WithinAbs(0.3f, 0.001f));   // alpha_cutoff
}

TEST_CASE("packMaterialForGPU emissive with intensity", "[render][light_packing]") {
    PBRMaterial mat;
    mat.emissive = Vec3(1.0f, 0.5f, 0.0f);
    mat.emissive_intensity = 5.0f;
    GPUMaterialData gpu = packMaterialForGPU(mat);

    REQUIRE_THAT(gpu.emissive_color.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(gpu.emissive_color.y, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(gpu.emissive_color.z, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(gpu.emissive_color.w, WithinAbs(5.0f, 0.001f));
}

// --- PBRMaterial defaults ---

TEST_CASE("PBRMaterial defaults", "[render][light_packing]") {
    PBRMaterial mat;

    REQUIRE_THAT(mat.albedo_color.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(mat.albedo_color.y, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(mat.albedo_color.z, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(mat.albedo_color.w, WithinAbs(1.0f, 0.001f));

    REQUIRE_THAT(mat.metallic, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(mat.roughness, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(mat.ao, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(mat.alpha_cutoff, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(mat.emissive_intensity, WithinAbs(1.0f, 0.001f));

    REQUIRE_THAT(mat.emissive.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(mat.emissive.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(mat.emissive.z, WithinAbs(0.0f, 0.001f));

    REQUIRE(mat.blend_mode == BlendMode::Opaque);
    REQUIRE(mat.double_sided == false);
    REQUIRE(mat.receive_shadows == true);
    REQUIRE(mat.cast_shadows == true);

    REQUIRE_FALSE(mat.albedo_map.valid());
    REQUIRE_FALSE(mat.normal_map.valid());
    REQUIRE_FALSE(mat.metallic_roughness.valid());
    REQUIRE_FALSE(mat.ao_map.valid());
    REQUIRE_FALSE(mat.emissive_map.valid());
}

// --- BlendMode enum ---

TEST_CASE("BlendMode enum values", "[render][light_packing]") {
    REQUIRE(static_cast<uint8_t>(BlendMode::Opaque) == 0);
    REQUIRE(static_cast<uint8_t>(BlendMode::AlphaTest) == 1);
    REQUIRE(static_cast<uint8_t>(BlendMode::AlphaBlend) == 2);
    REQUIRE(static_cast<uint8_t>(BlendMode::Additive) == 3);
    REQUIRE(static_cast<uint8_t>(BlendMode::Multiply) == 4);
}

// --- GPULightData struct layout ---

TEST_CASE("GPULightData default constructed", "[render][light_packing]") {
    GPULightData gpu{};
    REQUIRE_THAT(gpu.position_type.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(gpu.position_type.w, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(gpu.direction_range.w, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(gpu.color_intensity.w, WithinAbs(0.0f, 0.001f));
}

// --- GPUMaterialData struct layout ---

TEST_CASE("GPUMaterialData default constructed", "[render][light_packing]") {
    GPUMaterialData gpu{};
    REQUIRE_THAT(gpu.albedo_color.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(gpu.pbr_params.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(gpu.emissive_color.x, WithinAbs(0.0f, 0.001f));
}
