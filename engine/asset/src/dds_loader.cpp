#include <engine/asset/dds_loader.hpp>
#include <engine/core/filesystem.hpp>
#include <engine/core/log.hpp>
#include <cstring>

namespace engine::asset {

using namespace engine::core;
using namespace engine::render;

std::string DDSLoader::s_last_error;

// DDS file format constants
constexpr uint32_t DDS_MAGIC = 0x20534444; // "DDS "

// DDS header flags
constexpr uint32_t DDSD_CAPS = 0x1;
constexpr uint32_t DDSD_HEIGHT = 0x2;
constexpr uint32_t DDSD_WIDTH = 0x4;
constexpr uint32_t DDSD_PITCH = 0x8;
constexpr uint32_t DDSD_PIXELFORMAT = 0x1000;
constexpr uint32_t DDSD_MIPMAPCOUNT = 0x20000;
constexpr uint32_t DDSD_LINEARSIZE = 0x80000;
constexpr uint32_t DDSD_DEPTH = 0x800000;

// DDS pixel format flags
constexpr uint32_t DDPF_ALPHAPIXELS = 0x1;
constexpr uint32_t DDPF_ALPHA = 0x2;
constexpr uint32_t DDPF_FOURCC = 0x4;
constexpr uint32_t DDPF_RGB = 0x40;
constexpr uint32_t DDPF_YUV = 0x200;
constexpr uint32_t DDPF_LUMINANCE = 0x20000;

// DDS caps flags
constexpr uint32_t DDSCAPS_COMPLEX = 0x8;
constexpr uint32_t DDSCAPS_MIPMAP = 0x400000;
constexpr uint32_t DDSCAPS_TEXTURE = 0x1000;
constexpr uint32_t DDSCAPS2_CUBEMAP = 0x200;
constexpr uint32_t DDSCAPS2_CUBEMAP_ALLFACES = 0xFC00;
constexpr uint32_t DDSCAPS2_VOLUME = 0x200000;

// FourCC codes
constexpr uint32_t FOURCC_DXT1 = 0x31545844; // "DXT1"
constexpr uint32_t FOURCC_DXT3 = 0x33545844; // "DXT3"
constexpr uint32_t FOURCC_DXT5 = 0x35545844; // "DXT5"
constexpr uint32_t FOURCC_DX10 = 0x30315844; // "DX10"
constexpr uint32_t FOURCC_BC4U = 0x55344342; // "BC4U"
constexpr uint32_t FOURCC_BC4S = 0x53344342; // "BC4S"
constexpr uint32_t FOURCC_BC5U = 0x55354342; // "BC5U"
constexpr uint32_t FOURCC_BC5S = 0x53354342; // "BC5S"
constexpr uint32_t FOURCC_ATI1 = 0x31495441; // "ATI1" (BC4)
constexpr uint32_t FOURCC_ATI2 = 0x32495441; // "ATI2" (BC5)

// DXGI formats for DX10 header
enum DXGI_FORMAT {
    DXGI_FORMAT_BC1_UNORM = 71,
    DXGI_FORMAT_BC1_UNORM_SRGB = 72,
    DXGI_FORMAT_BC2_UNORM = 74,
    DXGI_FORMAT_BC2_UNORM_SRGB = 75,
    DXGI_FORMAT_BC3_UNORM = 77,
    DXGI_FORMAT_BC3_UNORM_SRGB = 78,
    DXGI_FORMAT_BC4_UNORM = 80,
    DXGI_FORMAT_BC4_SNORM = 81,
    DXGI_FORMAT_BC5_UNORM = 83,
    DXGI_FORMAT_BC5_SNORM = 84,
    DXGI_FORMAT_BC6H_UF16 = 95,
    DXGI_FORMAT_BC6H_SF16 = 96,
    DXGI_FORMAT_BC7_UNORM = 98,
    DXGI_FORMAT_BC7_UNORM_SRGB = 99
};

// DDS structures
#pragma pack(push, 1)
struct DDS_PIXELFORMAT {
    uint32_t size;
    uint32_t flags;
    uint32_t fourCC;
    uint32_t RGBBitCount;
    uint32_t RBitMask;
    uint32_t GBitMask;
    uint32_t BBitMask;
    uint32_t ABitMask;
};

struct DDS_HEADER {
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth;
    uint32_t mipMapCount;
    uint32_t reserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t caps;
    uint32_t caps2;
    uint32_t caps3;
    uint32_t caps4;
    uint32_t reserved2;
};

struct DDS_HEADER_DXT10 {
    uint32_t dxgiFormat;
    uint32_t resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t miscFlags2;
};
#pragma pack(pop)

static size_t get_block_size(TextureFormat format) {
    switch (format) {
        case TextureFormat::BC1: return 8;  // 4x4 block = 8 bytes
        case TextureFormat::BC3: return 16; // 4x4 block = 16 bytes
        case TextureFormat::BC7: return 16; // 4x4 block = 16 bytes
        default: return 0;
    }
}

static size_t calculate_mip_size(uint32_t width, uint32_t height, TextureFormat format) {
    size_t block_size = get_block_size(format);
    if (block_size == 0) {
        // Uncompressed - assume RGBA8 for now
        return static_cast<size_t>(width) * height * 4;
    }

    // Compressed - calculate block count
    uint32_t blocks_x = std::max(1u, (width + 3) / 4);
    uint32_t blocks_y = std::max(1u, (height + 3) / 4);
    return static_cast<size_t>(blocks_x) * blocks_y * block_size;
}

bool DDSLoader::load(const std::string& path, DDSData& out_data) {
    s_last_error.clear();

    // Read file
    auto file_data = FileSystem::read_binary(path);
    if (file_data.empty()) {
        s_last_error = "Failed to read DDS file: " + path;
        log(LogLevel::Error, s_last_error.c_str());
        return false;
    }

    // Check minimum size
    if (file_data.size() < sizeof(uint32_t) + sizeof(DDS_HEADER)) {
        s_last_error = "Invalid DDS file: too small";
        log(LogLevel::Error, s_last_error.c_str());
        return false;
    }

    const uint8_t* ptr = file_data.data();

    // Check magic number
    uint32_t magic;
    std::memcpy(&magic, ptr, sizeof(magic));
    if (magic != DDS_MAGIC) {
        s_last_error = "Invalid DDS file: bad magic number";
        log(LogLevel::Error, s_last_error.c_str());
        return false;
    }
    ptr += sizeof(magic);

    // Read header
    DDS_HEADER header;
    std::memcpy(&header, ptr, sizeof(header));
    ptr += sizeof(header);

    if (header.size != sizeof(DDS_HEADER)) {
        s_last_error = "Invalid DDS header size";
        log(LogLevel::Error, s_last_error.c_str());
        return false;
    }

    out_data.width = header.width;
    out_data.height = header.height;
    out_data.depth = (header.flags & DDSD_DEPTH) ? header.depth : 1;
    out_data.mip_levels = (header.flags & DDSD_MIPMAPCOUNT) ? std::max(1u, header.mipMapCount) : 1;
    out_data.is_cubemap = (header.caps2 & DDSCAPS2_CUBEMAP) != 0;
    out_data.array_size = out_data.is_cubemap ? 6 : 1;

    // Determine format
    bool has_dx10_header = false;
    DDS_HEADER_DXT10 dx10_header{};

    if (header.ddspf.flags & DDPF_FOURCC) {
        switch (header.ddspf.fourCC) {
            case FOURCC_DXT1:
                out_data.format = TextureFormat::BC1;
                break;
            case FOURCC_DXT3:
                out_data.format = TextureFormat::BC3; // Using BC3 for DXT3 (similar)
                break;
            case FOURCC_DXT5:
                out_data.format = TextureFormat::BC3;
                break;
            case FOURCC_DX10:
                has_dx10_header = true;
                if (file_data.size() < sizeof(uint32_t) + sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10)) {
                    s_last_error = "Invalid DDS file: missing DX10 header";
                    log(LogLevel::Error, s_last_error.c_str());
                    return false;
                }
                std::memcpy(&dx10_header, ptr, sizeof(dx10_header));
                ptr += sizeof(dx10_header);
                break;
            default:
                s_last_error = "Unsupported DDS FourCC format";
                log(LogLevel::Error, s_last_error.c_str());
                return false;
        }
    } else if (header.ddspf.flags & DDPF_RGB) {
        // Uncompressed RGB(A)
        out_data.format = TextureFormat::RGBA8;
    } else {
        s_last_error = "Unsupported DDS pixel format";
        log(LogLevel::Error, s_last_error.c_str());
        return false;
    }

