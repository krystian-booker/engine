#include <iostream>
#include "core/types.h"
#include "core/math.h"

int main(int, char**) {
    std::cout << "Game Engine - Day 1" << std::endl;

    // Test math
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(4.0f, 5.0f, 6.0f);
    Vec3 c = a + b;

    std::cout << "Vec3 add result: ("
              << c.x << ", " << c.y << ", " << c.z << ")" << std::endl;

    f32 dot = Dot(a, b);
    std::cout << "Dot product: " << dot << std::endl;

    return 0;
}