#pragma once

#include <engine/render/types.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace engine::asset {

// KTX texture loader - loads Khronos Texture files (KTX1 and KTX2)
class KTXLoader {
public:
    // Loaded KTX data
    struct KTXData {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 1;
        uint32_t mip_levels = 1;
        uint32_t array_size = 1;
        uint32_t faces = 1; // 6 for cubemaps
        render::TextureFormat format = render::TextureFormat::RGBA8;
        bool is_cubemap = false;
        std::vector<uint8_t> data;
    };

    // Load a KTX file (supports both KTX1 and KTX2)
    // Returns true on success, false on failure
    static bool load(const std::string& path, KTXData& out_data);

    // Get the last error message
    static const std::string& get_last_error();

private:
    static bool load_ktx1(const uint8_t* data, size_t size, KTXData& out_data);
    static bool load_ktx2(const uint8_t* data, size_t size, KTXData& out_data);

    static std::string s_last_error;
};

} // namespace engine::asset
