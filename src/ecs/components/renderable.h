#pragma once

#include "core/resource_handle.h"

// Lightweight renderable component linking an entity to a mesh resource.
struct Renderable {
    MeshHandle mesh = MeshHandle::Invalid;
    bool visible = true;
    bool castsShadows = true;

    // Placeholder for future material binding.
    // MaterialHandle material = MaterialHandle::Invalid;
};
