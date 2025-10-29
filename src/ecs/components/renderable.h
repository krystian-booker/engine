#pragma once

#include "core/resource_handle.h"

// Lightweight renderable component linking an entity to a mesh and material resource.
struct Renderable {
    MeshHandle mesh = MeshHandle::Invalid;
    MaterialHandle material = MaterialHandle::Invalid;
    bool visible = true;
    bool castsShadows = true;
};
