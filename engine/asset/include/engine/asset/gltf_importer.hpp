#pragma once

#include <engine/asset/types.hpp>
#include <engine/render/renderer.hpp>
#include <string>
#include <memory>
#include <vector>

namespace engine::asset {

// Skeleton data extracted from glTF for animation support
struct SkeletonData {
    struct Joint {
        std::string name;
        int32_t parent_index = -1;      // -1 for root
        core::Mat4 inverse_bind_matrix;
        core::Mat4 local_transform;
    };

    std::vector<Joint> joints;
    std::vector<int32_t> joint_indices; // Maps skin joint indices to skeleton indices
};

// Animation data extracted from glTF
struct AnimationData {
    struct Channel {
        int32_t target_joint;
        std::string path; // "translation", "rotation", "scale"
        std::vector<float> times;
        std::vector<float> values; // Interleaved values
    };

    std::string name;
    float duration = 0.0f;
    std::vector<Channel> channels;
};

// Extended mesh data with skinning support
struct ImportedMesh {
    render::MeshData mesh_data;

    // Skinning data (optional)
    bool has_skinning = false;
    std::vector<core::IVec4> bone_indices;  // 4 bone indices per vertex
    std::vector<core::Vec4> bone_weights;   // 4 bone weights per vertex

    // Material reference
    int32_t material_index = -1;
    std::string name;
};

// Complete imported model
struct ImportedModel {
    std::vector<ImportedMesh> meshes;
    std::vector<SkeletonData> skeletons;
    std::vector<AnimationData> animations;

    // Embedded textures (if any)
    std::vector<render::TextureData> textures;
};

// glTF Importer class
class GltfImporter {
public:
    // Import a glTF/glb file and return the mesh data
    // Returns nullptr on failure
    static std::shared_ptr<MeshAsset> import_mesh(
        const std::string& path,
        render::IRenderer* renderer
    );

    // Import full model with all meshes, skeleton, and animations
    static std::unique_ptr<ImportedModel> import_model(const std::string& path);

    // Get last error message
    static const std::string& get_last_error();

private:
    static std::string s_last_error;
};

} // namespace engine::asset
