#include <engine/core/uuid.hpp>
#include <random>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <cctype>

namespace engine::core {

namespace {
    // Thread-safe random number generation for UUID
    std::mutex g_uuid_mutex;
    
    // Initialize random generator lazily to avoid static initialization order issues
    std::mt19937_64& get_generator() {
        static std::random_device rd;
        static std::mt19937_64 gen(rd());
        return gen;
    }
    
    std::uniform_int_distribution<uint64_t>& get_distribution() {
        static std::uniform_int_distribution<uint64_t> dist;
        return dist;
    }

    // Hex character to nibble value
    constexpr int hex_to_nibble(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    // Nibble to hex character
    constexpr char nibble_to_hex(uint8_t nibble) {
        return "0123456789abcdef"[nibble & 0x0F];
    }
} // anonymous namespace

UUID UUID::generate() {
    std::lock_guard<std::mutex> lock(g_uuid_mutex);
    
    UUID uuid;
    
    // Generate random bytes
    auto& gen = get_generator();
    auto& dist = get_distribution();
    
    uint64_t high = dist(gen);
    uint64_t low = dist(gen);
    
    // Store bytes in big-endian order
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
    
    // Set version 4 (random) bits: byte 6, high nibble = 0100
    uuid.m_bytes[6] = (uuid.m_bytes[6] & 0x0F) | 0x40;
    
    // Set variant 1 bits: byte 8, high 2 bits = 10
    uuid.m_bytes[8] = (uuid.m_bytes[8] & 0x3F) | 0x80;
    
    return uuid;
}

std::optional<UUID> UUID::from_string(std::string_view str) {
    // Expected format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx (36 chars)
    // Or without hyphens: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx (32 chars)
    
    if (str.size() != STRING_SIZE && str.size() != 32) {
        return std::nullopt;
    }
    
    UUID uuid;
    size_t byte_index = 0;
    size_t i = 0;
    
    while (i < str.size() && byte_index < BYTE_SIZE) {
        // Skip hyphens
        if (str[i] == '-') {
            ++i;
            continue;
        }
        
        // Need two hex characters for one byte
        if (i + 1 >= str.size()) {
            return std::nullopt;
        }
        
        int high_nibble = hex_to_nibble(str[i]);
        int low_nibble = hex_to_nibble(str[i + 1]);
        
        if (high_nibble < 0 || low_nibble < 0) {
            return std::nullopt;
        }
        
        uuid.m_bytes[byte_index] = static_cast<uint8_t>((high_nibble << 4) | low_nibble);
        ++byte_index;
        i += 2;
    }
    
    if (byte_index != BYTE_SIZE) {
        return std::nullopt;
    }
    
    return uuid;
}

std::string UUID::to_string() const {
    // Format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    std::string result;
    result.reserve(STRING_SIZE);
    
    for (size_t i = 0; i < BYTE_SIZE; ++i) {
        // Add hyphens at positions 4, 6, 8, 10 (after bytes 3, 5, 7, 9)
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            result += '-';
        }
        result += nibble_to_hex(m_bytes[i] >> 4);
        result += nibble_to_hex(m_bytes[i] & 0x0F);
    }
    
    return result;
}

size_t UUID::hash() const noexcept {
    // Combine high and low parts for hash
    // Use a simple but effective hash combining technique
    size_t h = high();
    h ^= low() + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
}

} // namespace engine::core
