#include <engine/asset/gltf_importer.hpp>
#include <engine/core/log.hpp>
#include <engine/core/filesystem.hpp>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <cstring>
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace engine::asset {

using namespace engine::core;
using namespace engine::render;

std::string GltfImporter::s_last_error;

namespace {

// Helper to read accessor data
template<typename T>
void read_accessor(const cgltf_accessor* accessor, std::vector<T>& out) {
    if (!accessor) return;

    out.resize(accessor->count);
    for (size_t i = 0; i < accessor->count; ++i) {
        cgltf_accessor_read_float(accessor, i, reinterpret_cast<float*>(&out[i]), sizeof(T) / sizeof(float));
    }
}

// Read scalar accessor
void read_accessor_scalar(const cgltf_accessor* accessor, std::vector<float>& out) {
    if (!accessor) return;

    out.resize(accessor->count);
    for (size_t i = 0; i < accessor->count; ++i) {
        cgltf_accessor_read_float(accessor, i, &out[i], 1);
    }
}

// Read indices from accessor
void read_indices(const cgltf_accessor* accessor, std::vector<uint32_t>& out) {
    if (!accessor) return;

    out.resize(accessor->count);
    for (size_t i = 0; i < accessor->count; ++i) {
        out[i] = static_cast<uint32_t>(cgltf_accessor_read_index(accessor, i));
    }
}

// Read Vec2 from accessor
void read_vec2_accessor(const cgltf_accessor* accessor, std::vector<Vec2>& out) {
    if (!accessor) return;

    out.resize(accessor->count);
    for (size_t i = 0; i < accessor->count; ++i) {
        float v[2] = {0.0f, 0.0f};
        cgltf_accessor_read_float(accessor, i, v, 2);
        out[i] = Vec2(v[0], v[1]);
    }
}

// Read Vec3 from accessor
void read_vec3_accessor(const cgltf_accessor* accessor, std::vector<Vec3>& out) {
    if (!accessor) return;

    out.resize(accessor->count);
    for (size_t i = 0; i < accessor->count; ++i) {
        float v[3] = {0.0f, 0.0f, 0.0f};
        cgltf_accessor_read_float(accessor, i, v, 3);
        out[i] = Vec3(v[0], v[1], v[2]);
    }
}

// Read Vec4 from accessor
void read_vec4_accessor(const cgltf_accessor* accessor, std::vector<Vec4>& out) {
    if (!accessor) return;

    out.resize(accessor->count);
    for (size_t i = 0; i < accessor->count; ++i) {
        float v[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        cgltf_accessor_read_float(accessor, i, v, 4);
        out[i] = Vec4(v[0], v[1], v[2], v[3]);
    }
}

// Read joint indices (stored as u8 or u16)
void read_joint_indices(const cgltf_accessor* accessor, std::vector<IVec4>& out) {
    if (!accessor) return;

    out.resize(accessor->count);
    for (size_t i = 0; i < accessor->count; ++i) {
        cgltf_uint indices[4] = {0, 0, 0, 0};
        cgltf_accessor_read_uint(accessor, i, indices, 4);
        out[i] = IVec4(
            static_cast<int>(indices[0]),
            static_cast<int>(indices[1]),
            static_cast<int>(indices[2]),
            static_cast<int>(indices[3])
        );
    }
}

// Calculate tangents using MikkTSpace-like approach (simplified)
void calculate_tangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    // Reset tangents
    for (auto& v : vertices) {
        v.tangent = Vec3(0.0f);
    }

    // Calculate tangent for each triangle
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t i0 = indices[i + 0];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        const Vec3& p0 = vertices[i0].position;
        const Vec3& p1 = vertices[i1].position;
        const Vec3& p2 = vertices[i2].position;

        const Vec2& uv0 = vertices[i0].texcoord;
        const Vec2& uv1 = vertices[i1].texcoord;
        const Vec2& uv2 = vertices[i2].texcoord;

        Vec3 edge1 = p1 - p0;
        Vec3 edge2 = p2 - p0;

        Vec2 duv1 = uv1 - uv0;
        Vec2 duv2 = uv2 - uv0;

        float f = duv1.x * duv2.y - duv2.x * duv1.y;
        if (std::abs(f) < 1e-6f) continue;

        f = 1.0f / f;

        Vec3 tangent;
        tangent.x = f * (duv2.y * edge1.x - duv1.y * edge2.x);
        tangent.y = f * (duv2.y * edge1.y - duv1.y * edge2.y);
        tangent.z = f * (duv2.y * edge1.z - duv1.y * edge2.z);

        vertices[i0].tangent += tangent;
        vertices[i1].tangent += tangent;
        vertices[i2].tangent += tangent;
    }

    // Normalize tangents and orthogonalize
    for (auto& v : vertices) {
        float len = glm::length(v.tangent);
        if (len > 1e-6f) {
            v.tangent /= len;
            // Gram-Schmidt orthogonalize
            v.tangent = glm::normalize(v.tangent - v.normal * glm::dot(v.normal, v.tangent));
        } else {
            // Default tangent
            v.tangent = Vec3(1.0f, 0.0f, 0.0f);
        }
    }
}

// Calculate AABB from vertices
AABB calculate_bounds(const std::vector<Vertex>& vertices) {
    if (vertices.empty()) {
        return AABB{Vec3(0.0f), Vec3(0.0f)};
    }

    AABB bounds;
    bounds.min = vertices[0].position;
    bounds.max = vertices[0].position;

    for (const auto& v : vertices) {
        bounds.expand(v.position);
    }

    return bounds;
}

// Process a single primitive (submesh)
ImportedMesh process_primitive(const cgltf_primitive* primitive, const cgltf_data* data) {
    ImportedMesh result;

    // Get vertex count from position accessor
    const cgltf_accessor* position_accessor = nullptr;
    const cgltf_accessor* normal_accessor = nullptr;
    const cgltf_accessor* texcoord_accessor = nullptr;
    const cgltf_accessor* color_accessor = nullptr;
    const cgltf_accessor* tangent_accessor = nullptr;
    const cgltf_accessor* joints_accessor = nullptr;
    const cgltf_accessor* weights_accessor = nullptr;

    // Find accessors for each attribute
    for (size_t i = 0; i < primitive->attributes_count; ++i) {
        const cgltf_attribute& attr = primitive->attributes[i];
        switch (attr.type) {
            case cgltf_attribute_type_position:
                position_accessor = attr.data;
                break;
            case cgltf_attribute_type_normal:
                normal_accessor = attr.data;
                break;
            case cgltf_attribute_type_texcoord:
                if (attr.index == 0) texcoord_accessor = attr.data;
                break;
            case cgltf_attribute_type_color:
                if (attr.index == 0) color_accessor = attr.data;
                break;
            case cgltf_attribute_type_tangent:
                tangent_accessor = attr.data;
                break;
            case cgltf_attribute_type_joints:
                if (attr.index == 0) joints_accessor = attr.data;
                break;
            case cgltf_attribute_type_weights:
                if (attr.index == 0) weights_accessor = attr.data;
                break;
            default:
                break;
        }
    }

    if (!position_accessor) {
        return result;
    }

    size_t vertex_count = position_accessor->count;

    // Read all vertex attributes
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> texcoords;
    std::vector<Vec4> colors;
    std::vector<Vec4> tangents;

    read_vec3_accessor(position_accessor, positions);

    if (normal_accessor) {
        read_vec3_accessor(normal_accessor, normals);
    } else {
        normals.resize(vertex_count, Vec3(0.0f, 1.0f, 0.0f));
    }

    if (texcoord_accessor) {
        read_vec2_accessor(texcoord_accessor, texcoords);
    } else {
        texcoords.resize(vertex_count, Vec2(0.0f));
    }

    if (color_accessor) {
        read_vec4_accessor(color_accessor, colors);
    } else {
        colors.resize(vertex_count, Vec4(1.0f));
    }

    if (tangent_accessor) {
        read_vec4_accessor(tangent_accessor, tangents);
    }

    // Read skinning data if present
    if (joints_accessor && weights_accessor) {
        result.has_skinning = true;
        read_joint_indices(joints_accessor, result.bone_indices);
        read_vec4_accessor(weights_accessor, result.bone_weights);
    }

    // Build vertex array
    result.mesh_data.vertices.resize(vertex_count);
    for (size_t i = 0; i < vertex_count; ++i) {
        Vertex& v = result.mesh_data.vertices[i];
        v.position = positions[i];
        v.normal = normals[i];
        v.texcoord = texcoords[i];
        v.color = colors[i];
        if (!tangents.empty()) {
            v.tangent = Vec3(tangents[i].x, tangents[i].y, tangents[i].z);
        }
    }

    // Read indices
    if (primitive->indices) {
        read_indices(primitive->indices, result.mesh_data.indices);
    } else {
        // Generate indices for non-indexed geometry
        result.mesh_data.indices.resize(vertex_count);
        for (size_t i = 0; i < vertex_count; ++i) {
            result.mesh_data.indices[i] = static_cast<uint32_t>(i);
        }
    }

    // Calculate tangents if not provided
    if (tangents.empty() && !result.mesh_data.vertices.empty()) {
        calculate_tangents(result.mesh_data.vertices, result.mesh_data.indices);
    }

    // Calculate bounds
    result.mesh_data.bounds = calculate_bounds(result.mesh_data.vertices);

    // Get material index
    if (primitive->material) {
        result.material_index = static_cast<int32_t>(primitive->material - data->materials);
    }

    return result;
}

// Process a mesh node (may contain multiple primitives)
void process_mesh(const cgltf_mesh* mesh, const cgltf_data* data, ImportedModel& model) {
    for (size_t i = 0; i < mesh->primitives_count; ++i) {
        ImportedMesh imported = process_primitive(&mesh->primitives[i], data);
        if (!imported.mesh_data.vertices.empty()) {
            imported.name = mesh->name ? mesh->name : "";
            if (mesh->primitives_count > 1) {
                imported.name += "_" + std::to_string(i);
            }
            model.meshes.push_back(std::move(imported));
        }
    }
}

// Process skeleton/skin data
SkeletonData process_skin(const cgltf_skin* skin, const cgltf_data* data) {
    SkeletonData skeleton;

    skeleton.joints.resize(skin->joints_count);
    skeleton.joint_indices.resize(skin->joints_count);

    // Read inverse bind matrices
    std::vector<Mat4> inverse_bind_matrices;
    if (skin->inverse_bind_matrices) {
        inverse_bind_matrices.resize(skin->joints_count);
        for (size_t i = 0; i < skin->joints_count; ++i) {
            float m[16];
            cgltf_accessor_read_float(skin->inverse_bind_matrices, i, m, 16);
            inverse_bind_matrices[i] = glm::make_mat4(m);
        }
    }

    // Process each joint
    for (size_t i = 0; i < skin->joints_count; ++i) {
        const cgltf_node* joint_node = skin->joints[i];
        SkeletonData::Joint& joint = skeleton.joints[i];

        joint.name = joint_node->name ? joint_node->name : "";

        // Find parent index
        joint.parent_index = -1;
        if (joint_node->parent) {
            for (size_t j = 0; j < skin->joints_count; ++j) {
                if (skin->joints[j] == joint_node->parent) {
                    joint.parent_index = static_cast<int32_t>(j);
                    break;
                }
            }
        }

        // Set inverse bind matrix
        if (i < inverse_bind_matrices.size()) {
            joint.inverse_bind_matrix = inverse_bind_matrices[i];
        } else {
            joint.inverse_bind_matrix = Mat4(1.0f);
        }

        // Get local transform
        if (joint_node->has_matrix) {
            joint.local_transform = glm::make_mat4(joint_node->matrix);
        } else {
            Mat4 T = glm::translate(Mat4(1.0f),
                Vec3(joint_node->translation[0], joint_node->translation[1], joint_node->translation[2]));
            Mat4 R = glm::mat4_cast(Quat(
                joint_node->rotation[3],
                joint_node->rotation[0],
                joint_node->rotation[1],
                joint_node->rotation[2]));
            Mat4 S = glm::scale(Mat4(1.0f),
                Vec3(joint_node->scale[0], joint_node->scale[1], joint_node->scale[2]));
            joint.local_transform = T * R * S;
        }

        skeleton.joint_indices[i] = static_cast<int32_t>(i);
    }

    return skeleton;
}

// Process animation
AnimationData process_animation(const cgltf_animation* animation, const cgltf_skin* skin) {
    AnimationData anim;
    anim.name = animation->name ? animation->name : "";
    anim.duration = 0.0f;

    for (size_t i = 0; i < animation->channels_count; ++i) {
        const cgltf_animation_channel* channel = &animation->channels[i];
        const cgltf_animation_sampler* sampler = channel->sampler;

        if (!channel->target_node || !sampler->input || !sampler->output) {
            continue;
        }

        // Find joint index
        int32_t joint_index = -1;
        if (skin) {
            for (size_t j = 0; j < skin->joints_count; ++j) {
                if (skin->joints[j] == channel->target_node) {
                    joint_index = static_cast<int32_t>(j);
                    break;
                }
            }
        }

        if (joint_index < 0) continue;

        AnimationData::Channel anim_channel;
        anim_channel.target_joint = joint_index;

        // Read timestamps
        read_accessor_scalar(sampler->input, anim_channel.times);

        // Update animation duration
        if (!anim_channel.times.empty()) {
            anim.duration = std::max(anim.duration, anim_channel.times.back());
        }

        // Read values based on path type
        switch (channel->target_path) {
            case cgltf_animation_path_type_translation:
                anim_channel.path = "translation";
                {
                    std::vector<Vec3> values;
                    read_vec3_accessor(sampler->output, values);
                    anim_channel.values.resize(values.size() * 3);
                    for (size_t j = 0; j < values.size(); ++j) {
                        anim_channel.values[j * 3 + 0] = values[j].x;
                        anim_channel.values[j * 3 + 1] = values[j].y;
                        anim_channel.values[j * 3 + 2] = values[j].z;
                    }
                }
                break;

            case cgltf_animation_path_type_rotation:
                anim_channel.path = "rotation";
                {
                    std::vector<Vec4> values;
                    read_vec4_accessor(sampler->output, values);
                    anim_channel.values.resize(values.size() * 4);
                    for (size_t j = 0; j < values.size(); ++j) {
                        anim_channel.values[j * 4 + 0] = values[j].x;
                        anim_channel.values[j * 4 + 1] = values[j].y;
                        anim_channel.values[j * 4 + 2] = values[j].z;
                        anim_channel.values[j * 4 + 3] = values[j].w;
                    }
                }
                break;

            case cgltf_animation_path_type_scale:
                anim_channel.path = "scale";
                {
                    std::vector<Vec3> values;
                    read_vec3_accessor(sampler->output, values);
                    anim_channel.values.resize(values.size() * 3);
                    for (size_t j = 0; j < values.size(); ++j) {
                        anim_channel.values[j * 3 + 0] = values[j].x;
                        anim_channel.values[j * 3 + 1] = values[j].y;
                        anim_channel.values[j * 3 + 2] = values[j].z;
                    }
                }
                break;

            default:
                continue;
        }

        anim.channels.push_back(std::move(anim_channel));
    }

    return anim;
}

} // anonymous namespace

