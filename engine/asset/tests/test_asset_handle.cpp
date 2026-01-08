#include <catch2/catch_test_macros.hpp>
#include <engine/core/asset_handle.hpp>
#include <engine/core/uuid.hpp>
#include <unordered_set>

using namespace engine::core;

TEST_CASE("UUID default construction", "[asset][uuid]") {
    UUID uuid;

    REQUIRE(uuid.is_null());
    REQUIRE_FALSE(static_cast<bool>(uuid));
}

TEST_CASE("UUID::null()", "[asset][uuid]") {
    auto null_uuid = UUID::null();

    REQUIRE(null_uuid.is_null());
    REQUIRE(null_uuid.high() == 0);
    REQUIRE(null_uuid.low() == 0);
}

TEST_CASE("UUID::from_u64()", "[asset][uuid]") {
    auto uuid = UUID::from_u64(0x0123456789ABCDEF, 0xFEDCBA9876543210);

    REQUIRE_FALSE(uuid.is_null());
    REQUIRE(uuid.high() == 0x0123456789ABCDEF);
    REQUIRE(uuid.low() == 0xFEDCBA9876543210);
}

TEST_CASE("UUID::from_bytes()", "[asset][uuid]") {
    uint8_t bytes[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                         0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};
    auto uuid = UUID::from_bytes(bytes);

    REQUIRE_FALSE(uuid.is_null());
    for (size_t i = 0; i < 16; ++i) {
        REQUIRE(uuid[i] == bytes[i]);
    }
}

TEST_CASE("UUID comparison operators", "[asset][uuid]") {
    auto uuid1 = UUID::from_u64(0x1234, 0x5678);
    auto uuid2 = UUID::from_u64(0x1234, 0x5678);
    auto uuid3 = UUID::from_u64(0x1234, 0x5679);
    auto uuid4 = UUID::from_u64(0x1235, 0x5678);

    SECTION("Equality") {
        REQUIRE(uuid1 == uuid2);
        REQUIRE_FALSE(uuid1 == uuid3);
        REQUIRE_FALSE(uuid1 == uuid4);
    }

    SECTION("Inequality") {
        REQUIRE_FALSE(uuid1 != uuid2);
        REQUIRE(uuid1 != uuid3);
        REQUIRE(uuid1 != uuid4);
    }

    SECTION("Less than") {
        REQUIRE(uuid1 < uuid3);
        REQUIRE(uuid1 < uuid4);
        REQUIRE_FALSE(uuid3 < uuid1);
    }

    SECTION("Greater than") {
        REQUIRE(uuid3 > uuid1);
        REQUIRE(uuid4 > uuid1);
        REQUIRE_FALSE(uuid1 > uuid3);
    }
}

TEST_CASE("UUID to_string and from_string", "[asset][uuid]") {
    auto uuid = UUID::from_u64(0x550e8400e29b41d4, 0xa716446655440000);
    std::string str = uuid.to_string();

    REQUIRE(str.length() == UUID::STRING_SIZE);

    auto parsed = UUID::from_string(str);
    REQUIRE(parsed.has_value());
    REQUIRE(*parsed == uuid);
}

TEST_CASE("UUID::from_string invalid input", "[asset][uuid]") {
    REQUIRE_FALSE(UUID::from_string("").has_value());
    REQUIRE_FALSE(UUID::from_string("not-a-uuid").has_value());
    REQUIRE_FALSE(UUID::from_string("12345678-1234-1234-1234").has_value());
}

TEST_CASE("UUID hash", "[asset][uuid]") {
    auto uuid1 = UUID::from_u64(0x1234, 0x5678);
    auto uuid2 = UUID::from_u64(0x1234, 0x5678);
    auto uuid3 = UUID::from_u64(0x1234, 0x5679);

    REQUIRE(uuid1.hash() == uuid2.hash());
    // Different UUIDs should (usually) have different hashes
    REQUIRE(uuid1.hash() != uuid3.hash());
}

TEST_CASE("UUID in unordered_set", "[asset][uuid]") {
    std::unordered_set<UUID> set;

    auto uuid1 = UUID::from_u64(0x1111, 0x2222);
    auto uuid2 = UUID::from_u64(0x3333, 0x4444);
    auto uuid3 = UUID::from_u64(0x1111, 0x2222);

    set.insert(uuid1);
    set.insert(uuid2);
    set.insert(uuid3);

    REQUIRE(set.size() == 2);
    REQUIRE(set.count(uuid1) == 1);
    REQUIRE(set.count(uuid2) == 1);
}

TEST_CASE("AssetType enum values", "[asset][handle]") {
    REQUIRE(static_cast<uint8_t>(AssetType::Unknown) == 0);
    REQUIRE(static_cast<uint8_t>(AssetType::Mesh) == 1);
    REQUIRE(static_cast<uint8_t>(AssetType::Texture) == 2);
    REQUIRE(static_cast<uint8_t>(AssetType::Shader) == 3);
    REQUIRE(static_cast<uint8_t>(AssetType::Material) == 4);
    REQUIRE(static_cast<uint8_t>(AssetType::Audio) == 5);
    REQUIRE(static_cast<uint8_t>(AssetType::Animation) == 6);
    REQUIRE(static_cast<uint8_t>(AssetType::Skeleton) == 7);
    REQUIRE(static_cast<uint8_t>(AssetType::Scene) == 8);
    REQUIRE(static_cast<uint8_t>(AssetType::Prefab) == 9);
    REQUIRE(static_cast<uint8_t>(AssetType::Font) == 10);
    REQUIRE(static_cast<uint8_t>(AssetType::Script) == 11);
}

