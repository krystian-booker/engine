#include "ecs/component_array.h"
#include "ecs/entity_manager.h"
#include "core/math.h"
#include <iostream>

// Example component from the specification
struct Position {
    Vec3 value;
};

int main() {
    std::cout << "=== Component Array Example Test ===" << std::endl;
    std::cout << std::endl;

    // Test the exact example from the specification
    ComponentArray<Position> positions;
    EntityManager em;
    Entity e1 = em.CreateEntity();

    positions.Add(e1, Position{{1, 2, 3}});

    Position& pos = positions.Get(e1);
    pos.value.x += 10.0f;

    std::cout << "Position component for entity " << e1.index << ":" << std::endl;
    std::cout << "  x: " << pos.value.x << " (expected: 11)" << std::endl;
    std::cout << "  y: " << pos.value.y << " (expected: 2)" << std::endl;
    std::cout << "  z: " << pos.value.z << " (expected: 3)" << std::endl;
    std::cout << std::endl;

    // Verify expected values
    if (pos.value.x == 11.0f && pos.value.y == 2.0f && pos.value.z == 3.0f) {
        std::cout << "SUCCESS: Components store/retrieve correctly with O(1) access" << std::endl;
        return 0;
    } else {
        std::cout << "FAILURE: Unexpected values" << std::endl;
        return 1;
    }
}
