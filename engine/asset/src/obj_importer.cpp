#include <engine/asset/obj_importer.hpp>
#include <engine/core/log.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <unordered_map>
#include <limits>

namespace engine::asset {

using namespace engine::core;
using namespace engine::render;

std::string ObjImporter::s_last_error;

// Hash function for vertex deduplication
struct VertexHash {
    size_t operator()(const std::tuple<int, int, int>& v) const {
        size_t h1 = std::hash<int>{}(std::get<0>(v));
        size_t h2 = std::hash<int>{}(std::get<1>(v));
        size_t h3 = std::hash<int>{}(std::get<2>(v));
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

static void compute_tangent(
    const Vec3& p0, const Vec3& p1, const Vec3& p2,
    const Vec2& uv0, const Vec2& uv1, const Vec2& uv2,
    Vec3& tangent)
{
    Vec3 edge1 = p1 - p0;
    Vec3 edge2 = p2 - p0;
    Vec2 deltaUV1 = uv1 - uv0;
    Vec2 deltaUV2 = uv2 - uv0;

    float f = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
    if (std::abs(f) < 1e-6f) {
        tangent = Vec3{1.0f, 0.0f, 0.0f};
        return;
    }

    f = 1.0f / f;
    tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
    tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
    tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

    float len = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
    if (len > 1e-6f) {
        tangent.x /= len;
        tangent.y /= len;
        tangent.z /= len;
    } else {
        tangent = Vec3{1.0f, 0.0f, 0.0f};
    }
}

std::shared_ptr<MeshAsset> ObjImporter::import_mesh(
    const std::string& path,
    render::IRenderer* renderer)
{
    std::vector<ObjMaterial> materials;
    return import_mesh_with_materials(path, renderer, materials);
}

std::shared_ptr<MeshAsset> ObjImporter::import_mesh_with_materials(
    const std::string& path,
    render::IRenderer* renderer,
    std::vector<ObjMaterial>& out_materials)
{
    s_last_error.clear();

    if (!renderer) {
        s_last_error = "Renderer not initialized";
        return nullptr;
    }

    tinyobj::ObjReaderConfig reader_config;
    reader_config.triangulate = true;
    reader_config.vertex_color = true;

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(path, reader_config)) {
        if (!reader.Error().empty()) {
            s_last_error = "TinyObjReader error: " + reader.Error();
        } else {
            s_last_error = "Failed to parse OBJ file: " + path;
        }
        log(LogLevel::Error, s_last_error.c_str());
        return nullptr;
    }

    if (!reader.Warning().empty()) {
        log(LogLevel::Warn, ("TinyObjReader warning: " + reader.Warning()).c_str());
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();
    const auto& materials = reader.GetMaterials();

    // Convert materials
    out_materials.clear();
    out_materials.reserve(materials.size());
    for (const auto& mat : materials) {
        ObjMaterial obj_mat;
        obj_mat.name = mat.name;
        obj_mat.diffuse_texture = mat.diffuse_texname;
        obj_mat.normal_texture = mat.bump_texname.empty() ? mat.normal_texname : mat.bump_texname;
        obj_mat.specular_texture = mat.specular_texname;
        obj_mat.ambient = Vec3{mat.ambient[0], mat.ambient[1], mat.ambient[2]};
        obj_mat.diffuse = Vec3{mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]};
        obj_mat.specular = Vec3{mat.specular[0], mat.specular[1], mat.specular[2]};
        obj_mat.shininess = mat.shininess;
        obj_mat.opacity = mat.dissolve;
        out_materials.push_back(obj_mat);
    }

    // Build mesh data with vertex deduplication
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::unordered_map<std::tuple<int, int, int>, uint32_t, VertexHash> vertex_map;

    Vec3 bounds_min{std::numeric_limits<float>::max()};
    Vec3 bounds_max{std::numeric_limits<float>::lowest()};

    for (const auto& shape : shapes) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            size_t fv = shape.mesh.num_face_vertices[f];

            // Should be triangulated, so fv should be 3
            for (size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

                auto key = std::make_tuple(idx.vertex_index, idx.normal_index, idx.texcoord_index);
                auto it = vertex_map.find(key);

                if (it != vertex_map.end()) {
                    indices.push_back(it->second);
                } else {
                    Vertex vert{};

                    // Position
                    vert.position.x = attrib.vertices[3 * idx.vertex_index + 0];
                    vert.position.y = attrib.vertices[3 * idx.vertex_index + 1];
                    vert.position.z = attrib.vertices[3 * idx.vertex_index + 2];

                    // Update bounds
                    bounds_min.x = std::min(bounds_min.x, vert.position.x);
                    bounds_min.y = std::min(bounds_min.y, vert.position.y);
                    bounds_min.z = std::min(bounds_min.z, vert.position.z);
                    bounds_max.x = std::max(bounds_max.x, vert.position.x);
                    bounds_max.y = std::max(bounds_max.y, vert.position.y);
                    bounds_max.z = std::max(bounds_max.z, vert.position.z);

                    // Normal
                    if (idx.normal_index >= 0 && !attrib.normals.empty()) {
                        size_t ni = static_cast<size_t>(idx.normal_index);
                        if (ni * 3 + 2 < attrib.normals.size()) {
                            vert.normal.x = attrib.normals[3 * ni + 0];
                            vert.normal.y = attrib.normals[3 * ni + 1];
                            vert.normal.z = attrib.normals[3 * ni + 2];
                        } else {
                            vert.normal = Vec3{0.0f, 1.0f, 0.0f};
                        }
                    } else {
                        vert.normal = Vec3{0.0f, 1.0f, 0.0f};
                    }

                    // Texcoord
                    if (idx.texcoord_index >= 0 && !attrib.texcoords.empty()) {
                        size_t ti = static_cast<size_t>(idx.texcoord_index);
                        if (ti * 2 + 1 < attrib.texcoords.size()) {
                            vert.texcoord.x = attrib.texcoords[2 * ti + 0];
                            vert.texcoord.y = 1.0f - attrib.texcoords[2 * ti + 1]; // Flip Y
                        } else {
                            vert.texcoord = Vec2{0.0f, 0.0f};
                        }
                    } else {
                        vert.texcoord = Vec2{0.0f, 0.0f};
                    }

                    // Vertex color (if available)
                    if (!attrib.colors.empty() && idx.vertex_index >= 0) {
                        size_t color_idx = static_cast<size_t>(idx.vertex_index);
                        if (color_idx * 3 + 2 < attrib.colors.size()) {
                            vert.color.x = attrib.colors[3 * color_idx + 0];
                            vert.color.y = attrib.colors[3 * color_idx + 1];
                            vert.color.z = attrib.colors[3 * color_idx + 2];
                            vert.color.w = 1.0f;
                        }
                    }

                    uint32_t new_index = static_cast<uint32_t>(vertices.size());
                    vertex_map[key] = new_index;
                    vertices.push_back(vert);
                    indices.push_back(new_index);
                }
            }
            index_offset += fv;
        }
    }

