#pragma once
#include "core/types.h"
#include "core/math.h"
#include <string>

struct MeshRenderer {
    std::string meshPath;       // Path to mesh file (e.g., "models/cube.gltf")
    std::string materialPath;   // Path to material (e.g., "materials/default.mat")

    bool castsShadows = true;
    bool receiveShadows = true;
    bool visible = true;

    // Bounding sphere for culling (computed from mesh)
    Vec3 boundingSphereCenter{0, 0, 0};
    f32 boundingSphereRadius = 1.0f;
};
