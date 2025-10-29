#include "resources/material_converter.h"
#include <algorithm>

namespace MaterialConverter {

ConversionResult ConvertSpecGlossToMetalRough(
    const Vec3& diffuseColor,
    const Vec3& specularColor,
    f32 glossiness)
{
    ConversionResult result;

    // Step 1: Convert glossiness to roughness (inverse relationship)
    result.roughness = 1.0f - glossiness;
    result.roughness = std::clamp(result.roughness, 0.0f, 1.0f);

    // Step 2: Detect if material is metallic based on specular color
    // Metals have:
    // - Achromatic (grayscale) specular color
    // - High specular intensity (luminance > 0.5)
    // - No diffuse contribution (or very little)

    f32 specularLuminance = Luminance(specularColor);
    bool isAchromatic = IsAchromatic(specularColor, 0.05f);
    bool isHighSpecular = specularLuminance > 0.5f;

    // Determine metallic factor
    if (isAchromatic && isHighSpecular) {
        // Likely metallic - use specular luminance as metallic factor
        result.metallic = std::clamp(specularLuminance, 0.0f, 1.0f);
        result.isMetallic = true;

        // For metals, baseColor should preserve specular color
        // Mix between diffuse and specular based on metallic factor
        result.baseColor = glm::mix(diffuseColor, specularColor, result.metallic);
    } else {
        // Dielectric material (non-metallic)
        result.metallic = 0.0f;
        result.isMetallic = false;

        // For dielectrics, baseColor is the diffuse color
        result.baseColor = diffuseColor;
    }

    // Ensure baseColor is in valid range [0, 1]
    result.baseColor.r = std::clamp(result.baseColor.r, 0.0f, 1.0f);
    result.baseColor.g = std::clamp(result.baseColor.g, 0.0f, 1.0f);
    result.baseColor.b = std::clamp(result.baseColor.b, 0.0f, 1.0f);

    return result;
}

ConversionResult ConvertGlossinessOnly(
    const Vec3& diffuseColor,
    f32 glossiness)
{
    ConversionResult result;

    // Simple conversion: assume dielectric material
    result.metallic = 0.0f;
    result.isMetallic = false;
    result.roughness = 1.0f - glossiness;
    result.roughness = std::clamp(result.roughness, 0.0f, 1.0f);

    // Base color is just the diffuse color
    result.baseColor = diffuseColor;
    result.baseColor.r = std::clamp(result.baseColor.r, 0.0f, 1.0f);
    result.baseColor.g = std::clamp(result.baseColor.g, 0.0f, 1.0f);
    result.baseColor.b = std::clamp(result.baseColor.b, 0.0f, 1.0f);

    return result;
}

} // namespace MaterialConverter
