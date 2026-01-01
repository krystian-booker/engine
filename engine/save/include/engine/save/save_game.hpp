#pragma once

#include <engine/core/serialize.hpp>
#include <engine/core/math.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <chrono>

namespace engine::save {

using namespace engine::core;

// Save file version for compatibility checking
constexpr uint32_t SAVE_VERSION = 1;
constexpr uint32_t SAVE_MAGIC = 0x53415645;  // "SAVE"

// Save game metadata
struct SaveGameMetadata {
    std::string name;
    std::string description;
    uint64_t timestamp = 0;           // Unix timestamp
    uint32_t play_time_seconds = 0;   // Total play time
    std::string level_name;           // Current level/scene
    uint32_t version = SAVE_VERSION;

    // Optional thumbnail
    std::vector<uint8_t> thumbnail_data;
    uint32_t thumbnail_width = 0;
    uint32_t thumbnail_height = 0;

    // Serialize metadata
    void serialize(IArchive& archive);

    // Get formatted date string
    std::string get_date_string() const;

    // Get formatted play time string
    std::string get_play_time_string() const;
};

// Save data chunk types
enum class SaveChunkType : uint32_t {
    Header = 0,
    Metadata,
    WorldState,
    EntityData,
    CustomData,
    Thumbnail,
    EndOfFile
};

// Save chunk header
struct SaveChunkHeader {
    SaveChunkType type;
    uint32_t size;  // Size of chunk data in bytes
    uint32_t checksum;  // CRC32 of chunk data
};

// Save game file - represents a complete save
class SaveGame {
public:
    SaveGame() = default;
    ~SaveGame() = default;

    // Metadata access
    SaveGameMetadata& metadata() { return m_metadata; }
    const SaveGameMetadata& metadata() const { return m_metadata; }

    // Store/retrieve custom data by key
    void set_data(const std::string& key, const std::vector<uint8_t>& data);
    bool get_data(const std::string& key, std::vector<uint8_t>& out_data) const;
    bool has_data(const std::string& key) const;
    void remove_data(const std::string& key);

    // Store/retrieve JSON data
    void set_json(const std::string& key, const json& data);
    json get_json(const std::string& key) const;

    // Store/retrieve typed values
    template<typename T>
    void set_value(const std::string& key, const T& value);

    template<typename T>
    T get_value(const std::string& key, const T& default_value = T{}) const;

    // Entity data storage (internal use by SaveSystem)
    void set_entity_data(uint64_t persistent_id, const std::string& json_data);
    std::string get_entity_data(uint64_t persistent_id) const;
    bool has_entity_data(uint64_t persistent_id) const;
    void clear_entity_data();
    std::vector<uint64_t> get_all_entity_ids() const;

    // File I/O
    bool save_to_file(const std::string& path) const;
    bool load_from_file(const std::string& path);

    // Binary serialization (for memory)
    std::vector<uint8_t> to_binary() const;
    bool from_binary(const std::vector<uint8_t>& data);

    // Clear all data
    void clear();

    // Validation
    bool is_valid() const { return m_is_valid; }
    uint32_t get_version() const { return m_metadata.version; }

private:
    bool write_chunk(std::ostream& stream, SaveChunkType type, const std::vector<uint8_t>& data) const;
    bool read_chunk(std::istream& stream, SaveChunkHeader& header, std::vector<uint8_t>& data);
    uint32_t calculate_checksum(const std::vector<uint8_t>& data) const;

    SaveGameMetadata m_metadata;
    std::unordered_map<std::string, std::vector<uint8_t>> m_custom_data;
    std::unordered_map<uint64_t, std::string> m_entity_data;
    bool m_is_valid = true;
};

// Template implementations

template<typename T>
void SaveGame::set_value(const std::string& key, const T& value) {
    JsonArchive archive;
    T mutable_value = value;  // Need non-const for serialize
    archive.serialize("value", mutable_value);
    set_json(key, archive.get_json());
}

template<typename T>
T SaveGame::get_value(const std::string& key, const T& default_value) const {
    if (!has_data(key)) {
        return default_value;
    }

    json j = get_json(key);
    if (j.is_null() || !j.contains("value")) {
        return default_value;
    }

    JsonArchive archive(j);
    T result = default_value;
    archive.serialize("value", result);
    return result;
}

// Convenience functions for common types
template<>
inline void SaveGame::set_value<std::string>(const std::string& key, const std::string& value) {
    json j;
    j["value"] = value;
    set_json(key, j);
}

template<>
inline std::string SaveGame::get_value<std::string>(const std::string& key, const std::string& default_value) const {
    json j = get_json(key);
    if (j.is_null() || !j.contains("value")) {
        return default_value;
    }
    return j["value"].get<std::string>();
}

template<>
inline void SaveGame::set_value<int>(const std::string& key, const int& value) {
    json j;
    j["value"] = value;
    set_json(key, j);
}

template<>
inline int SaveGame::get_value<int>(const std::string& key, const int& default_value) const {
    json j = get_json(key);
    if (j.is_null() || !j.contains("value")) {
        return default_value;
    }
    return j["value"].get<int>();
}

template<>
inline void SaveGame::set_value<float>(const std::string& key, const float& value) {
    json j;
    j["value"] = value;
    set_json(key, j);
}

template<>
inline float SaveGame::get_value<float>(const std::string& key, const float& default_value) const {
    json j = get_json(key);
    if (j.is_null() || !j.contains("value")) {
        return default_value;
    }
    return j["value"].get<float>();
}

template<>
inline void SaveGame::set_value<bool>(const std::string& key, const bool& value) {
    json j;
    j["value"] = value;
    set_json(key, j);
}

template<>
inline bool SaveGame::get_value<bool>(const std::string& key, const bool& default_value) const {
    json j = get_json(key);
    if (j.is_null() || !j.contains("value")) {
        return default_value;
    }
    return j["value"].get<bool>();
}

} // namespace engine::save