std::shared_ptr<MeshAsset> GltfImporter::import_mesh(
    const std::string& path,
    render::IRenderer* renderer
) {
    if (!renderer) {
        s_last_error = "Renderer is null";
        return nullptr;
    }

    auto model = import_model(path);
    if (!model || model->meshes.empty()) {
        return nullptr;
    }

    // For simple mesh loading, we combine all meshes into one
    // (For more complex cases, use import_model directly)
    MeshData combined;
    uint32_t vertex_offset = 0;

    for (const auto& mesh : model->meshes) {
        // Add vertices
        for (const auto& v : mesh.mesh_data.vertices) {
            combined.vertices.push_back(v);
        }

        // Add indices with offset
        for (uint32_t idx : mesh.mesh_data.indices) {
            combined.indices.push_back(idx + vertex_offset);
        }

        // Expand bounds
        if (vertex_offset == 0) {
            combined.bounds = mesh.mesh_data.bounds;
        } else {
            combined.bounds.expand(mesh.mesh_data.bounds.min);
            combined.bounds.expand(mesh.mesh_data.bounds.max);
        }

        vertex_offset = static_cast<uint32_t>(combined.vertices.size());
    }

    // Create GPU mesh
    MeshHandle handle = renderer->create_mesh(combined);
    if (!handle.valid()) {
        s_last_error = "Failed to create GPU mesh";
        return nullptr;
    }

    auto asset = std::make_shared<MeshAsset>();
    asset->path = path;
    asset->handle = handle;
    asset->bounds = combined.bounds;
    asset->vertex_count = static_cast<uint32_t>(combined.vertices.size());
    asset->index_count = static_cast<uint32_t>(combined.indices.size());

    return asset;
}

