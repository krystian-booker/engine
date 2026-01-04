#include <engine/navigation/navmesh.hpp>
#include <engine/core/log.hpp>

#include <DetourNavMesh.h>
#include <DetourCommon.h>

#include <fstream>
#include <cstring>

namespace engine::navigation {

// NavMesh file header
static constexpr uint32_t NAVMESH_MAGIC = 'NAVM';
static constexpr uint32_t NAVMESH_VERSION = 1;

struct NavMeshFileHeader {
    uint32_t magic;
    uint32_t version;
    int num_tiles;
    dtNavMeshParams params;
};

// Deleter implementation
void NavMesh::NavMeshDeleter::operator()(dtNavMesh* mesh) const {
    if (mesh) {
        dtFreeNavMesh(mesh);
    }
}

NavMesh::NavMesh() = default;
NavMesh::~NavMesh() = default;

NavMesh::NavMesh(NavMesh&& other) noexcept
    : m_navmesh(std::move(other.m_navmesh))
    , m_settings(other.m_settings)
    , m_supports_tile_cache(other.m_supports_tile_cache)
    , m_tile_cache_layers(std::move(other.m_tile_cache_layers)) {
    other.m_supports_tile_cache = false;
}

NavMesh& NavMesh::operator=(NavMesh&& other) noexcept {
    if (this != &other) {
        m_navmesh = std::move(other.m_navmesh);
        m_settings = other.m_settings;
        m_supports_tile_cache = other.m_supports_tile_cache;
        m_tile_cache_layers = std::move(other.m_tile_cache_layers);
        other.m_supports_tile_cache = false;
    }
    return *this;
}

bool NavMesh::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        core::log(core::LogLevel::Error, "NavMesh: Failed to open file: {}", path);
        return false;
    }

    // Get file size
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read file into buffer
    std::vector<uint8_t> buffer(file_size);
    file.read(reinterpret_cast<char*>(buffer.data()), file_size);

    return load_from_memory(buffer.data(), buffer.size());
}

bool NavMesh::load_from_memory(const uint8_t* data, size_t size) {
    if (size < sizeof(NavMeshFileHeader)) {
        core::log(core::LogLevel::Error, "NavMesh: File too small");
        return false;
    }

    const NavMeshFileHeader* header = reinterpret_cast<const NavMeshFileHeader*>(data);

    if (header->magic != NAVMESH_MAGIC) {
        core::log(core::LogLevel::Error, "NavMesh: Invalid file magic");
        return false;
    }

    if (header->version != NAVMESH_VERSION) {
        core::log(core::LogLevel::Error, "NavMesh: Unsupported version {}", header->version);
        return false;
    }

    // Create navmesh
    dtNavMesh* navmesh = dtAllocNavMesh();
    if (!navmesh) {
        core::log(core::LogLevel::Error, "NavMesh: Failed to allocate navmesh");
        return false;
    }

    dtStatus status = navmesh->init(&header->params);
    if (dtStatusFailed(status)) {
        core::log(core::LogLevel::Error, "NavMesh: Failed to initialize navmesh");
        dtFreeNavMesh(navmesh);
        return false;
    }

    // Read tiles
    const uint8_t* tile_data = data + sizeof(NavMeshFileHeader);
    size_t remaining = size - sizeof(NavMeshFileHeader);

    for (int i = 0; i < header->num_tiles; ++i) {
        if (remaining < sizeof(dtMeshHeader)) {
            core::log(core::LogLevel::Error, "NavMesh: Unexpected end of file");
            dtFreeNavMesh(navmesh);
            return false;
        }

        // Read tile header to get data size
        struct TileHeader {
            dtTileRef tile_ref;
            int data_size;
        };

        const TileHeader* tile_header = reinterpret_cast<const TileHeader*>(tile_data);
        tile_data += sizeof(TileHeader);
        remaining -= sizeof(TileHeader);

        if (remaining < static_cast<size_t>(tile_header->data_size)) {
            core::log(core::LogLevel::Error, "NavMesh: Unexpected end of file in tile data");
            dtFreeNavMesh(navmesh);
            return false;
        }

        // Allocate and copy tile data (Detour will own this memory)
        uint8_t* nav_data = static_cast<uint8_t*>(dtAlloc(tile_header->data_size, DT_ALLOC_PERM));
        if (!nav_data) {
            core::log(core::LogLevel::Error, "NavMesh: Failed to allocate tile data");
            dtFreeNavMesh(navmesh);
            return false;
        }
        std::memcpy(nav_data, tile_data, tile_header->data_size);

        // Add tile
        status = navmesh->addTile(nav_data, tile_header->data_size, DT_TILE_FREE_DATA, 0, nullptr);
        if (dtStatusFailed(status)) {
            dtFree(nav_data);
            core::log(core::LogLevel::Error, "NavMesh: Failed to add tile");
            dtFreeNavMesh(navmesh);
            return false;
        }

        tile_data += tile_header->data_size;
        remaining -= tile_header->data_size;
    }

    m_navmesh.reset(navmesh);
    core::log(core::LogLevel::Info, "NavMesh: Loaded {} tiles", header->num_tiles);
    return true;
}

