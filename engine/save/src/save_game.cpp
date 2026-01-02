#include <engine/save/save_game.hpp>
#include <engine/core/log.hpp>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <limits>
#include <filesystem>

namespace engine::save {

// SaveGameMetadata implementation

void SaveGameMetadata::serialize(IArchive& archive) {
    archive.serialize("name", name);
    archive.serialize("description", description);
    archive.serialize("level_name", level_name);

    // Handle uint64_t as two uint32_t for portability
    if (archive.is_writing()) {
        uint32_t ts_low = static_cast<uint32_t>(timestamp & 0xFFFFFFFF);
        uint32_t ts_high = static_cast<uint32_t>((timestamp >> 32) & 0xFFFFFFFF);
        archive.serialize("timestamp_low", ts_low);
        archive.serialize("timestamp_high", ts_high);
    } else {
        uint32_t ts_low = 0, ts_high = 0;
        archive.serialize("timestamp_low", ts_low);
        archive.serialize("timestamp_high", ts_high);
        timestamp = (static_cast<uint64_t>(ts_high) << 32) | ts_low;
    }

    archive.serialize("play_time_seconds", play_time_seconds);
    archive.serialize("version", version);
    archive.serialize("thumbnail_width", thumbnail_width);
    archive.serialize("thumbnail_height", thumbnail_height);
}

std::string SaveGameMetadata::get_date_string() const {
    std::time_t time = static_cast<std::time_t>(timestamp);
    std::tm* tm_info = std::localtime(&time);
    if (!tm_info) return "Unknown";

    std::ostringstream oss;
    oss << std::put_time(tm_info, "%Y-%m-%d %H:%M");
    return oss.str();
}

std::string SaveGameMetadata::get_play_time_string() const {
    uint32_t hours = play_time_seconds / 3600;
    uint32_t minutes = (play_time_seconds % 3600) / 60;
    uint32_t seconds = play_time_seconds % 60;

    std::ostringstream oss;
    if (hours > 0) {
        oss << hours << "h " << minutes << "m";
    } else if (minutes > 0) {
        oss << minutes << "m " << seconds << "s";
    } else {
        oss << seconds << "s";
    }
    return oss.str();
}

// SaveGame implementation

void SaveGame::set_data(const std::string& key, const std::vector<uint8_t>& data) {
    m_custom_data[key] = data;
}

bool SaveGame::get_data(const std::string& key, std::vector<uint8_t>& out_data) const {
    auto it = m_custom_data.find(key);
    if (it == m_custom_data.end()) return false;
    out_data = it->second;
    return true;
}

bool SaveGame::has_data(const std::string& key) const {
    return m_custom_data.find(key) != m_custom_data.end();
}

void SaveGame::remove_data(const std::string& key) {
    m_custom_data.erase(key);
}

void SaveGame::set_json(const std::string& key, const json& data) {
    std::string json_str = data.dump();
    std::vector<uint8_t> bytes(json_str.begin(), json_str.end());
    set_data(key, bytes);
}

json SaveGame::get_json(const std::string& key) const {
    std::vector<uint8_t> bytes;
    if (!get_data(key, bytes)) return json{};
    std::string json_str(bytes.begin(), bytes.end());
    try {
        return json::parse(json_str);
    } catch (const std::exception& e) {
        core::log(core::LogLevel::Error, "SaveGame: Failed to parse JSON for key '{}': {}", key, e.what());
        return json{};
    }
}

void SaveGame::set_entity_data(uint64_t persistent_id, const std::string& json_data) {
    m_entity_data[persistent_id] = json_data;
}

std::string SaveGame::get_entity_data(uint64_t persistent_id) const {
    auto it = m_entity_data.find(persistent_id);
    return it != m_entity_data.end() ? it->second : "";
}

bool SaveGame::has_entity_data(uint64_t persistent_id) const {
    return m_entity_data.find(persistent_id) != m_entity_data.end();
}

void SaveGame::clear_entity_data() {
    m_entity_data.clear();
}

std::vector<uint64_t> SaveGame::get_all_entity_ids() const {
    std::vector<uint64_t> ids;
    ids.reserve(m_entity_data.size());
    for (const auto& [id, data] : m_entity_data) {
        ids.push_back(id);
    }
    return ids;
}

bool SaveGame::save_to_file(const std::string& path) const {
    namespace fs = std::filesystem;

    // Write to temp file first for atomic operation
    std::string temp_path = path + ".tmp";

    std::ofstream file(temp_path, std::ios::binary);
    if (!file.is_open()) return false;

    // Write magic number and version
    file.write(reinterpret_cast<const char*>(&SAVE_MAGIC), sizeof(SAVE_MAGIC));
    file.write(reinterpret_cast<const char*>(&SAVE_VERSION), sizeof(SAVE_VERSION));

    // Serialize metadata
    JsonArchive metadata_archive;
    SaveGameMetadata mutable_meta = m_metadata;
    mutable_meta.serialize(metadata_archive);
    std::string metadata_json = metadata_archive.to_string();
    std::vector<uint8_t> metadata_bytes(metadata_json.begin(), metadata_json.end());
    if (!write_chunk(file, SaveChunkType::Metadata, metadata_bytes)) {
        file.close();
        fs::remove(temp_path);
        return false;
    }

    // Write thumbnail if present
    if (!m_metadata.thumbnail_data.empty()) {
        if (!write_chunk(file, SaveChunkType::Thumbnail, m_metadata.thumbnail_data)) {
            file.close();
            fs::remove(temp_path);
            return false;
        }
    }

    // Write entity data
    json entity_json;
    for (const auto& [id, data] : m_entity_data) {
        entity_json[std::to_string(id)] = data;
    }
    std::string entity_str = entity_json.dump();
    std::vector<uint8_t> entity_bytes(entity_str.begin(), entity_str.end());
    if (!write_chunk(file, SaveChunkType::EntityData, entity_bytes)) {
        file.close();
        fs::remove(temp_path);
        return false;
    }

    // Write custom data
    json custom_json;
    for (const auto& [key, data] : m_custom_data) {
        // Encode binary data as hex
        std::string hex;
        for (uint8_t byte : data) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02x", byte);
            hex += buf;
        }
        custom_json[key] = hex;
    }
    std::string custom_str = custom_json.dump();
    std::vector<uint8_t> custom_bytes(custom_str.begin(), custom_str.end());
    if (!write_chunk(file, SaveChunkType::CustomData, custom_bytes)) {
        file.close();
        fs::remove(temp_path);
        return false;
    }

