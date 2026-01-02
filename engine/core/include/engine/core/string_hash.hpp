#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <functional>

namespace engine::core {

// FNV-1a hash constants for 64-bit
namespace detail {
    constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    constexpr uint64_t FNV_PRIME = 1099511628211ULL;

    // Compile-time FNV-1a hash implementation
    constexpr uint64_t fnv1a_hash(const char* str, size_t len) {
        uint64_t hash = FNV_OFFSET_BASIS;
        for (size_t i = 0; i < len; ++i) {
            hash ^= static_cast<uint64_t>(static_cast<unsigned char>(str[i]));
            hash *= FNV_PRIME;
        }
        return hash;
    }

    // Null-terminated version
    constexpr uint64_t fnv1a_hash(const char* str) {
        uint64_t hash = FNV_OFFSET_BASIS;
        while (*str) {
            hash ^= static_cast<uint64_t>(static_cast<unsigned char>(*str));
            hash *= FNV_PRIME;
            ++str;
        }
        return hash;
    }
} // namespace detail

/**
 * @brief 64-bit string hash for fast comparisons
 * 
 * StringHash provides compile-time and runtime string hashing using the FNV-1a
 * algorithm. Use this instead of std::string comparisons in performance-critical
 * code paths (e.g., tag comparisons, asset lookups).
 * 
 * Example usage:
 * @code
 * // Compile-time hash via literal
 * constexpr auto PlayerTag = "Player"_sh;
 * 
 * // Runtime hash
 * StringHash dynamicTag(some_string);
 * 
 * // Fast comparison
 * if (tag == PlayerTag) { ... }
 * 
 * // Use in switch (compile-time only)
 * switch (tag.value()) {
 *     case "Player"_sh.value(): break;
 *     case "Enemy"_sh.value(): break;
 * }
 * @endcode
 */
class StringHash {
public:
    using HashType = uint64_t;

    /// Default constructor - creates an empty/invalid hash (value 0)
    constexpr StringHash() noexcept = default;

    /// Construct from null-terminated C string (compile-time capable)
    constexpr StringHash(const char* str) noexcept
        : m_hash(str ? detail::fnv1a_hash(str) : 0)
#ifdef ENGINE_DEBUG
        , m_debug_str(str)
#endif
    {}

    /// Construct from string_view (compile-time capable)
    constexpr StringHash(std::string_view str) noexcept
        : m_hash(detail::fnv1a_hash(str.data(), str.size()))
#ifdef ENGINE_DEBUG
        , m_debug_str(nullptr)  // Can't store pointer to temporary
#endif
    {}

    /// Construct from std::string (runtime only)
    explicit StringHash(const std::string& str) noexcept
        : m_hash(detail::fnv1a_hash(str.data(), str.size()))
#ifdef ENGINE_DEBUG
        , m_debug_str(nullptr)
#endif
    {}

    /// Construct from raw hash value (for deserialization)
    static constexpr StringHash from_hash(HashType hash) noexcept {
        StringHash result;
        result.m_hash = hash;
        return result;
    }

    /// Get the raw hash value
    [[nodiscard]] constexpr HashType value() const noexcept { return m_hash; }

    /// Check if this hash is valid (non-zero)
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return m_hash != 0; }

    /// Check if this hash is empty/invalid
    [[nodiscard]] constexpr bool empty() const noexcept { return m_hash == 0; }

    // Comparison operators
    [[nodiscard]] constexpr bool operator==(StringHash other) const noexcept {
        return m_hash == other.m_hash;
    }
    [[nodiscard]] constexpr bool operator!=(StringHash other) const noexcept {
        return m_hash != other.m_hash;
    }
    [[nodiscard]] constexpr bool operator<(StringHash other) const noexcept {
        return m_hash < other.m_hash;
    }
    [[nodiscard]] constexpr bool operator<=(StringHash other) const noexcept {
        return m_hash <= other.m_hash;
    }
    [[nodiscard]] constexpr bool operator>(StringHash other) const noexcept {
        return m_hash > other.m_hash;
    }
    [[nodiscard]] constexpr bool operator>=(StringHash other) const noexcept {
        return m_hash >= other.m_hash;
    }

#ifdef ENGINE_DEBUG
    /// Get debug string (only available in debug builds, may be nullptr)
    [[nodiscard]] const char* debug_string() const noexcept { return m_debug_str; }
#endif

private:
    HashType m_hash = 0;

#ifdef ENGINE_DEBUG
    const char* m_debug_str = nullptr;
#endif
};

/// Compile-time hash function for use in constexpr contexts
[[nodiscard]] constexpr uint64_t hash_string(const char* str) noexcept {
    return detail::fnv1a_hash(str);
}

[[nodiscard]] constexpr uint64_t hash_string(std::string_view str) noexcept {
    return detail::fnv1a_hash(str.data(), str.size());
}

/// User-defined literal for compile-time string hashing
/// Usage: auto hash = "Player"_sh;
[[nodiscard]] constexpr StringHash operator""_sh(const char* str, size_t len) noexcept {
    return StringHash(std::string_view(str, len));
}

} // namespace engine::core

// std::hash specialization for use in unordered containers
namespace std {
    template<>
    struct hash<engine::core::StringHash> {
        [[nodiscard]] size_t operator()(engine::core::StringHash sh) const noexcept {
            return static_cast<size_t>(sh.value());
        }
    };
} // namespace std
