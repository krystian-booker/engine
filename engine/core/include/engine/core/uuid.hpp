#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <optional>
#include <functional>
#include <array>

namespace engine::core {

/**
 * @brief 128-bit Universally Unique Identifier (RFC 4122 Version 4)
 * 
 * UUID provides a globally unique identifier for assets, entities, and other
 * objects that need stable identification across file renames, moves, and
 * across different machines/sessions.
 * 
 * This implementation uses RFC 4122 Version 4 (random) UUIDs.
 * 
 * Example usage:
 * @code
 * // Generate a new UUID
 * auto id = UUID::generate();
 * 
 * // Convert to/from string
 * std::string str = id.to_string();  // "550e8400-e29b-41d4-a716-446655440000"
 * auto parsed = UUID::from_string(str);
 * 
 * // Comparison
 * if (id1 == id2) { ... }
 * 
 * // Use in containers
 * std::unordered_map<UUID, Asset> assets;
 * @endcode
 */
class UUID {
public:
    static constexpr size_t BYTE_SIZE = 16;
    static constexpr size_t STRING_SIZE = 36;  // "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"

    /// Default constructor creates a null UUID (all zeros)
    constexpr UUID() noexcept = default;

    /// Generate a new random UUID (thread-safe)
    [[nodiscard]] static UUID generate();

    /// Create UUID from string "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
    /// Returns std::nullopt if parsing fails
    [[nodiscard]] static std::optional<UUID> from_string(std::string_view str);

    /// Create UUID from raw 16-byte array
    [[nodiscard]] static constexpr UUID from_bytes(const uint8_t* bytes) noexcept {
        UUID uuid;
        for (size_t i = 0; i < BYTE_SIZE; ++i) {
            uuid.m_bytes[i] = bytes[i];
        }
        return uuid;
    }

    /// Create UUID from two 64-bit values (high, low)
    [[nodiscard]] static constexpr UUID from_u64(uint64_t high, uint64_t low) noexcept {
        UUID uuid;
        // Store in big-endian order
        uuid.m_bytes[0] = static_cast<uint8_t>((high >> 56) & 0xFF);
        uuid.m_bytes[1] = static_cast<uint8_t>((high >> 48) & 0xFF);
        uuid.m_bytes[2] = static_cast<uint8_t>((high >> 40) & 0xFF);
        uuid.m_bytes[3] = static_cast<uint8_t>((high >> 32) & 0xFF);
        uuid.m_bytes[4] = static_cast<uint8_t>((high >> 24) & 0xFF);
        uuid.m_bytes[5] = static_cast<uint8_t>((high >> 16) & 0xFF);
        uuid.m_bytes[6] = static_cast<uint8_t>((high >> 8) & 0xFF);
        uuid.m_bytes[7] = static_cast<uint8_t>(high & 0xFF);
        uuid.m_bytes[8] = static_cast<uint8_t>((low >> 56) & 0xFF);
        uuid.m_bytes[9] = static_cast<uint8_t>((low >> 48) & 0xFF);
        uuid.m_bytes[10] = static_cast<uint8_t>((low >> 40) & 0xFF);
        uuid.m_bytes[11] = static_cast<uint8_t>((low >> 32) & 0xFF);
        uuid.m_bytes[12] = static_cast<uint8_t>((low >> 24) & 0xFF);
        uuid.m_bytes[13] = static_cast<uint8_t>((low >> 16) & 0xFF);
        uuid.m_bytes[14] = static_cast<uint8_t>((low >> 8) & 0xFF);
        uuid.m_bytes[15] = static_cast<uint8_t>(low & 0xFF);
        return uuid;
    }

    /// Get the null UUID (all zeros)
    [[nodiscard]] static constexpr UUID null() noexcept { return UUID{}; }

    /// Check if this UUID is null (all zeros)
    [[nodiscard]] constexpr bool is_null() const noexcept {
        for (size_t i = 0; i < BYTE_SIZE; ++i) {
            if (m_bytes[i] != 0) return false;
        }
        return true;
    }

    /// Check if this UUID is valid (non-null)
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return !is_null(); }

    /// Convert to string "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
    [[nodiscard]] std::string to_string() const;

    /// Access raw byte data
    [[nodiscard]] constexpr const uint8_t* data() const noexcept { return m_bytes; }
    
    /// Get byte at index
    [[nodiscard]] constexpr uint8_t operator[](size_t index) const noexcept { 
        return m_bytes[index]; 
    }

    /// Get the high 64 bits (for efficient comparison)
    [[nodiscard]] constexpr uint64_t high() const noexcept {
        return (static_cast<uint64_t>(m_bytes[0]) << 56) |
               (static_cast<uint64_t>(m_bytes[1]) << 48) |
               (static_cast<uint64_t>(m_bytes[2]) << 40) |
               (static_cast<uint64_t>(m_bytes[3]) << 32) |
               (static_cast<uint64_t>(m_bytes[4]) << 24) |
               (static_cast<uint64_t>(m_bytes[5]) << 16) |
               (static_cast<uint64_t>(m_bytes[6]) << 8) |
               static_cast<uint64_t>(m_bytes[7]);
    }

    /// Get the low 64 bits (for efficient comparison)
    [[nodiscard]] constexpr uint64_t low() const noexcept {
        return (static_cast<uint64_t>(m_bytes[8]) << 56) |
               (static_cast<uint64_t>(m_bytes[9]) << 48) |
               (static_cast<uint64_t>(m_bytes[10]) << 40) |
               (static_cast<uint64_t>(m_bytes[11]) << 32) |
               (static_cast<uint64_t>(m_bytes[12]) << 24) |
               (static_cast<uint64_t>(m_bytes[13]) << 16) |
               (static_cast<uint64_t>(m_bytes[14]) << 8) |
               static_cast<uint64_t>(m_bytes[15]);
    }

    /// Compute hash for unordered containers
    [[nodiscard]] size_t hash() const noexcept;

    // Comparison operators
    [[nodiscard]] constexpr bool operator==(const UUID& other) const noexcept {
        for (size_t i = 0; i < BYTE_SIZE; ++i) {
            if (m_bytes[i] != other.m_bytes[i]) return false;
        }
        return true;
    }
    [[nodiscard]] constexpr bool operator!=(const UUID& other) const noexcept {
        return !(*this == other);
    }
    [[nodiscard]] constexpr bool operator<(const UUID& other) const noexcept {
        for (size_t i = 0; i < BYTE_SIZE; ++i) {
            if (m_bytes[i] < other.m_bytes[i]) return true;
            if (m_bytes[i] > other.m_bytes[i]) return false;
        }
        return false;  // Equal
    }
    [[nodiscard]] constexpr bool operator<=(const UUID& other) const noexcept {
        return !(other < *this);
    }
    [[nodiscard]] constexpr bool operator>(const UUID& other) const noexcept {
        return other < *this;
    }
    [[nodiscard]] constexpr bool operator>=(const UUID& other) const noexcept {
        return !(*this < other);
    }

private:
    uint8_t m_bytes[BYTE_SIZE] = {0};
};

} // namespace engine::core

// std::hash specialization for use in unordered containers
namespace std {
    template<>
    struct hash<engine::core::UUID> {
        [[nodiscard]] size_t operator()(const engine::core::UUID& uuid) const noexcept {
            return uuid.hash();
        }
    };
} // namespace std
