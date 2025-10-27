#pragma once
#include "core/resource_manager.h"
#include "core/texture_data.h"
#include "core/texture_load_options.h"
#include "core/math.h"

// Global texture configuration
namespace TextureConfig {
    extern u32 g_DefaultAnisotropy;
    void SetDefaultAnisotropy(u32 level);
    u32 GetDefaultAnisotropy();
}

// Texture resource manager (singleton)
class TextureManager : public ResourceManager<TextureData, TextureHandle> {
public:
    // Singleton access
    static TextureManager& Instance() {
        static TextureManager instance;
        return instance;
    }

    // Load texture from file with options
    TextureHandle Load(const std::string& filepath, const TextureLoadOptions& options);

    // Load with default options (inferred from file extension or usage)
    TextureHandle Load(const std::string& filepath) {
        return ResourceManager::Load(filepath);
    }

    // Create procedural textures
    TextureHandle CreateSolid(u32 width, u32 height, const Vec4& color, TextureUsage usage = TextureUsage::Generic);
    TextureHandle CreateWhite();       // 1x1 white (for default albedo)
    TextureHandle CreateBlack();       // 1x1 black
    TextureHandle CreateNormalMap();   // 1x1 (0.5, 0.5, 1.0) - neutral normal in tangent space

protected:
    // Override ResourceManager::LoadResource to use default options
    std::unique_ptr<TextureData> LoadResource(const std::string& filepath) override;

private:
    TextureManager();
    ~TextureManager() = default;

    // Prevent copying
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    // Helper to create a single-pixel texture
    TextureHandle CreateSinglePixel(u8 r, u8 g, u8 b, u8 a, TextureUsage usage);

    // Default load options
    TextureLoadOptions m_DefaultOptions;

    // Cache for default textures
    TextureHandle m_WhiteTexture;
    TextureHandle m_BlackTexture;
    TextureHandle m_NormalMapTexture;
};
