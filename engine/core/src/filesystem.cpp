#include <engine/core/filesystem.hpp>
#include <fstream>

namespace engine::core {

bool FileSystem::exists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

std::vector<uint8_t> FileSystem::read_binary(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};

    auto size = file.tellg();
    file.seekg(0);

    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

std::string FileSystem::read_text(const std::string& path) {
    std::ifstream file(path);
    if (!file) return {};

    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

bool FileSystem::write_binary(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return file.good();
}

bool FileSystem::write_text(const std::string& path, const std::string& text) {
    std::ofstream file(path);
    if (!file) return false;
    file << text;
    return file.good();
}

} // namespace engine::core
