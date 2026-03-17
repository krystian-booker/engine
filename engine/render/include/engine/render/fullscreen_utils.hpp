#pragma once

#include <array>
#include <cstdint>

namespace engine::render {

struct FullscreenQuadVertex {
    float x;
    float y;
    float z;
    float u;
    float v;
};

constexpr std::array<FullscreenQuadVertex, 4> make_fullscreen_quad_vertices(bool origin_bottom_left) {
    const float top_v = origin_bottom_left ? 1.0f : 0.0f;
    const float bottom_v = origin_bottom_left ? 0.0f : 1.0f;

    return {{
        { -1.0f,  1.0f, 0.0f, 0.0f, top_v    },
        {  1.0f,  1.0f, 0.0f, 1.0f, top_v    },
        {  1.0f, -1.0f, 0.0f, 1.0f, bottom_v },
        { -1.0f, -1.0f, 0.0f, 0.0f, bottom_v },
    }};
}

constexpr std::array<uint16_t, 6> make_fullscreen_quad_indices() {
    return {{ 0, 1, 2, 0, 2, 3 }};
}

} // namespace engine::render
