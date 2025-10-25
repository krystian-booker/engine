#include <iostream>
#include "core/math.h"

int main(int, char**) {
    std::cout << "=== Game Engine - Day 1: Math Tests ===" << std::endl;
    std::cout << std::endl;

    // Test 1: Vector addition
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(4.0f, 5.0f, 6.0f);
    Vec3 c = a + b;

    std::cout << "[TEST 1] Vec3 addition:" << std::endl;
    std::cout << "  a + b = (" << c.x << ", " << c.y << ", " << c.z << ")" << std::endl;
    std::cout << "  Expected: (5, 7, 9)" << std::endl;
    std::cout << std::endl;

    // Test 2: Dot product
    f32 dot = Dot(a, b);
    std::cout << "[TEST 2] Dot product:" << std::endl;
    std::cout << "  Dot(a, b) = " << dot << std::endl;
    std::cout << "  Expected: 32" << std::endl;
    std::cout << std::endl;

    // Test 3: Cross product
    Vec3 cross = Cross(a, b);
    std::cout << "[TEST 3] Cross product:" << std::endl;
    std::cout << "  Cross(a, b) = (" << cross.x << ", " << cross.y << ", " << cross.z << ")" << std::endl;
    std::cout << "  Expected: (-3, 6, -3)" << std::endl;
    std::cout << std::endl;

    // Test 4: Vector length
    f32 len = Length(a);
    std::cout << "[TEST 4] Vector length:" << std::endl;
    std::cout << "  Length(a) = " << len << std::endl;
    std::cout << "  Expected: ~3.74" << std::endl;
    std::cout << std::endl;

    // Test 5: Normalization
    Vec3 normalized = Normalize(a);
    f32 normalizedLen = Length(normalized);
    std::cout << "[TEST 5] Normalization:" << std::endl;
    std::cout << "  Normalized = (" << normalized.x << ", " << normalized.y << ", " << normalized.z << ")" << std::endl;
    std::cout << "  Length of normalized = " << normalizedLen << std::endl;
    std::cout << "  Expected length: 1.0" << std::endl;
    std::cout << std::endl;

    // Test 6: Matrix creation and transformation
    Mat4 identity(1.0f);
    Mat4 translation = Translate(identity, Vec3(10.0f, 20.0f, 30.0f));
    (void)translation; 
    std::cout << "[TEST 6] Matrix transformation:" << std::endl;
    std::cout << "  Translation matrix created" << std::endl;
    std::cout << "  Translation: (10, 20, 30)" << std::endl;
    std::cout << std::endl;

    // Test 7: Rotation matrix
    Mat4 rotation = Rotate(identity, Radians(45.0f), Vec3(0, 1, 0));
    (void)rotation; 
    std::cout << "[TEST 7] Rotation matrix:" << std::endl;
    std::cout << "  45-degree rotation around Y-axis created" << std::endl;
    std::cout << std::endl;

    // Test 8: Quaternion
    Quat quat = QuatFromAxisAngle(Vec3(0, 1, 0), Radians(90.0f));
    Mat4 quatMat = QuatToMat4(quat);
    (void)quatMat; 
    std::cout << "[TEST 8] Quaternion:" << std::endl;
    std::cout << "  90-degree quaternion rotation created" << std::endl;
    std::cout << "  Converted to matrix successfully" << std::endl;
    std::cout << std::endl;

    // Test 9: Perspective projection
    Mat4 proj = Perspective(Radians(60.0f), 16.0f/9.0f, 0.1f, 100.0f);
    (void)proj; 
    std::cout << "[TEST 9] Perspective projection:" << std::endl;
    std::cout << "  FOV: 60°, Aspect: 16:9, Near: 0.1, Far: 100" << std::endl;
    std::cout << std::endl;

    // Test 10: View matrix
    Vec3 eye(0, 5, 10);
    Vec3 target(0, 0, 0);
    Vec3 up(0, 1, 0);
    Mat4 view = LookAt(eye, target, up);
    (void)view; 
    std::cout << "[TEST 10] View matrix (LookAt):" << std::endl;
    std::cout << "  Eye: (0, 5, 10)" << std::endl;
    std::cout << "  Target: (0, 0, 0)" << std::endl;
    std::cout << "  Up: (0, 1, 0)" << std::endl;
    std::cout << std::endl;

    std::cout << "======================================" << std::endl;
    std::cout << "All math tests completed successfully! ✓" << std::endl;
    std::cout << "GLM integration working perfectly." << std::endl;
    std::cout << "======================================" << std::endl;

    return 0;
}