TEST_CASE("asset_type_name", "[asset][handle]") {
    REQUIRE(std::string(asset_type_name(AssetType::Unknown)) == "Unknown");
    REQUIRE(std::string(asset_type_name(AssetType::Mesh)) == "Mesh");
    REQUIRE(std::string(asset_type_name(AssetType::Texture)) == "Texture");
    REQUIRE(std::string(asset_type_name(AssetType::Shader)) == "Shader");
    REQUIRE(std::string(asset_type_name(AssetType::Material)) == "Material");
    REQUIRE(std::string(asset_type_name(AssetType::Audio)) == "Audio");
    REQUIRE(std::string(asset_type_name(AssetType::Animation)) == "Animation");
    REQUIRE(std::string(asset_type_name(AssetType::Skeleton)) == "Skeleton");
    REQUIRE(std::string(asset_type_name(AssetType::Scene)) == "Scene");
    REQUIRE(std::string(asset_type_name(AssetType::Prefab)) == "Prefab");
    REQUIRE(std::string(asset_type_name(AssetType::Font)) == "Font");
    REQUIRE(std::string(asset_type_name(AssetType::Script)) == "Script");
}

TEST_CASE("AssetHandle default construction", "[asset][handle]") {
    MeshAssetHandle handle;

    REQUIRE_FALSE(handle.valid());
    REQUIRE_FALSE(static_cast<bool>(handle));
    REQUIRE(handle.uuid().is_null());
}

TEST_CASE("AssetHandle from UUID", "[asset][handle]") {
    auto uuid = UUID::from_u64(0x1234, 0x5678);
    MeshAssetHandle handle(uuid);

    REQUIRE(handle.valid());
    REQUIRE(static_cast<bool>(handle));
    REQUIRE(handle.uuid() == uuid);
}

TEST_CASE("AssetHandle::from_u64", "[asset][handle]") {
    auto handle = TextureAssetHandle::from_u64(0xABCD, 0xEF01);

    REQUIRE(handle.valid());
    REQUIRE(handle.uuid().high() == 0xABCD);
    REQUIRE(handle.uuid().low() == 0xEF01);
}

TEST_CASE("AssetHandle type information", "[asset][handle]") {
    REQUIRE(MeshAssetHandle::type() == AssetType::Mesh);
    REQUIRE(TextureAssetHandle::type() == AssetType::Texture);
    REQUIRE(ShaderAssetHandle::type() == AssetType::Shader);
    REQUIRE(MaterialAssetHandle::type() == AssetType::Material);
    REQUIRE(AudioAssetHandle::type() == AssetType::Audio);
    REQUIRE(AnimationAssetHandle::type() == AssetType::Animation);
    REQUIRE(SkeletonAssetHandle::type() == AssetType::Skeleton);
    REQUIRE(SceneAssetHandle::type() == AssetType::Scene);
    REQUIRE(PrefabAssetHandle::type() == AssetType::Prefab);

    REQUIRE(std::string(MeshAssetHandle::type_name()) == "Mesh");
    REQUIRE(std::string(TextureAssetHandle::type_name()) == "Texture");
}

TEST_CASE("AssetHandle comparison", "[asset][handle]") {
    auto uuid1 = UUID::from_u64(0x1234, 0x5678);
    auto uuid2 = UUID::from_u64(0x1234, 0x5678);
    auto uuid3 = UUID::from_u64(0x1234, 0x5679);

    MeshAssetHandle h1(uuid1);
    MeshAssetHandle h2(uuid2);
    MeshAssetHandle h3(uuid3);

    SECTION("Equality") {
        REQUIRE(h1 == h2);
        REQUIRE_FALSE(h1 == h3);
    }

    SECTION("Inequality") {
        REQUIRE_FALSE(h1 != h2);
        REQUIRE(h1 != h3);
    }

    SECTION("Less than") {
        REQUIRE(h1 < h3);
        REQUIRE_FALSE(h3 < h1);
    }

    SECTION("Greater than") {
        REQUIRE(h3 > h1);
        REQUIRE_FALSE(h1 > h3);
    }
}

TEST_CASE("AssetHandle in unordered_set", "[asset][handle]") {
    std::unordered_set<MeshAssetHandle> set;

    auto h1 = MeshAssetHandle::from_u64(0x1111, 0x2222);
    auto h2 = MeshAssetHandle::from_u64(0x3333, 0x4444);
    auto h3 = MeshAssetHandle::from_u64(0x1111, 0x2222);

    set.insert(h1);
    set.insert(h2);
    set.insert(h3);

    REQUIRE(set.size() == 2);
    REQUIRE(set.count(h1) == 1);
    REQUIRE(set.count(h2) == 1);
}