    if (vertices.empty()) {
        s_last_error = "No vertices found in OBJ file";
        return nullptr;
    }

    // Compute tangents
    std::vector<Vec3> tangents(vertices.size(), Vec3{0.0f});
    std::vector<int> tangent_counts(vertices.size(), 0);

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        Vec3 tangent;
        compute_tangent(
            vertices[i0].position, vertices[i1].position, vertices[i2].position,
            vertices[i0].texcoord, vertices[i1].texcoord, vertices[i2].texcoord,
            tangent);

        tangents[i0] = tangents[i0] + tangent;
        tangents[i1] = tangents[i1] + tangent;
        tangents[i2] = tangents[i2] + tangent;
        tangent_counts[i0]++;
        tangent_counts[i1]++;
        tangent_counts[i2]++;
    }

    // Average and normalize tangents
    for (size_t i = 0; i < vertices.size(); i++) {
        if (tangent_counts[i] > 0) {
            Vec3& t = tangents[i];
            float len = std::sqrt(t.x * t.x + t.y * t.y + t.z * t.z);
            if (len > 1e-6f) {
                vertices[i].tangent = Vec3{t.x / len, t.y / len, t.z / len};
            } else {
                vertices[i].tangent = Vec3{1.0f, 0.0f, 0.0f};
            }
        }
    }

    // Create mesh data
    MeshData mesh_data;
    mesh_data.vertices = std::move(vertices);
    mesh_data.indices = std::move(indices);
    mesh_data.bounds.min = bounds_min;
    mesh_data.bounds.max = bounds_max;

    // Create GPU mesh
    MeshHandle handle = renderer->create_mesh(mesh_data);
    if (!handle.valid()) {
        s_last_error = "Failed to create GPU mesh";
        return nullptr;
    }

    auto asset = std::make_shared<MeshAsset>();
    asset->path = path;
    asset->handle = handle;
    asset->bounds = mesh_data.bounds;
    asset->vertex_count = static_cast<uint32_t>(mesh_data.vertices.size());
    asset->index_count = static_cast<uint32_t>(mesh_data.indices.size());

    log(LogLevel::Debug, ("Loaded OBJ: " + path +
        " (verts: " + std::to_string(asset->vertex_count) +
        ", indices: " + std::to_string(asset->index_count) + ")").c_str());

    return asset;
}

const std::string& ObjImporter::get_last_error() {
    return s_last_error;
}

} // namespace engine::asset