    // Write end of file marker
    std::vector<uint8_t> empty;
    if (!write_chunk(file, SaveChunkType::EndOfFile, empty)) {
        file.close();
        fs::remove(temp_path);
        return false;
    }

    if (!file.good()) {
        file.close();
        fs::remove(temp_path);
        return false;
    }

    file.close();

    // Atomic rename: replace target file with temp file
    std::error_code ec;
    fs::rename(temp_path, path, ec);
    if (ec) {
        core::log(core::LogLevel::Error, "SaveGame: Failed to rename temp file: {}", ec.message());
        fs::remove(temp_path);
        return false;
    }

    return true;
}

bool SaveGame::load_from_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    clear();

    // Read and verify magic number
    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != SAVE_MAGIC) {
        m_is_valid = false;
        return false;
    }

    // Read version
    uint32_t version = 0;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version > SAVE_VERSION) {
        m_is_valid = false;
        return false;  // Future version, can't load
    }

    // Read chunks
    SaveChunkHeader header;
    std::vector<uint8_t> data;

    while (read_chunk(file, header, data)) {
        switch (header.type) {
            case SaveChunkType::Metadata: {
                std::string json_str(data.begin(), data.end());
                JsonArchive archive(json_str);
                m_metadata.serialize(archive);
                break;
            }
            case SaveChunkType::Thumbnail: {
                m_metadata.thumbnail_data = std::move(data);
                break;
            }
            case SaveChunkType::EntityData: {
                std::string json_str(data.begin(), data.end());
                try {
                    json entity_json = json::parse(json_str);
                    for (auto& [id_str, entity_data] : entity_json.items()) {
                        uint64_t id = std::stoull(id_str);
                        m_entity_data[id] = entity_data.get<std::string>();
                    }
                } catch (const std::exception& e) {
                    core::log(core::LogLevel::Error, "SaveGame: Failed to parse entity data: {}", e.what());
                    m_is_valid = false;
                }
                break;
            }
            case SaveChunkType::CustomData: {
                std::string json_str(data.begin(), data.end());
                try {
                    json custom_json = json::parse(json_str);
                    for (auto& [key, hex_value] : custom_json.items()) {
                        std::string hex = hex_value.get<std::string>();
                        std::vector<uint8_t> bytes;
                        for (size_t i = 0; i < hex.length(); i += 2) {
                            uint8_t byte = static_cast<uint8_t>(
                                std::stoi(hex.substr(i, 2), nullptr, 16)
                            );
                            bytes.push_back(byte);
                        }
                        m_custom_data[key] = bytes;
                    }
                } catch (const std::exception& e) {
                    core::log(core::LogLevel::Error, "SaveGame: Failed to parse custom data: {}", e.what());
                    m_is_valid = false;
                }
                break;
            }
            case SaveChunkType::EndOfFile:
                return m_is_valid;
            default:
                // Unknown chunk, skip it
                break;
        }
    }

    return m_is_valid && file.eof();
}

