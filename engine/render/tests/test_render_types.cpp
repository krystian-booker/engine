#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/render/types.hpp>

using namespace engine::render;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("MeshHandle", "[render][types]") {
    SECTION("Default is invalid") {
        MeshHandle handle;
        REQUIRE_FALSE(handle.valid());
        REQUIRE(handle.id == UINT32_MAX);
    }

    SECTION("Valid handle") {
        MeshHandle handle;
        handle.id = 42;
        REQUIRE(handle.valid());
    }
}

TEST_CASE("TextureHandle", "[render][types]") {
    SECTION("Default is invalid") {
        TextureHandle handle;
        REQUIRE_FALSE(handle.valid());
        REQUIRE(handle.id == UINT32_MAX);
    }

    SECTION("Valid handle") {
        TextureHandle handle;
        handle.id = 100;
        REQUIRE(handle.valid());
    }
}

TEST_CASE("ShaderHandle", "[render][types]") {
    SECTION("Default is invalid") {
        ShaderHandle handle;
        REQUIRE_FALSE(handle.valid());
    }

    SECTION("Valid handle") {
        ShaderHandle handle;
        handle.id = 5;
        REQUIRE(handle.valid());
    }
}

TEST_CASE("MaterialHandle", "[render][types]") {
    SECTION("Default is invalid") {
        MaterialHandle handle;
        REQUIRE_FALSE(handle.valid());
    }

    SECTION("Valid handle") {
        MaterialHandle handle;
        handle.id = 10;
        REQUIRE(handle.valid());
    }
}

