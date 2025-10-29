#pragma once
#include "core/types.h"
#include "core/math.h"

// Material workflow conversion utilities
// Converts between Specular/Glossiness and Metallic/Roughness PBR workflows
namespace MaterialConverter {
    // Conversion result from Spec/Gloss to Metal/Rough
    struct ConversionResult {
        Vec3 baseColor;      // Converted base color/albedo
        f32 metallic;        // Metallic factor [0, 1]
        f32 roughness;       // Roughness factor [0, 1]
        bool isMetallic;     // True if material was detected as metallic
    };

    // Convert Specular/Glossiness workflow to Metallic/Roughness
    //
    // Parameters:
    // - diffuseColor: Diffuse/albedo color from Spec/Gloss workflow
    // - specularColor: Specular color (RGB, typically grayscale for dielectrics)
    // - glossiness: Glossiness factor [0, 1] where 1 = perfectly smooth
    //
    // Returns:
    // - ConversionResult with converted baseColor, metallic, and roughness
    //
    // Algorithm:
    // 1. Roughness = 1.0 - glossiness (direct inverse)
    // 2. Detect if metallic based on specular color:
    //    - If specular is achromatic (R≈G≈B) and high (luminance > 0.5) → metallic
    //    - Otherwise → dielectric (metallic = 0)
    // 3. For metals: baseColor = specular color
    //    For dielectrics: baseColor = diffuse color
    ConversionResult ConvertSpecGlossToMetalRough(
        const Vec3& diffuseColor,
        const Vec3& specularColor,
        f32 glossiness);

    // Simplified conversion with fallback for missing data
    // If specular color is not available, assumes dielectric material
    //
    // Parameters:
    // - diffuseColor: Diffuse/albedo color
    // - glossiness: Glossiness factor [0, 1]
    //
    // Returns:
    // - ConversionResult with metallic = 0 and roughness = 1 - glossiness
    ConversionResult ConvertGlossinessOnly(
        const Vec3& diffuseColor,
        f32 glossiness);

    // Helper: Calculate luminance of RGB color (perceived brightness)
    inline f32 Luminance(const Vec3& color) {
        // Standard luminance weights (Rec. 709)
        return 0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b;
    }

    // Helper: Check if color is achromatic (grayscale)
    // threshold: Maximum allowed difference between channels to consider achromatic
    inline bool IsAchromatic(const Vec3& color, f32 threshold = 0.05f) {
        f32 minVal = std::min(std::min(color.r, color.g), color.b);
        f32 maxVal = std::max(std::max(color.r, color.g), color.b);
        return (maxVal - minVal) < threshold;
    }
}
