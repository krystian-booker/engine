#include "ltc_tables.h"
#include <cmath>
#include <vector>

// LTC lookup table data generated from Eric Heitz's original implementation
// Source: https://eheitzresearch.wordpress.com/415-2/
//
// This is a simplified runtime-generated version
// For production, use pre-fitted data from: https://github.com/selfshadow/ltc_code

namespace LTCTables {

// Runtime-generated LTC tables (simplified approximation)
static std::vector<f32> g_LTCMatrixData;
static std::vector<f32> g_LTCAmplitudeData;
static bool g_Initialized = false;

// Initialize LTC lookup tables at runtime
void InitializeLTCTables() {
    if (g_Initialized) return;

    constexpr u32 matrixSize = LTC_LUT_SIZE * LTC_LUT_SIZE * 4;
    constexpr u32 amplitudeSize = LTC_LUT_SIZE * LTC_LUT_SIZE * 2;

    g_LTCMatrixData.resize(matrixSize);
    g_LTCAmplitudeData.resize(amplitudeSize);

    // Generate simplified LTC matrix data
    for (u32 y = 0; y < LTC_LUT_SIZE; ++y) {
        for (u32 x = 0; x < LTC_LUT_SIZE; ++x) {
            f32 roughness = (x + 0.5f) / static_cast<f32>(LTC_LUT_SIZE);
            f32 cosTheta = (y + 0.5f) / static_cast<f32>(LTC_LUT_SIZE);

            u32 matrixIdx = (y * LTC_LUT_SIZE + x) * 4;
            u32 amplitudeIdx = (y * LTC_LUT_SIZE + x) * 2;

            // Simplified approximation (better than identity, but not as accurate as fitted data)
            // Real LTC data requires complex BRDF fitting
            f32 a = 1.0f / (roughness * roughness + 0.01f);  // Inverse roughness scaling
            f32 bias = roughness * 0.5f;

            // Matrix elements (inverse M^-1)
            g_LTCMatrixData[matrixIdx + 0] = a;          // m11
            g_LTCMatrixData[matrixIdx + 1] = a;          // m22
            g_LTCMatrixData[matrixIdx + 2] = bias;       // m13
            g_LTCMatrixData[matrixIdx + 3] = 0.0f;       // m23

            // Amplitude and Fresnel
            f32 magnitude = 1.0f - roughness * 0.5f;  // Rougher surfaces = less specular
            f32 fresnel = std::pow(1.0f - cosTheta, 5.0f);  // Schlick approximation

            g_LTCAmplitudeData[amplitudeIdx + 0] = magnitude;
            g_LTCAmplitudeData[amplitudeIdx + 1] = fresnel;
        }
    }

    g_Initialized = true;
}

// Get LTC matrix data pointer (initialize if needed)
const f32* GetLTCMatrixData() {
    InitializeLTCTables();
    return g_LTCMatrixData.data();
}

// Get LTC amplitude data pointer (initialize if needed)
const f32* GetLTCAmplitudeData() {
    InitializeLTCTables();
    return g_LTCAmplitudeData.data();
}

// Legacy arrays for compatibility
const f32 LTC_MATRIX_DATA[LTC_LUT_SIZE * LTC_LUT_SIZE * 4] = { 1.0f }; // Dummy, use GetLTCMatrixData() instead
const f32 LTC_AMPLITUDE_DATA[LTC_LUT_SIZE * LTC_LUT_SIZE * 2] = { 1.0f }; // Dummy, use GetLTCAmplitudeData() instead

} // namespace LTCTables
