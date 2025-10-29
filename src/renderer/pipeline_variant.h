#pragma once

#include "core/types.h"
#include "core/material_data.h"
#include <functional>

// Pipeline variants for different material rendering modes
enum class PipelineVariant : u8 {
    Opaque = 0,               // Standard opaque rendering (cull back faces)
    OpaqueDoubleSided,        // Opaque without culling
    AlphaBlend,               // Alpha blending enabled (cull back)
    AlphaBlendDoubleSided,    // Alpha blending + no culling
    AlphaMask,                // Alpha masking with discard (cull back)
    AlphaMaskDoubleSided,     // Alpha masking + no culling

    Count  // Total number of variants
};

// Hash function for PipelineVariant (for use with unordered_map)
namespace std {
    template<>
    struct hash<PipelineVariant> {
        size_t operator()(PipelineVariant variant) const {
            return std::hash<u8>()(static_cast<u8>(variant));
        }
    };
}

// Determine pipeline variant from material flags
inline PipelineVariant GetPipelineVariant(MaterialFlags flags) {
    bool doubleSided = HasFlag(flags, MaterialFlags::DoubleSided);
    bool alphaBlend = HasFlag(flags, MaterialFlags::AlphaBlend);
    bool alphaMask = HasFlag(flags, MaterialFlags::AlphaMask) || HasFlag(flags, MaterialFlags::AlphaTest);

    if (alphaBlend) {
        return doubleSided ? PipelineVariant::AlphaBlendDoubleSided : PipelineVariant::AlphaBlend;
    } else if (alphaMask) {
        return doubleSided ? PipelineVariant::AlphaMaskDoubleSided : PipelineVariant::AlphaMask;
    } else {
        return doubleSided ? PipelineVariant::OpaqueDoubleSided : PipelineVariant::Opaque;
    }
}

// Get sort order for rendering (opaque first, then masked, then blended)
inline u32 GetPipelineVariantSortOrder(PipelineVariant variant) {
    switch (variant) {
        case PipelineVariant::Opaque:
        case PipelineVariant::OpaqueDoubleSided:
            return 0;  // Render opaque objects first

        case PipelineVariant::AlphaMask:
        case PipelineVariant::AlphaMaskDoubleSided:
            return 1;  // Then alpha-masked

        case PipelineVariant::AlphaBlend:
        case PipelineVariant::AlphaBlendDoubleSided:
            return 2;  // Finally alpha-blended (back-to-front)

        default:
            return 0;
    }
}
