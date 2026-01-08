// Minimal test to verify new headers compile
#include <engine/core/string_hash.hpp>
#include <engine/core/uuid.hpp>
#include <engine/core/asset_handle.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("StringHash and UUID compile-time checks", "[core][hash][uuid]") {
    using namespace engine::core;
    
    // Test StringHash
    constexpr auto hash1 = "Player"_sh;
    constexpr auto hash2 = "Enemy"_sh;
    static_assert(hash1 != hash2, "Different strings should have different hashes");
    static_assert(hash1 == StringHash("Player"), "Same strings should have same hash");
    
    SECTION("Runtime StringHash") {
        StringHash runtime_hash(std::string("Dynamic"));
        REQUIRE(runtime_hash == "Dynamic"_sh);
    }
    
    // Test UUID (compile-time only - no generation test)
    constexpr UUID null_uuid = UUID::null();
    static_assert(null_uuid.is_null(), "Null UUID should be null");
    
    constexpr auto from_parts = UUID::from_u64(0x123456789ABCDEF0ULL, 0xFEDCBA9876543210ULL);
    static_assert(!from_parts.is_null(), "Non-zero UUID should not be null");
    
    // Test AssetHandle
    constexpr MeshAssetHandle mesh_handle;
    static_assert(!mesh_handle.valid() == true, "Default handle should be invalid");
    static_assert(MeshAssetHandle::type() == AssetType::Mesh, "Type should match");
    
    SECTION("TextureAssetHandle validation") {
        TextureAssetHandle tex_handle(from_parts);
        REQUIRE(tex_handle.valid());
        
        UUID id = tex_handle.uuid();
        REQUIRE(!id.is_null());
    }
}