bool NavMesh::save(const std::string& path) const {
    if (!m_navmesh) {
        core::log(core::LogLevel::Error, "NavMesh: Cannot save - no navmesh");
        return false;
    }

    std::vector<uint8_t> data = get_binary_data();
    if (data.empty()) {
        return false;
    }

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        core::log(core::LogLevel::Error, "NavMesh: Failed to create file: {}", path);
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    core::log(core::LogLevel::Info, "NavMesh: Saved to {}", path);
    return true;
}

std::vector<uint8_t> NavMesh::get_binary_data() const {
    if (!m_navmesh) {
        return {};
    }

    std::vector<uint8_t> data;

    // Reserve space for header
    data.resize(sizeof(NavMeshFileHeader));

    const dtNavMesh* mesh = m_navmesh.get();

    // Count tiles and serialize
    int num_tiles = 0;
    for (int i = 0; i < mesh->getMaxTiles(); ++i) {
        const dtMeshTile* tile = mesh->getTile(i);
        if (!tile || !tile->header || !tile->dataSize) continue;

        num_tiles++;

        // Write tile header
        struct TileHeader {
            dtTileRef tile_ref;
            int data_size;
        };

        TileHeader tile_header;
        tile_header.tile_ref = mesh->getTileRef(tile);
        tile_header.data_size = tile->dataSize;

        size_t offset = data.size();
        data.resize(offset + sizeof(TileHeader) + tile->dataSize);
        std::memcpy(data.data() + offset, &tile_header, sizeof(TileHeader));
        std::memcpy(data.data() + offset + sizeof(TileHeader), tile->data, tile->dataSize);
    }

    // Write header
    NavMeshFileHeader* header = reinterpret_cast<NavMeshFileHeader*>(data.data());
    header->magic = NAVMESH_MAGIC;
    header->version = NAVMESH_VERSION;
    header->num_tiles = num_tiles;
    header->params = *mesh->getParams();

    return data;
}

int NavMesh::get_tile_count() const {
    if (!m_navmesh) return 0;

    const dtNavMesh* mesh = m_navmesh.get();
    int count = 0;
    for (int i = 0; i < mesh->getMaxTiles(); ++i) {
        const dtMeshTile* tile = mesh->getTile(i);
        if (tile && tile->header) count++;
    }
    return count;
}

int NavMesh::get_polygon_count() const {
    if (!m_navmesh) return 0;

    const dtNavMesh* mesh = m_navmesh.get();
    int count = 0;
    for (int i = 0; i < mesh->getMaxTiles(); ++i) {
        const dtMeshTile* tile = mesh->getTile(i);
        if (tile && tile->header) {
            count += tile->header->polyCount;
        }
    }
    return count;
}