std::vector<uint8_t> SaveGame::to_binary() const {
    std::ostringstream oss(std::ios::binary);

    // Write magic and version
    oss.write(reinterpret_cast<const char*>(&SAVE_MAGIC), sizeof(SAVE_MAGIC));
    oss.write(reinterpret_cast<const char*>(&SAVE_VERSION), sizeof(SAVE_VERSION));

    // Use same format as file save
    JsonArchive metadata_archive;
    SaveGameMetadata mutable_meta = m_metadata;
    mutable_meta.serialize(metadata_archive);
    std::string metadata_json = metadata_archive.to_string();
    std::vector<uint8_t> metadata_bytes(metadata_json.begin(), metadata_json.end());

    // Validate size doesn't exceed uint32_t max
    if (metadata_bytes.size() > std::numeric_limits<uint32_t>::max()) {
        return {};  // Return empty vector on overflow
    }

    // Write metadata chunk
    SaveChunkHeader header;
    header.type = SaveChunkType::Metadata;
    header.size = static_cast<uint32_t>(metadata_bytes.size());
    header.checksum = calculate_checksum(metadata_bytes);
    oss.write(reinterpret_cast<const char*>(&header), sizeof(header));
    oss.write(reinterpret_cast<const char*>(metadata_bytes.data()), metadata_bytes.size());

    std::string str = oss.str();
    return std::vector<uint8_t>(str.begin(), str.end());
}

bool SaveGame::from_binary(const std::vector<uint8_t>& data) {
    std::istringstream iss(std::string(data.begin(), data.end()), std::ios::binary);

    uint32_t magic = 0;
    iss.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != SAVE_MAGIC) return false;

    uint32_t version = 0;
    iss.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version > SAVE_VERSION) return false;

    // Read metadata chunk
    SaveChunkHeader header;
    iss.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (header.type != SaveChunkType::Metadata) return false;

    std::vector<uint8_t> chunk_data(header.size);
    iss.read(reinterpret_cast<char*>(chunk_data.data()), header.size);

    std::string json_str(chunk_data.begin(), chunk_data.end());
    JsonArchive archive(json_str);
    m_metadata.serialize(archive);

    return true;
}

void SaveGame::clear() {
    m_metadata = SaveGameMetadata{};
    m_custom_data.clear();
    m_entity_data.clear();
    m_is_valid = true;
}

bool SaveGame::write_chunk(std::ostream& stream, SaveChunkType type, const std::vector<uint8_t>& data) const {
    // Validate size doesn't exceed uint32_t max
    if (data.size() > std::numeric_limits<uint32_t>::max()) {
        return false;
    }

    SaveChunkHeader header;
    header.type = type;
    header.size = static_cast<uint32_t>(data.size());
    header.checksum = calculate_checksum(data);

    stream.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!data.empty()) {
        stream.write(reinterpret_cast<const char*>(data.data()), data.size());
    }

    return stream.good();
}

bool SaveGame::read_chunk(std::istream& stream, SaveChunkHeader& header, std::vector<uint8_t>& data) {
    stream.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!stream.good()) return false;

    // Validate chunk size to prevent OOM from corrupted/malicious files
    if (header.size > MAX_CHUNK_SIZE) {
        m_is_valid = false;
        return false;
    }

    if (header.size > 0) {
        data.resize(header.size);
        stream.read(reinterpret_cast<char*>(data.data()), header.size);
        if (!stream.good()) return false;

        // Verify checksum
        uint32_t calculated = calculate_checksum(data);
        if (calculated != header.checksum) {
            m_is_valid = false;
            return false;
        }
    } else {
        data.clear();
    }

    return true;
}

uint32_t SaveGame::calculate_checksum(const std::vector<uint8_t>& data) const {
    // Simple CRC-like checksum
    uint32_t crc = 0xFFFFFFFF;
    for (uint8_t byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

} // namespace engine::save
