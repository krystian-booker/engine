#pragma once

#include <engine/asset/types.hpp>
#include <engine/render/renderer.hpp>
#include <string>
#include <memory>
#include <vector>

namespace engine::asset {

// Material reference from MTL file
struct ObjMaterial {
    std::string name;
    std::string diffuse_texture;
    std::string normal_texture;
    std::string specular_texture;
    core::Vec3 ambient{0.1f};
    core::Vec3 diffuse{0.8f};
    core::Vec3 specular{0.0f};
    float shininess = 32.0f;
    float opacity = 1.0f;
};

// OBJ Importer class
class ObjImporter {
public:
    // Import an OBJ file and return the mesh data
    // Combines all shapes into a single mesh
    static std::shared_ptr<MeshAsset> import_mesh(
        const std::string& path,
        render::IRenderer* renderer
    );

    // Import OBJ and get material information
    static std::shared_ptr<MeshAsset> import_mesh_with_materials(
        const std::string& path,
        render::IRenderer* renderer,
        std::vector<ObjMaterial>& out_materials
    );

    // Get last error message
    static const std::string& get_last_error();

private:
    static std::string s_last_error;
};

} // namespace engine::asset
