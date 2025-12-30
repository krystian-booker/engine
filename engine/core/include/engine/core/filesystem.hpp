#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace engine::core {

struct FileSystem {
    static bool exists(const std::string& path);
    static std::vector<uint8_t> read_binary(const std::string& path);
    static std::string read_text(const std::string& path);
    static bool write_binary(const std::string& path, const std::vector<uint8_t>& data);
    static bool write_text(const std::string& path, const std::string& text);
};

} // namespace engine::core
