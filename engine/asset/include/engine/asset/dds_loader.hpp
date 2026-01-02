#pragma once

#include <engine/render/types.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace engine::asset {

// DDS texture loader - loads DirectDraw Surface files with compressed textures
class DDSLoader {
public:
    // Loaded DDS data
    struct DDSData {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 1;
        uint32_t mip_levels = 1;
        uint32_t array_size = 1;
        render::TextureFormat format = render::TextureFormat::RGBA8;
        bool is_cubemap = false;
        std::vector<uint8_t> data;
    };

    // Load a DDS file
    // Returns true on success, false on failure
    static bool load(const std::string& path, DDSData& out_data);

    // Get the last error message
    static const std::string& get_last_error();

private:
    static std::string s_last_error;
};

} // namespace engine::asset
