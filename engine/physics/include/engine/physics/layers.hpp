#pragma once

#include <cstdint>
#include <bitset>

namespace engine::physics {

// Predefined collision layers
namespace layers {
    constexpr uint16_t STATIC = 0;
    constexpr uint16_t DYNAMIC = 1;
    constexpr uint16_t PLAYER = 2;
    constexpr uint16_t ENEMY = 3;
    constexpr uint16_t TRIGGER = 4;
    constexpr uint16_t DEBRIS = 5;
    constexpr uint16_t PROJECTILE = 6;
    // User-defined layers start at 8
    constexpr uint16_t USER_START = 8;
    constexpr uint16_t MAX_LAYERS = 16;
}

// Collision filter - defines which layers collide with which
class CollisionFilter {
public:
    CollisionFilter() {
        // Default: everything collides with everything
        for (uint16_t i = 0; i < layers::MAX_LAYERS; ++i) {
            m_matrix[i].set();
        }
    }

    // Set whether two layers should collide
    void set_collision(uint16_t layer_a, uint16_t layer_b, bool collides) {
        if (layer_a >= layers::MAX_LAYERS || layer_b >= layers::MAX_LAYERS) return;
        m_matrix[layer_a][layer_b] = collides;
        m_matrix[layer_b][layer_a] = collides;
    }

    // Check if two layers should collide
    bool should_collide(uint16_t layer_a, uint16_t layer_b) const {
        if (layer_a >= layers::MAX_LAYERS || layer_b >= layers::MAX_LAYERS) return false;
        return m_matrix[layer_a][layer_b];
    }

    // Set all layers to collide or not with a specific layer
    void set_layer_collisions(uint16_t layer, bool collides_with_all) {
        if (layer >= layers::MAX_LAYERS) return;
        if (collides_with_all) {
            m_matrix[layer].set();
            for (uint16_t i = 0; i < layers::MAX_LAYERS; ++i) {
                m_matrix[i][layer] = true;
            }
        } else {
            m_matrix[layer].reset();
            for (uint16_t i = 0; i < layers::MAX_LAYERS; ++i) {
                m_matrix[i][layer] = false;
            }
        }
    }

private:
    std::bitset<layers::MAX_LAYERS> m_matrix[layers::MAX_LAYERS];
};

} // namespace engine::physics
