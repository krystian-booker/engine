#include <engine/asset/fbx_importer.hpp>
#include <engine/core/log.hpp>

// ufbx is a single-header library
#define UFBX_REAL_IS_FLOAT
#include <ufbx.h>

#include <unordered_map>
#include <limits>
#include <cmath>

namespace engine::asset {

using namespace engine::core;
using namespace engine::render;

std::string FbxImporter::s_last_error;

static Vec3 to_vec3(const ufbx_vec3& v) {
    return Vec3{static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
}

static Vec2 to_vec2(const ufbx_vec2& v) {
    return Vec2{static_cast<float>(v.x), static_cast<float>(v.y)};
}

static Vec4 to_vec4(const ufbx_vec4& v) {
    return Vec4{static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z), static_cast<float>(v.w)};
}

static Mat4 to_mat4(const ufbx_matrix& m) {
    Mat4 result;
    // ufbx uses row-major, GLM uses column-major
    result[0] = Vec4{static_cast<float>(m.m00), static_cast<float>(m.m10), static_cast<float>(m.m20), 0.0f};
    result[1] = Vec4{static_cast<float>(m.m01), static_cast<float>(m.m11), static_cast<float>(m.m21), 0.0f};
    result[2] = Vec4{static_cast<float>(m.m02), static_cast<float>(m.m12), static_cast<float>(m.m22), 0.0f};
    result[3] = Vec4{static_cast<float>(m.m03), static_cast<float>(m.m13), static_cast<float>(m.m23), 1.0f};
    return result;
}

static Mat4 to_mat4(const ufbx_transform& t) {
    Mat4 translate = glm::translate(Mat4(1.0f), Vec3{
        static_cast<float>(t.translation.x),
        static_cast<float>(t.translation.y),
        static_cast<float>(t.translation.z)
    });

    Quat rot{
        static_cast<float>(t.rotation.x),
        static_cast<float>(t.rotation.y),
        static_cast<float>(t.rotation.z),
        static_cast<float>(t.rotation.w)
    };

    Mat4 rotation = glm::mat4_cast(rot);
    Mat4 scale = glm::scale(Mat4(1.0f), Vec3{
        static_cast<float>(t.scale.x),
        static_cast<float>(t.scale.y),
        static_cast<float>(t.scale.z)
    });

    return translate * rotation * scale;
}

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

std::shared_ptr<MeshAsset> FbxImporter::import_mesh(
    const std::string& path,
    render::IRenderer* renderer)
{
    s_last_error.clear();

    if (!renderer) {
        s_last_error = "Renderer not initialized";
        return nullptr;
    }

    // Load FBX file
    ufbx_load_opts opts{};
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.target_unit_meters = 1.0f;

    ufbx_error error;
    ufbx_scene* scene = ufbx_load_file(path.c_str(), &opts, &error);

    if (!scene) {
        s_last_error = "Failed to load FBX: " + std::string(error.description.data, error.description.length);
        log(LogLevel::Error, s_last_error.c_str());
        return nullptr;
    }

    std::vector<Vertex> all_vertices;
    std::vector<uint32_t> all_indices;
    Vec3 bounds_min{std::numeric_limits<float>::max()};
    Vec3 bounds_max{std::numeric_limits<float>::lowest()};

    // Process all meshes in the scene
    for (size_t mesh_idx = 0; mesh_idx < scene->meshes.count; mesh_idx++) {
        const ufbx_mesh* mesh = scene->meshes.data[mesh_idx];

        // Get mesh node for transform (first instance)
        Mat4 transform = Mat4{1.0f};
        if (mesh->instances.count > 0) {
            transform = to_mat4(mesh->instances.data[0]->geometry_to_world);
        }

    uint32_t base_vertex = static_cast<uint32_t>(all_vertices.size());

    // Triangulate each face
    size_t num_triangles = mesh->num_triangles;
    std::vector<uint32_t> tri_indices(mesh->num_indices);
    for (size_t i = 0; i < mesh->num_indices; ++i) {
        tri_indices[i] = static_cast<uint32_t>(i);
    }

        // Process each triangle
        for (size_t i = 0; i < tri_indices.size(); i++) {
            uint32_t idx = tri_indices[i];
            if (idx >= mesh->num_indices) continue;

            uint32_t vertex_idx = mesh->vertex_indices.data[idx];

            Vertex vert{};

            // Position (apply transform)
            if (vertex_idx < mesh->vertices.count) {
                ufbx_vec3 pos = mesh->vertices.data[vertex_idx];
                Vec4 world_pos = transform * Vec4{static_cast<float>(pos.x), static_cast<float>(pos.y), static_cast<float>(pos.z), 1.0f};
                vert.position = Vec3{world_pos.x, world_pos.y, world_pos.z};
            }

            // Update bounds
            bounds_min.x = std::min(bounds_min.x, vert.position.x);
            bounds_min.y = std::min(bounds_min.y, vert.position.y);
            bounds_min.z = std::min(bounds_min.z, vert.position.z);
            bounds_max.x = std::max(bounds_max.x, vert.position.x);
            bounds_max.y = std::max(bounds_max.y, vert.position.y);
            bounds_max.z = std::max(bounds_max.z, vert.position.z);

            // Normal
            if (mesh->vertex_normal.exists && idx < mesh->vertex_normal.indices.count) {
                uint32_t normal_idx = mesh->vertex_normal.indices.data[idx];
                if (normal_idx < mesh->vertex_normal.values.count) {
                    vert.normal = to_vec3(mesh->vertex_normal.values.data[normal_idx]);
                }
            } else {
                vert.normal = Vec3{0.0f, 1.0f, 0.0f};
            }

            // UV
            if (mesh->vertex_uv.exists && idx < mesh->vertex_uv.indices.count) {
                uint32_t uv_idx = mesh->vertex_uv.indices.data[idx];
                if (uv_idx < mesh->vertex_uv.values.count) {
                    Vec2 uv = to_vec2(mesh->vertex_uv.values.data[uv_idx]);
                    vert.texcoord = Vec2{uv.x, 1.0f - uv.y}; // Flip Y
                }
            }

            // Vertex color
            if (mesh->vertex_color.exists && idx < mesh->vertex_color.indices.count) {
                uint32_t color_idx = mesh->vertex_color.indices.data[idx];
                if (color_idx < mesh->vertex_color.values.count) {
                    vert.color = to_vec4(mesh->vertex_color.values.data[color_idx]);
                }
            } else {
                vert.color = Vec4{1.0f};
            }

            all_vertices.push_back(vert);
            all_indices.push_back(base_vertex + static_cast<uint32_t>(i));
        }
    }

    ufbx_free_scene(scene);

    if (all_vertices.empty()) {
        s_last_error = "No vertices found in FBX file";
        return nullptr;
    }

    // Compute tangents
    std::vector<Vec3> tangents(all_vertices.size(), Vec3{0.0f});
    std::vector<int> tangent_counts(all_vertices.size(), 0);

    for (size_t i = 0; i + 2 < all_indices.size(); i += 3) {
        uint32_t i0 = all_indices[i];
        uint32_t i1 = all_indices[i + 1];
        uint32_t i2 = all_indices[i + 2];

        Vec3 tangent;
        compute_tangent(
            all_vertices[i0].position, all_vertices[i1].position, all_vertices[i2].position,
            all_vertices[i0].texcoord, all_vertices[i1].texcoord, all_vertices[i2].texcoord,
            tangent);

        tangents[i0] = tangents[i0] + tangent;
        tangents[i1] = tangents[i1] + tangent;
        tangents[i2] = tangents[i2] + tangent;
        tangent_counts[i0]++;
        tangent_counts[i1]++;
        tangent_counts[i2]++;
    }

    for (size_t i = 0; i < all_vertices.size(); i++) {
        if (tangent_counts[i] > 0) {
            Vec3& t = tangents[i];
            float len = std::sqrt(t.x * t.x + t.y * t.y + t.z * t.z);
            if (len > 1e-6f) {
                all_vertices[i].tangent = Vec3{t.x / len, t.y / len, t.z / len};
            }
        }
    }

    // Create mesh data
    MeshData mesh_data;
    mesh_data.vertices = std::move(all_vertices);
    mesh_data.indices = std::move(all_indices);
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

    log(LogLevel::Debug, ("Loaded FBX: " + path +
        " (verts: " + std::to_string(asset->vertex_count) +
        ", indices: " + std::to_string(asset->index_count) + ")").c_str());

    return asset;
}

std::unique_ptr<ImportedModel> FbxImporter::import_model(const std::string& path) {
    s_last_error.clear();

    // Load FBX file
    ufbx_load_opts opts{};
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.target_unit_meters = 1.0f;

    ufbx_error error;
    ufbx_scene* scene = ufbx_load_file(path.c_str(), &opts, &error);

    if (!scene) {
        s_last_error = "Failed to load FBX: " + std::string(error.description.data, error.description.length);
        return nullptr;
    }

    auto model = std::make_unique<ImportedModel>();

    // Process all meshes
    for (size_t mesh_idx = 0; mesh_idx < scene->meshes.count; mesh_idx++) {
        const ufbx_mesh* fbx_mesh = scene->meshes.data[mesh_idx];

        ImportedMesh imported_mesh;
        imported_mesh.name = fbx_mesh->name.data;

        // Check for skinning
        ufbx_skin_deformer* skin = nullptr;
        if (fbx_mesh->skin_deformers.count > 0) {
            skin = fbx_mesh->skin_deformers.data[0];
            imported_mesh.has_skinning = true;
        }

        Vec3 bounds_min{std::numeric_limits<float>::max()};
        Vec3 bounds_max{std::numeric_limits<float>::lowest()};

    // Triangulate
    size_t num_triangles = fbx_mesh->num_triangles;
    std::vector<uint32_t> tri_indices(fbx_mesh->num_indices);
    for (size_t i = 0; i < fbx_mesh->num_indices; ++i) {
        tri_indices[i] = static_cast<uint32_t>(i);
    }

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<IVec4> bone_indices_vec;
        std::vector<Vec4> bone_weights_vec;

        for (size_t i = 0; i < tri_indices.size(); i++) {
            uint32_t idx = tri_indices[i];
            if (idx >= fbx_mesh->num_indices) continue;

            uint32_t vertex_idx = fbx_mesh->vertex_indices.data[idx];

            Vertex vert{};

            // Position
            if (vertex_idx < fbx_mesh->vertices.count) {
                vert.position = to_vec3(fbx_mesh->vertices.data[vertex_idx]);
            }

            // Bounds
            bounds_min.x = std::min(bounds_min.x, vert.position.x);
            bounds_min.y = std::min(bounds_min.y, vert.position.y);
            bounds_min.z = std::min(bounds_min.z, vert.position.z);
            bounds_max.x = std::max(bounds_max.x, vert.position.x);
            bounds_max.y = std::max(bounds_max.y, vert.position.y);
            bounds_max.z = std::max(bounds_max.z, vert.position.z);

            // Normal
            if (fbx_mesh->vertex_normal.exists && idx < fbx_mesh->vertex_normal.indices.count) {
                uint32_t normal_idx = fbx_mesh->vertex_normal.indices.data[idx];
                if (normal_idx < fbx_mesh->vertex_normal.values.count) {
                    vert.normal = to_vec3(fbx_mesh->vertex_normal.values.data[normal_idx]);
                }
            }

            // UV
            if (fbx_mesh->vertex_uv.exists && idx < fbx_mesh->vertex_uv.indices.count) {
                uint32_t uv_idx = fbx_mesh->vertex_uv.indices.data[idx];
                if (uv_idx < fbx_mesh->vertex_uv.values.count) {
                    Vec2 uv = to_vec2(fbx_mesh->vertex_uv.values.data[uv_idx]);
                    vert.texcoord = Vec2{uv.x, 1.0f - uv.y};
                }
            }

            // Vertex color
            if (fbx_mesh->vertex_color.exists && idx < fbx_mesh->vertex_color.indices.count) {
                uint32_t color_idx = fbx_mesh->vertex_color.indices.data[idx];
                if (color_idx < fbx_mesh->vertex_color.values.count) {
                    vert.color = to_vec4(fbx_mesh->vertex_color.values.data[color_idx]);
                }
            } else {
                vert.color = Vec4{1.0f};
            }

            vertices.push_back(vert);
            indices.push_back(static_cast<uint32_t>(vertices.size() - 1));

            // Skinning data
            if (skin && vertex_idx < skin->vertices.count) {
                ufbx_skin_vertex skin_vert = skin->vertices.data[vertex_idx];

                IVec4 bone_idx{0, 0, 0, 0};
                Vec4 bone_weight{0.0f, 0.0f, 0.0f, 0.0f};

                size_t num_weights = std::min(skin_vert.num_weights, static_cast<uint32_t>(4));
                for (size_t w = 0; w < num_weights; w++) {
                    ufbx_skin_weight sw = skin->weights.data[skin_vert.weight_begin + w];
                    bone_idx[static_cast<int>(w)] = static_cast<int>(sw.cluster_index);
                    bone_weight[static_cast<int>(w)] = static_cast<float>(sw.weight);
                }

                // Normalize weights
                float total = bone_weight.x + bone_weight.y + bone_weight.z + bone_weight.w;
                if (total > 0.0f) {
                    bone_weight.x /= total;
                    bone_weight.y /= total;
                    bone_weight.z /= total;
                    bone_weight.w /= total;
                }

                bone_indices_vec.push_back(bone_idx);
                bone_weights_vec.push_back(bone_weight);
            }
        }

        imported_mesh.mesh_data.vertices = std::move(vertices);
        imported_mesh.mesh_data.indices = std::move(indices);
        imported_mesh.mesh_data.bounds.min = bounds_min;
        imported_mesh.mesh_data.bounds.max = bounds_max;

        if (imported_mesh.has_skinning) {
            imported_mesh.bone_indices = std::move(bone_indices_vec);
            imported_mesh.bone_weights = std::move(bone_weights_vec);
        }

        model->meshes.push_back(std::move(imported_mesh));
    }

    // Extract skeletons
    for (size_t skin_idx = 0; skin_idx < scene->skin_deformers.count; skin_idx++) {
        const ufbx_skin_deformer* skin = scene->skin_deformers.data[skin_idx];

        SkeletonData skeleton;

        for (size_t cluster_idx = 0; cluster_idx < skin->clusters.count; cluster_idx++) {
            const ufbx_skin_cluster* cluster = skin->clusters.data[cluster_idx];

            SkeletonData::Joint joint;
            joint.name = cluster->bone_node ? std::string(cluster->bone_node->name.data) : "";
            joint.inverse_bind_matrix = to_mat4(cluster->geometry_to_bone);

            // Find parent
            joint.parent_index = -1;
            if (cluster->bone_node && cluster->bone_node->parent) {
                for (size_t j = 0; j < cluster_idx; j++) {
                    if (skin->clusters.data[j]->bone_node == cluster->bone_node->parent) {
                        joint.parent_index = static_cast<int32_t>(j);
                        break;
                    }
                }
            }

            // Local transform
            if (cluster->bone_node) {
                joint.local_transform = to_mat4(cluster->bone_node->local_transform);
            }

            skeleton.joints.push_back(joint);
            skeleton.joint_indices.push_back(static_cast<int32_t>(cluster_idx));
        }

        model->skeletons.push_back(std::move(skeleton));
    }

    // Extract animations
    for (size_t anim_idx = 0; anim_idx < scene->anim_stacks.count; anim_idx++) {
        const ufbx_anim_stack* stack = scene->anim_stacks.data[anim_idx];

        AnimationData anim;
        anim.name = stack->name.data;
        anim.duration = static_cast<float>(stack->time_end - stack->time_begin);

        // Sample animation at regular intervals
        double sample_rate = 30.0;
        int num_samples = static_cast<int>(anim.duration * sample_rate) + 1;

        // Find animated nodes (typically bones)
        for (size_t layer_idx = 0; layer_idx < stack->layers.count; layer_idx++) {
            const ufbx_anim_layer* layer = stack->layers.data[layer_idx];

            for (size_t node_idx = 0; node_idx < scene->nodes.count; node_idx++) {
                const ufbx_node* node = scene->nodes.data[node_idx];

                // Check if this node has animation on any property
                bool has_translation = false;
                bool has_rotation = false;
                bool has_scale = false;

                for (size_t prop_idx = 0; prop_idx < layer->anim_props.count; prop_idx++) {
                    const ufbx_anim_prop& prop = layer->anim_props.data[prop_idx];
                    if (prop.element != &node->element) continue;

                    if (prop.prop_name.data && strstr(prop.prop_name.data, "Lcl Translation")) {
                        has_translation = true;
                    } else if (prop.prop_name.data && strstr(prop.prop_name.data, "Lcl Rotation")) {
                        has_rotation = true;
                    } else if (prop.prop_name.data && strstr(prop.prop_name.data, "Lcl Scaling")) {
                        has_scale = true;
                    }
                }

                // Find joint index for this node
                int32_t joint_index = -1;
                for (size_t skin_idx = 0; skin_idx < scene->skin_deformers.count; skin_idx++) {
                    const ufbx_skin_deformer* skin = scene->skin_deformers.data[skin_idx];
                    for (size_t c = 0; c < skin->clusters.count; c++) {
                        if (skin->clusters.data[c]->bone_node == node) {
                            joint_index = static_cast<int32_t>(c);
                            break;
                        }
                    }
                    if (joint_index >= 0) break;
                }

                if (joint_index < 0) continue;

                // Sample translation
                if (has_translation) {
                    AnimationData::Channel channel;
                    channel.target_joint = joint_index;
                    channel.path = "translation";

                    for (int s = 0; s < num_samples; s++) {
                        double time = stack->time_begin + (s / sample_rate);
                        channel.times.push_back(static_cast<float>(time - stack->time_begin));

                        ufbx_transform transform = ufbx_evaluate_transform(stack->anim, node, time);
                        channel.values.push_back(static_cast<float>(transform.translation.x));
                        channel.values.push_back(static_cast<float>(transform.translation.y));
                        channel.values.push_back(static_cast<float>(transform.translation.z));
                    }

                    anim.channels.push_back(std::move(channel));
                }

                // Sample rotation (as quaternion)
                if (has_rotation) {
                    AnimationData::Channel channel;
                    channel.target_joint = joint_index;
                    channel.path = "rotation";

                    for (int s = 0; s < num_samples; s++) {
                        double time = stack->time_begin + (s / sample_rate);
                        channel.times.push_back(static_cast<float>(time - stack->time_begin));

                        ufbx_transform transform = ufbx_evaluate_transform(stack->anim, node, time);
                        channel.values.push_back(static_cast<float>(transform.rotation.x));
                        channel.values.push_back(static_cast<float>(transform.rotation.y));
                        channel.values.push_back(static_cast<float>(transform.rotation.z));
                        channel.values.push_back(static_cast<float>(transform.rotation.w));
                    }

                    anim.channels.push_back(std::move(channel));
                }

                // Sample scale
                if (has_scale) {
                    AnimationData::Channel channel;
                    channel.target_joint = joint_index;
                    channel.path = "scale";

                    for (int s = 0; s < num_samples; s++) {
                        double time = stack->time_begin + (s / sample_rate);
                        channel.times.push_back(static_cast<float>(time - stack->time_begin));

                        ufbx_transform transform = ufbx_evaluate_transform(stack->anim, node, time);
                        channel.values.push_back(static_cast<float>(transform.scale.x));
                        channel.values.push_back(static_cast<float>(transform.scale.y));
                        channel.values.push_back(static_cast<float>(transform.scale.z));
                    }

                    anim.channels.push_back(std::move(channel));
                }
            }
        }

        if (!anim.channels.empty()) {
            model->animations.push_back(std::move(anim));
        }
    }

    ufbx_free_scene(scene);

    log(LogLevel::Debug, ("Loaded FBX model: " + path +
        " (meshes: " + std::to_string(model->meshes.size()) +
        ", skeletons: " + std::to_string(model->skeletons.size()) +
        ", animations: " + std::to_string(model->animations.size()) + ")").c_str());

    return model;
}

const std::string& FbxImporter::get_last_error() {
    return s_last_error;
}

} // namespace engine::asset
