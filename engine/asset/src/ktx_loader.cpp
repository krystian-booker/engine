#include <engine/asset/ktx_loader.hpp>
#include <engine/core/filesystem.hpp>
#include <engine/core/log.hpp>
#include <cstring>

namespace engine::asset {

using namespace engine::core;
using namespace engine::render;

std::string KTXLoader::s_last_error;

// KTX1 file identifier
static const uint8_t KTX1_IDENTIFIER[12] = {
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

// KTX2 file identifier
static const uint8_t KTX2_IDENTIFIER[12] = {
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

// Common OpenGL internal formats
constexpr uint32_t GL_COMPRESSED_RGB_S3TC_DXT1_EXT = 0x83F0;
constexpr uint32_t GL_COMPRESSED_RGBA_S3TC_DXT1_EXT = 0x83F1;
constexpr uint32_t GL_COMPRESSED_RGBA_S3TC_DXT3_EXT = 0x83F2;
constexpr uint32_t GL_COMPRESSED_RGBA_S3TC_DXT5_EXT = 0x83F3;
constexpr uint32_t GL_COMPRESSED_RGBA_BPTC_UNORM = 0x8E8C;
constexpr uint32_t GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM = 0x8E8D;
constexpr uint32_t GL_RGBA8 = 0x8058;
constexpr uint32_t GL_RGBA16F = 0x881A;

// VkFormat equivalents for KTX2
constexpr uint32_t VK_FORMAT_BC1_RGB_UNORM_BLOCK = 131;
constexpr uint32_t VK_FORMAT_BC1_RGBA_UNORM_BLOCK = 133;
constexpr uint32_t VK_FORMAT_BC3_UNORM_BLOCK = 137;
constexpr uint32_t VK_FORMAT_BC7_UNORM_BLOCK = 145;
constexpr uint32_t VK_FORMAT_R8G8B8A8_UNORM = 37;
constexpr uint32_t VK_FORMAT_R16G16B16A16_SFLOAT = 97;

#pragma pack(push, 1)
struct KTX1Header {
    uint8_t identifier[12];
    uint32_t endianness;
    uint32_t glType;
    uint32_t glTypeSize;
    uint32_t glFormat;
    uint32_t glInternalFormat;
    uint32_t glBaseInternalFormat;
    uint32_t pixelWidth;
    uint32_t pixelHeight;
    uint32_t pixelDepth;
    uint32_t numberOfArrayElements;
    uint32_t numberOfFaces;
    uint32_t numberOfMipmapLevels;
    uint32_t bytesOfKeyValueData;
};

struct KTX2Header {
    uint8_t identifier[12];
    uint32_t vkFormat;
    uint32_t typeSize;
    uint32_t pixelWidth;
    uint32_t pixelHeight;
    uint32_t pixelDepth;
    uint32_t layerCount;
    uint32_t faceCount;
    uint32_t levelCount;
    uint32_t supercompressionScheme;
};

struct KTX2LevelIndex {
    uint64_t byteOffset;
    uint64_t byteLength;
    uint64_t uncompressedByteLength;
};
#pragma pack(pop)

static TextureFormat gl_to_format(uint32_t gl_internal_format) {
    switch (gl_internal_format) {
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            return TextureFormat::BC1;
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            return TextureFormat::BC3;
        case GL_COMPRESSED_RGBA_BPTC_UNORM:
        case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:
            return TextureFormat::BC7;
        case GL_RGBA8:
            return TextureFormat::RGBA8;
        case GL_RGBA16F:
            return TextureFormat::RGBA16F;
        default:
            return TextureFormat::RGBA8;
    }
}

static TextureFormat vk_to_format(uint32_t vk_format) {
    switch (vk_format) {
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
            return TextureFormat::BC1;
        case VK_FORMAT_BC3_UNORM_BLOCK:
            return TextureFormat::BC3;
        case VK_FORMAT_BC7_UNORM_BLOCK:
            return TextureFormat::BC7;
        case VK_FORMAT_R8G8B8A8_UNORM:
            return TextureFormat::RGBA8;
        case VK_FORMAT_R16G16B16A16_SFLOAT:
            return TextureFormat::RGBA16F;
        default:
            return TextureFormat::RGBA8;
    }
}

static size_t get_block_size(TextureFormat format) {
    switch (format) {
        case TextureFormat::BC1: return 8;
        case TextureFormat::BC3: return 16;
        case TextureFormat::BC7: return 16;
        default: return 0;
    }
}

static size_t calculate_level_size(uint32_t width, uint32_t height, TextureFormat format) {
    size_t block_size = get_block_size(format);
    if (block_size == 0) {
        // Uncompressed
        if (format == TextureFormat::RGBA16F) {
            return static_cast<size_t>(width) * height * 8; // 4 channels * 2 bytes
        }
        return static_cast<size_t>(width) * height * 4; // RGBA8
    }

    uint32_t blocks_x = std::max(1u, (width + 3) / 4);
    uint32_t blocks_y = std::max(1u, (height + 3) / 4);
    return static_cast<size_t>(blocks_x) * blocks_y * block_size;
}

bool KTXLoader::load_ktx1(const uint8_t* data, size_t size, KTXData& out_data) {
    if (size < sizeof(KTX1Header)) {
        s_last_error = "KTX1 file too small for header";
        return false;
    }

    KTX1Header header;
    std::memcpy(&header, data, sizeof(header));

    // Check endianness
    if (header.endianness != 0x04030201) {
        s_last_error = "KTX1 file has wrong endianness (big-endian not supported)";
        return false;
    }

    out_data.width = header.pixelWidth;
    out_data.height = std::max(1u, header.pixelHeight);
    out_data.depth = std::max(1u, header.pixelDepth);
    out_data.mip_levels = std::max(1u, header.numberOfMipmapLevels);
    out_data.array_size = std::max(1u, header.numberOfArrayElements);
    out_data.faces = header.numberOfFaces;
    out_data.is_cubemap = (header.numberOfFaces == 6);
    out_data.format = gl_to_format(header.glInternalFormat);

    // Skip key-value data
    const uint8_t* ptr = data + sizeof(KTX1Header) + header.bytesOfKeyValueData;

    // Read mip levels
    std::vector<uint8_t> all_data;
    for (uint32_t level = 0; level < out_data.mip_levels; level++) {
        if (ptr + 4 > data + size) {
            s_last_error = "KTX1 file truncated at level " + std::to_string(level);
            return false;
        }

        uint32_t image_size;
        std::memcpy(&image_size, ptr, sizeof(image_size));
        ptr += 4;

        // For cubemaps and arrays, data is stored per-face/layer
        uint32_t face_count = out_data.faces;
        size_t total_face_size = 0;

        for (uint32_t face = 0; face < face_count; face++) {
            if (ptr + image_size > data + size) {
                s_last_error = "KTX1 file truncated in image data";
                return false;
            }

            size_t old_size = all_data.size();
            all_data.resize(old_size + image_size);
            std::memcpy(all_data.data() + old_size, ptr, image_size);
            ptr += image_size;
            total_face_size += image_size;

            // Align to 4 bytes
            size_t padding = (4 - (image_size % 4)) % 4;
            ptr += padding;
        }
    }

    out_data.data = std::move(all_data);
    return true;
}

bool KTXLoader::load_ktx2(const uint8_t* data, size_t size, KTXData& out_data) {
    if (size < sizeof(KTX2Header)) {
        s_last_error = "KTX2 file too small for header";
        return false;
    }

    KTX2Header header;
    std::memcpy(&header, data, sizeof(header));

    out_data.width = header.pixelWidth;
    out_data.height = std::max(1u, header.pixelHeight);
    out_data.depth = std::max(1u, header.pixelDepth);
    out_data.mip_levels = std::max(1u, header.levelCount);
    out_data.array_size = std::max(1u, header.layerCount);
    out_data.faces = header.faceCount;
    out_data.is_cubemap = (header.faceCount == 6);
    out_data.format = vk_to_format(header.vkFormat);

    // Check supercompression
    if (header.supercompressionScheme != 0) {
        s_last_error = "KTX2 supercompression not supported (scheme: " +
            std::to_string(header.supercompressionScheme) + ")";
        return false;
    }

    // Read level index
    const uint8_t* ptr = data + sizeof(KTX2Header);

    // Calculate expected level index size
    size_t level_index_size = static_cast<size_t>(out_data.mip_levels) * sizeof(KTX2LevelIndex);
    if (ptr + level_index_size > data + size) {
        s_last_error = "KTX2 file truncated in level index";
        return false;
    }

    std::vector<KTX2LevelIndex> level_indices(out_data.mip_levels);
    std::memcpy(level_indices.data(), ptr, level_index_size);

    // Read all levels
    std::vector<uint8_t> all_data;
    for (uint32_t level = 0; level < out_data.mip_levels; level++) {
        const auto& idx = level_indices[level];
        if (idx.byteOffset + idx.byteLength > size) {
            s_last_error = "KTX2 file truncated at level " + std::to_string(level);
            return false;
        }

        size_t old_size = all_data.size();
        all_data.resize(old_size + static_cast<size_t>(idx.byteLength));
        std::memcpy(all_data.data() + old_size, data + idx.byteOffset, static_cast<size_t>(idx.byteLength));
    }

    out_data.data = std::move(all_data);
    return true;
}

bool KTXLoader::load(const std::string& path, KTXData& out_data) {
    s_last_error.clear();

    // Read file
    auto file_data = FileSystem::read_binary(path);
    if (file_data.empty()) {
        s_last_error = "Failed to read KTX file: " + path;
        log(LogLevel::Error, s_last_error.c_str());
        return false;
    }

    if (file_data.size() < 12) {
        s_last_error = "Invalid KTX file: too small";
        log(LogLevel::Error, s_last_error.c_str());
        return false;
    }

    // Detect KTX version
    bool is_ktx1 = (std::memcmp(file_data.data(), KTX1_IDENTIFIER, 12) == 0);
    bool is_ktx2 = (std::memcmp(file_data.data(), KTX2_IDENTIFIER, 12) == 0);

    if (!is_ktx1 && !is_ktx2) {
        s_last_error = "Invalid KTX file: unrecognized format";
        log(LogLevel::Error, s_last_error.c_str());
        return false;
    }

    bool success;
    if (is_ktx1) {
        success = load_ktx1(file_data.data(), file_data.size(), out_data);
    } else {
        success = load_ktx2(file_data.data(), file_data.size(), out_data);
    }

    if (!success) {
        log(LogLevel::Error, s_last_error.c_str());
        return false;
    }

    log(LogLevel::Debug, ("Loaded KTX: " + path + " (" +
        std::to_string(out_data.width) + "x" + std::to_string(out_data.height) +
        ", " + std::to_string(out_data.mip_levels) + " mips, " +
        (is_ktx1 ? "KTX1" : "KTX2") + ")").c_str());

    return true;
}

const std::string& KTXLoader::get_last_error() {
    return s_last_error;
}

} // namespace engine::asset