TEST_CASE("Vertex defaults", "[render][types]") {
    Vertex v;

    REQUIRE_THAT(v.position.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(v.position.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(v.position.z, WithinAbs(0.0f, 0.001f));

    REQUIRE_THAT(v.normal.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(v.normal.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(v.normal.z, WithinAbs(0.0f, 0.001f));

    REQUIRE_THAT(v.texcoord.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(v.texcoord.y, WithinAbs(0.0f, 0.001f));

    // Default color is white
    REQUIRE_THAT(v.color.r, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(v.color.g, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(v.color.b, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(v.color.a, WithinAbs(1.0f, 0.001f));

    // Default tangent is zero
    REQUIRE_THAT(v.tangent.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(v.tangent.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(v.tangent.z, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("Vertex custom values", "[render][types]") {
    Vertex v;
    v.position = Vec3{1.0f, 2.0f, 3.0f};
    v.normal = Vec3{0.0f, 1.0f, 0.0f};
    v.texcoord = Vec2{0.5f, 0.5f};
    v.color = Vec4{1.0f, 0.0f, 0.0f, 1.0f};
    v.tangent = Vec3{1.0f, 0.0f, 0.0f};

    REQUIRE_THAT(v.position.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(v.position.y, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(v.position.z, WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(v.normal.y, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(v.texcoord.x, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(v.color.r, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(v.tangent.x, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("SkinnedVertex defaults", "[render][types]") {
    SkinnedVertex v;

    // Bone indices default to 0
    REQUIRE(v.bone_indices.x == 0);
    REQUIRE(v.bone_indices.y == 0);
    REQUIRE(v.bone_indices.z == 0);
    REQUIRE(v.bone_indices.w == 0);

    // Bone weights default to 0
    REQUIRE_THAT(v.bone_weights.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(v.bone_weights.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(v.bone_weights.z, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(v.bone_weights.w, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("SkinnedVertex with bone data", "[render][types]") {
    SkinnedVertex v;
    v.bone_indices = IVec4{0, 1, 2, 3};
    v.bone_weights = Vec4{0.5f, 0.3f, 0.15f, 0.05f};

    REQUIRE(v.bone_indices.x == 0);
    REQUIRE(v.bone_indices.y == 1);
    REQUIRE(v.bone_indices.z == 2);
    REQUIRE(v.bone_indices.w == 3);

    REQUIRE_THAT(v.bone_weights.x, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(v.bone_weights.y, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(v.bone_weights.z, WithinAbs(0.15f, 0.001f));
    REQUIRE_THAT(v.bone_weights.w, WithinAbs(0.05f, 0.001f));

    // Weights should sum to 1.0
    float sum = v.bone_weights.x + v.bone_weights.y + v.bone_weights.z + v.bone_weights.w;
    REQUIRE_THAT(sum, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("MeshData", "[render][types]") {
    MeshData data;

    REQUIRE(data.vertices.empty());
    REQUIRE(data.indices.empty());

    data.vertices.push_back(Vertex{});
    data.vertices.push_back(Vertex{});
    data.vertices.push_back(Vertex{});
    data.indices = {0, 1, 2};

    REQUIRE(data.vertices.size() == 3);
    REQUIRE(data.indices.size() == 3);
}

TEST_CASE("SkinnedMeshData", "[render][types]") {
    SkinnedMeshData data;

    REQUIRE(data.vertices.empty());
    REQUIRE(data.indices.empty());
    REQUIRE(data.bone_count == 0);

    data.bone_count = 50;
    REQUIRE(data.bone_count == 50);
}

TEST_CASE("TextureFormat enum", "[render][types]") {
    REQUIRE(static_cast<uint8_t>(TextureFormat::RGBA8) == 0);
    REQUIRE(static_cast<uint8_t>(TextureFormat::RGBA16F) == 1);
    REQUIRE(static_cast<uint8_t>(TextureFormat::RGBA32F) == 2);
    REQUIRE(static_cast<uint8_t>(TextureFormat::R8) == 3);
    REQUIRE(static_cast<uint8_t>(TextureFormat::RG8) == 4);
    REQUIRE(static_cast<uint8_t>(TextureFormat::Depth24) == 5);
    REQUIRE(static_cast<uint8_t>(TextureFormat::Depth32F) == 6);
    REQUIRE(static_cast<uint8_t>(TextureFormat::BC1) == 7);
    REQUIRE(static_cast<uint8_t>(TextureFormat::BC3) == 8);
    REQUIRE(static_cast<uint8_t>(TextureFormat::BC7) == 9);
}

TEST_CASE("TextureData defaults", "[render][types]") {
    TextureData data;

    REQUIRE(data.width == 0);
    REQUIRE(data.height == 0);
    REQUIRE(data.depth == 1);
    REQUIRE(data.mip_levels == 1);
    REQUIRE(data.format == TextureFormat::RGBA8);
    REQUIRE(data.pixels.empty());
    REQUIRE(data.is_cubemap == false);
}

TEST_CASE("TextureData custom values", "[render][types]") {
    TextureData data;
    data.width = 512;
    data.height = 512;
    data.mip_levels = 9;
    data.format = TextureFormat::RGBA16F;
    data.is_cubemap = true;

    REQUIRE(data.width == 512);
    REQUIRE(data.height == 512);
    REQUIRE(data.mip_levels == 9);
    REQUIRE(data.format == TextureFormat::RGBA16F);
    REQUIRE(data.is_cubemap == true);
}

TEST_CASE("ShaderType enum", "[render][types]") {
    REQUIRE(static_cast<uint8_t>(ShaderType::Vertex) == 0);
    REQUIRE(static_cast<uint8_t>(ShaderType::Fragment) == 1);
    REQUIRE(static_cast<uint8_t>(ShaderType::Compute) == 2);
}

TEST_CASE("ShaderData", "[render][types]") {
    ShaderData data;

    REQUIRE(data.vertex_binary.empty());
    REQUIRE(data.fragment_binary.empty());

    data.vertex_binary = {0x01, 0x02, 0x03};
    data.fragment_binary = {0x04, 0x05, 0x06};

    REQUIRE(data.vertex_binary.size() == 3);
    REQUIRE(data.fragment_binary.size() == 3);
}

TEST_CASE("MaterialPropertyType enum", "[render][types]") {
    REQUIRE(static_cast<uint8_t>(MaterialPropertyType::Float) == 0);
    REQUIRE(static_cast<uint8_t>(MaterialPropertyType::Vec2) == 1);
    REQUIRE(static_cast<uint8_t>(MaterialPropertyType::Vec3) == 2);
    REQUIRE(static_cast<uint8_t>(MaterialPropertyType::Vec4) == 3);
    REQUIRE(static_cast<uint8_t>(MaterialPropertyType::Mat4) == 4);
    REQUIRE(static_cast<uint8_t>(MaterialPropertyType::Texture) == 5);
}

TEST_CASE("MaterialProperty defaults", "[render][types]") {
    MaterialProperty prop;

    REQUIRE(prop.type == MaterialPropertyType::Float);
    REQUIRE_THAT(prop.value.f, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("MaterialData defaults", "[render][types]") {
    MaterialData data;

    REQUIRE_FALSE(data.shader.valid());
    REQUIRE(data.properties.empty());

    REQUIRE_THAT(data.albedo.r, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(data.albedo.g, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(data.albedo.b, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(data.albedo.a, WithinAbs(1.0f, 0.001f));

    REQUIRE_THAT(data.emissive.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(data.metallic, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(data.roughness, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(data.ao, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(data.alpha_cutoff, WithinAbs(0.5f, 0.001f));

    REQUIRE(data.double_sided == false);
    REQUIRE(data.transparent == false);
}

TEST_CASE("MaterialData PBR values", "[render][types]") {
    MaterialData data;
    data.albedo = Vec4{0.8f, 0.2f, 0.1f, 1.0f};
    data.metallic = 0.9f;
    data.roughness = 0.1f;
    data.emissive = Vec3{0.5f, 0.0f, 0.0f};

    REQUIRE_THAT(data.albedo.r, WithinAbs(0.8f, 0.001f));
    REQUIRE_THAT(data.metallic, WithinAbs(0.9f, 0.001f));
    REQUIRE_THAT(data.roughness, WithinAbs(0.1f, 0.001f));
    REQUIRE_THAT(data.emissive.x, WithinAbs(0.5f, 0.001f));
}

TEST_CASE("DrawCall defaults", "[render][types]") {
    DrawCall call;

    REQUIRE_FALSE(call.mesh.valid());
    REQUIRE_FALSE(call.material.valid());
    REQUIRE(call.render_layer == 0);
    REQUIRE(call.cast_shadows == true);
}

TEST_CASE("LightData defaults", "[render][types]") {
    LightData light;

    REQUIRE_THAT(light.position.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(light.direction.y, WithinAbs(-1.0f, 0.001f));
    REQUIRE_THAT(light.color.r, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(light.intensity, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(light.range, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(light.inner_angle, WithinAbs(30.0f, 0.001f));
    REQUIRE_THAT(light.outer_angle, WithinAbs(45.0f, 0.001f));
    REQUIRE(light.type == 0);  // directional
    REQUIRE(light.cast_shadows == false);
    REQUIRE(light.shadow_map_index == -1);
}

TEST_CASE("LightData types", "[render][types]") {
    SECTION("Directional") {
        LightData light;
        light.type = 0;
        REQUIRE(light.type == 0);
    }

    SECTION("Point") {
        LightData light;
        light.type = 1;
        light.range = 20.0f;
        REQUIRE(light.type == 1);
        REQUIRE_THAT(light.range, WithinAbs(20.0f, 0.001f));
    }

    SECTION("Spot") {
        LightData light;
        light.type = 2;
        light.inner_angle = 20.0f;
        light.outer_angle = 40.0f;
        REQUIRE(light.type == 2);
        REQUIRE_THAT(light.inner_angle, WithinAbs(20.0f, 0.001f));
        REQUIRE_THAT(light.outer_angle, WithinAbs(40.0f, 0.001f));
    }
}

TEST_CASE("PrimitiveMesh enum", "[render][types]") {
    REQUIRE(static_cast<uint8_t>(PrimitiveMesh::Cube) == 0);
    REQUIRE(static_cast<uint8_t>(PrimitiveMesh::Sphere) == 1);
    REQUIRE(static_cast<uint8_t>(PrimitiveMesh::Plane) == 2);
    REQUIRE(static_cast<uint8_t>(PrimitiveMesh::Cylinder) == 3);
    REQUIRE(static_cast<uint8_t>(PrimitiveMesh::Cone) == 4);
    REQUIRE(static_cast<uint8_t>(PrimitiveMesh::Quad) == 5);
}