    // Handle DX10 extended header
    if (has_dx10_header) {
        out_data.array_size = dx10_header.arraySize;

        switch (dx10_header.dxgiFormat) {
            case DXGI_FORMAT_BC1_UNORM:
            case DXGI_FORMAT_BC1_UNORM_SRGB:
                out_data.format = TextureFormat::BC1;
                break;
            case DXGI_FORMAT_BC2_UNORM:
            case DXGI_FORMAT_BC2_UNORM_SRGB:
            case DXGI_FORMAT_BC3_UNORM:
            case DXGI_FORMAT_BC3_UNORM_SRGB:
                out_data.format = TextureFormat::BC3;
                break;
            case DXGI_FORMAT_BC7_UNORM:
            case DXGI_FORMAT_BC7_UNORM_SRGB:
                out_data.format = TextureFormat::BC7;
                break;
            default:
                s_last_error = "Unsupported DXGI format: " + std::to_string(dx10_header.dxgiFormat);
                log(LogLevel::Error, s_last_error.c_str());
                return false;
        }
    }

    // Calculate total data size
    size_t total_size = 0;
    for (uint32_t array_idx = 0; array_idx < out_data.array_size; array_idx++) {
        uint32_t mip_width = out_data.width;
        uint32_t mip_height = out_data.height;
        for (uint32_t mip = 0; mip < out_data.mip_levels; mip++) {
            total_size += calculate_mip_size(mip_width, mip_height, out_data.format);
            mip_width = std::max(1u, mip_width / 2);
            mip_height = std::max(1u, mip_height / 2);
        }
    }

    // Validate remaining data size
    size_t header_size = static_cast<size_t>(ptr - file_data.data());
    if (file_data.size() - header_size < total_size) {
        s_last_error = "Invalid DDS file: truncated data";
        log(LogLevel::Error, s_last_error.c_str());
        return false;
    }

    // Copy pixel data
    out_data.data.resize(total_size);
    std::memcpy(out_data.data.data(), ptr, total_size);

    log(LogLevel::Debug, ("Loaded DDS: " + path + " (" +
        std::to_string(out_data.width) + "x" + std::to_string(out_data.height) +
        ", " + std::to_string(out_data.mip_levels) + " mips)").c_str());

    return true;
}

const std::string& DDSLoader::get_last_error() {
    return s_last_error;
}

} // namespace engine::asset
