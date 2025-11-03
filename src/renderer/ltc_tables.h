#pragma once

#include "core/types.h"

// LTC (Linearly Transformed Cosines) lookup table data for area lights
// Based on "Real-Time Polygonal-Light Shading with Linearly Transformed Cosines" by Heitz et al. (2016)
//
// The LTC method approximates the BRDF as a clamped cosine distribution that has been linearly transformed.
// Two lookup tables are used:
// 1. LTC Matrix (inverse M^-1): 4 floats (m11, m22, m13, m23) - 2x2 matrix + offset stored in 64x64 texture
// 2. LTC Amplitude/Fresnel: 2 floats (magnitude, fresnel) stored in 64x64 texture
//
// Lookup coordinates: (roughness, cos(theta)) where theta is angle between normal and view

namespace LTCTables {

// LTC matrix lookup table (64x64 RGBA32F)
// Each texel contains: (m11, m22, m13, m23)
// The full 3x3 matrix M^-1 is:
// [ m11  0   m13 ]
// [  0  m22  m23 ]
// [  0   0    1  ]
constexpr u32 LTC_LUT_SIZE = 64;

// Initialize LTC tables (call once at startup)
void InitializeLTCTables();

// Get runtime-generated LTC matrix data
const f32* GetLTCMatrixData();

// Get runtime-generated LTC amplitude data
const f32* GetLTCAmplitudeData();

// Legacy arrays (deprecated - use Get functions instead)
extern const f32 LTC_MATRIX_DATA[LTC_LUT_SIZE * LTC_LUT_SIZE * 4];
extern const f32 LTC_AMPLITUDE_DATA[LTC_LUT_SIZE * LTC_LUT_SIZE * 2];

} // namespace LTCTables