int NavMesh::get_vertex_count() const {
    if (!m_navmesh) return 0;

    const dtNavMesh* mesh = m_navmesh.get();
    int count = 0;
    for (int i = 0; i < mesh->getMaxTiles(); ++i) {
        const dtMeshTile* tile = mesh->getTile(i);
        if (tile && tile->header) {
            count += tile->header->vertCount;
        }
    }
    return count;
}

AABB NavMesh::get_bounds() const {
    AABB bounds;
    if (!m_navmesh) return bounds;

    const dtNavMesh* mesh = m_navmesh.get();
    bool first = true;
    for (int i = 0; i < mesh->getMaxTiles(); ++i) {
        const dtMeshTile* tile = mesh->getTile(i);
        if (!tile || !tile->header) continue;

        Vec3 tile_min(tile->header->bmin[0], tile->header->bmin[1], tile->header->bmin[2]);
        Vec3 tile_max(tile->header->bmax[0], tile->header->bmax[1], tile->header->bmax[2]);

        if (first) {
            bounds.min = tile_min;
            bounds.max = tile_max;
            first = false;
        } else {
            bounds.min = glm::min(bounds.min, tile_min);
            bounds.max = glm::max(bounds.max, tile_max);
        }
    }

    return bounds;
}

std::vector<NavMesh::DebugVertex> NavMesh::get_debug_geometry() const {
    std::vector<DebugVertex> vertices;
    if (!m_navmesh) return vertices;

    const dtNavMesh* mesh = m_navmesh.get();

    for (int i = 0; i < mesh->getMaxTiles(); ++i) {
        const dtMeshTile* tile = mesh->getTile(i);
        if (!tile || !tile->header) continue;

        for (int j = 0; j < tile->header->polyCount; ++j) {
            const dtPoly* poly = &tile->polys[j];

            // Skip off-mesh connections
            if (poly->getType() == DT_POLYTYPE_OFFMESH_CONNECTION) continue;

            // Triangulate polygon
            const unsigned int nv = poly->vertCount;
            for (unsigned int k = 2; k < nv; ++k) {
                // Get vertex indices
                unsigned int vi0 = poly->verts[0];
                unsigned int vi1 = poly->verts[k - 1];
                unsigned int vi2 = poly->verts[k];

                // Get vertex positions
                const float* v0 = &tile->verts[vi0 * 3];
                const float* v1 = &tile->verts[vi1 * 3];
                const float* v2 = &tile->verts[vi2 * 3];

                // Determine color based on area type
                Vec4 color;
                switch (poly->getArea()) {
                    case 0: color = Vec4(0.2f, 0.6f, 0.2f, 0.5f); break;  // Walkable (green)
                    case 1: color = Vec4(0.2f, 0.2f, 0.6f, 0.5f); break;  // Water (blue)
                    case 2: color = Vec4(0.4f, 0.6f, 0.2f, 0.5f); break;  // Grass (yellow-green)
                    case 3: color = Vec4(0.5f, 0.5f, 0.5f, 0.5f); break;  // Road (gray)
                    default: color = Vec4(0.6f, 0.2f, 0.2f, 0.5f); break; // Other (red)
                }

                // Add triangle vertices
                vertices.push_back({Vec3(v0[0], v0[1], v0[2]), color});
                vertices.push_back({Vec3(v1[0], v1[1], v1[2]), color});
                vertices.push_back({Vec3(v2[0], v2[1], v2[2]), color});
            }
        }
    }

    return vertices;
}

void NavMesh::set_tile_cache_layers(std::vector<std::vector<uint8_t>> layers) {
    m_tile_cache_layers = std::move(layers);
    m_supports_tile_cache = !m_tile_cache_layers.empty();
}

} // namespace engine::navigation