std::unique_ptr<ImportedModel> GltfImporter::import_model(const std::string& path) {
    // Parse options
    cgltf_options options = {};
    cgltf_data* data = nullptr;

    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);
    if (result != cgltf_result_success) {
        s_last_error = "Failed to parse glTF file: " + path;
        log(LogLevel::Error, s_last_error.c_str());
        return nullptr;
    }

    // Load buffers (handles both .gltf with external bins and .glb)
    result = cgltf_load_buffers(&options, data, path.c_str());
    if (result != cgltf_result_success) {
        s_last_error = "Failed to load glTF buffers: " + path;
        log(LogLevel::Error, s_last_error.c_str());
        cgltf_free(data);
        return nullptr;
    }

    // Validate
    result = cgltf_validate(data);
    if (result != cgltf_result_success) {
        s_last_error = "glTF validation failed: " + path;
        log(LogLevel::Warn, s_last_error.c_str());
        // Continue anyway, some files may have minor validation issues
    }

    auto model = std::make_unique<ImportedModel>();

    // Process all meshes
    for (size_t i = 0; i < data->meshes_count; ++i) {
        process_mesh(&data->meshes[i], data, *model);
    }

    // Process skins (skeletons)
    for (size_t i = 0; i < data->skins_count; ++i) {
        model->skeletons.push_back(process_skin(&data->skins[i], data));
    }

    // Process animations
    for (size_t i = 0; i < data->animations_count; ++i) {
        const cgltf_skin* skin = data->skins_count > 0 ? &data->skins[0] : nullptr;
        model->animations.push_back(process_animation(&data->animations[i], skin));
    }

    cgltf_free(data);

    log(LogLevel::Info, ("Loaded glTF: " + path +
        " (meshes: " + std::to_string(model->meshes.size()) +
        ", skeletons: " + std::to_string(model->skeletons.size()) +
        ", animations: " + std::to_string(model->animations.size()) + ")").c_str());

    return model;
}

const std::string& GltfImporter::get_last_error() {
    return s_last_error;
}

} // namespace engine::asset
