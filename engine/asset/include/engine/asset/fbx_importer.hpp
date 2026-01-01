#pragma once

#include <engine/asset/types.hpp>
#include <engine/asset/gltf_importer.hpp>  // Reuse ImportedMesh, ImportedModel, etc.
#include <engine/render/renderer.hpp>
#include <string>
#include <memory>
#include <vector>

namespace engine::asset {

// FBX Importer class
// Uses ufbx library for parsing FBX files
class FbxImporter {
public:
    // Import an FBX file and return the mesh data
    // Combines all meshes into a single mesh asset
    static std::shared_ptr<MeshAsset> import_mesh(
        const std::string& path,
        render::IRenderer* renderer
    );

    // Import full model with all meshes, skeleton, and animations
    // Reuses ImportedModel struct from gltf_importer.hpp
    static std::unique_ptr<ImportedModel> import_model(const std::string& path);

    // Get last error message
    static const std::string& get_last_error();

private:
    static std::string s_last_error;
};

} // namespace engine::asset
