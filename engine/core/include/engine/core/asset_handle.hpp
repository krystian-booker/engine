#pragma once

#include <engine/core/uuid.hpp>
#include <cstdint>
#include <functional>

namespace engine::core {

/**
 * @brief Asset type enumeration for type-safe asset handles
 * 
 * Each asset type has a unique tag that prevents accidentally using
 * handles of one type where another is expected.
 */
enum class AssetType : uint8_t {
    Unknown = 0,
    Mesh,
    Texture,
    Shader,
    Material,
    Audio,
    Animation,
    Skeleton,
    Scene,
    Prefab,
    Font,
    Script,
    // Add new types as needed
    Count
};

/**
 * @brief Get a human-readable name for an asset type
 */
[[nodiscard]] constexpr const char* asset_type_name(AssetType type) noexcept {
    switch (type) {
        case AssetType::Unknown:   return "Unknown";
        case AssetType::Mesh:      return "Mesh";
        case AssetType::Texture:   return "Texture";
        case AssetType::Shader:    return "Shader";
        case AssetType::Material:  return "Material";
        case AssetType::Audio:     return "Audio";
        case AssetType::Animation: return "Animation";
        case AssetType::Skeleton:  return "Skeleton";
        case AssetType::Scene:     return "Scene";
        case AssetType::Prefab:    return "Prefab";
        case AssetType::Font:      return "Font";
        case AssetType::Script:    return "Script";
        default:                   return "Invalid";
    }
}

/**
 * @brief Type-safe asset handle identified by UUID
 * 
 * AssetHandle provides stable, type-safe identification of assets that
 * survives file renames and moves. Unlike path-based identification,
 * a UUID handle remains valid even when the asset file is relocated.
 * 
 * The template parameter ensures compile-time type safety:
 * @code
 * MeshAssetHandle meshHandle = ...;
 * TextureAssetHandle texHandle = ...;
 * 
 * // This would be a compile error:
 * // meshHandle = texHandle;  // Error: incompatible types
 * @endcode
 * 
 * Example usage:
 * @code
 * // Create from UUID
 * auto handle = MeshAssetHandle(someUUID);
 * 
 * // Check validity
 * if (handle) {
 *     // Use handle...
 * }
 * 
 * // Get the UUID for serialization
 * UUID id = handle.uuid();
 * @endcode
 */
template<AssetType Type>
class AssetHandle {
public:
    /// Default constructor creates an invalid (null) handle
    constexpr AssetHandle() noexcept = default;

    /// Create handle from UUID
    explicit constexpr AssetHandle(UUID id) noexcept : m_id(id) {}

    /// Create handle from UUID components
    static constexpr AssetHandle from_u64(uint64_t high, uint64_t low) noexcept {
        return AssetHandle(UUID::from_u64(high, low));
    }

    /// Get the asset type
    [[nodiscard]] static constexpr AssetType type() noexcept { return Type; }

    /// Get the asset type name
    [[nodiscard]] static constexpr const char* type_name() noexcept {
        return asset_type_name(Type);
    }

    /// Check if this handle is valid (has a non-null UUID)
    [[nodiscard]] constexpr bool valid() const noexcept { return !m_id.is_null(); }

    /// Check if this handle is valid
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }

    /// Get the underlying UUID
    [[nodiscard]] constexpr const UUID& uuid() const noexcept { return m_id; }

    /// Compute hash for unordered containers
    [[nodiscard]] size_t hash() const noexcept { return m_id.hash(); }

    // Comparison operators (only compare handles of the same type)
    [[nodiscard]] constexpr bool operator==(const AssetHandle& other) const noexcept {
        return m_id == other.m_id;
    }
    [[nodiscard]] constexpr bool operator!=(const AssetHandle& other) const noexcept {
        return m_id != other.m_id;
    }
    [[nodiscard]] constexpr bool operator<(const AssetHandle& other) const noexcept {
        return m_id < other.m_id;
    }
    [[nodiscard]] constexpr bool operator<=(const AssetHandle& other) const noexcept {
        return m_id <= other.m_id;
    }
    [[nodiscard]] constexpr bool operator>(const AssetHandle& other) const noexcept {
        return m_id > other.m_id;
    }
    [[nodiscard]] constexpr bool operator>=(const AssetHandle& other) const noexcept {
        return m_id >= other.m_id;
    }

private:
    UUID m_id;
};

// Type aliases for common asset types
using MeshAssetHandle      = AssetHandle<AssetType::Mesh>;
using TextureAssetHandle   = AssetHandle<AssetType::Texture>;
using ShaderAssetHandle    = AssetHandle<AssetType::Shader>;
using MaterialAssetHandle  = AssetHandle<AssetType::Material>;
using AudioAssetHandle     = AssetHandle<AssetType::Audio>;
using AnimationAssetHandle = AssetHandle<AssetType::Animation>;
using SkeletonAssetHandle  = AssetHandle<AssetType::Skeleton>;
using SceneAssetHandle     = AssetHandle<AssetType::Scene>;
using PrefabAssetHandle    = AssetHandle<AssetType::Prefab>;
using FontAssetHandle      = AssetHandle<AssetType::Font>;
using ScriptAssetHandle    = AssetHandle<AssetType::Script>;

// Generic/untyped asset handle (for asset manager internals, serialization, etc.)
using GenericAssetHandle   = AssetHandle<AssetType::Unknown>;

} // namespace engine::core

// std::hash specializations for use in unordered containers
namespace std {
    template<engine::core::AssetType Type>
    struct hash<engine::core::AssetHandle<Type>> {
        [[nodiscard]] size_t operator()(const engine::core::AssetHandle<Type>& handle) const noexcept {
            return handle.hash();
        }
    };
} // namespace std
