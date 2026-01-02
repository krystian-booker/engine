#pragma once

#include <engine/ui/ui_types.hpp>
#include <engine/render/types.hpp>
#include <bgfx/bgfx.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace engine::ui {

// Glyph metrics
struct GlyphInfo {
    uint32_t codepoint;
    float x0, y0, x1, y1;      // Texture coordinates (normalized)
    float offset_x, offset_y;   // Offset from cursor position
    float width, height;        // Size in pixels
    float advance;              // Horizontal advance
};

// Font metrics
struct FontMetrics {
    float ascent;       // Distance from baseline to top
    float descent;      // Distance from baseline to bottom (negative)
    float line_height;  // Recommended line spacing
    float cap_height;   // Height of capital letters
    float x_height;     // Height of lowercase letters
};

// Font atlas (texture containing all glyphs)
class FontAtlas {
public:
    FontAtlas();
    ~FontAtlas();

    bool load_from_ttf(const std::string& path, float size_pixels, const std::string& charset = "");
    void shutdown();

    render::TextureHandle get_texture() const {
        render::TextureHandle handle;
        handle.id = bgfx::isValid(m_texture) ? m_texture.idx : UINT32_MAX;
        return handle;
    }
    int get_texture_width() const { return m_width; }
    int get_texture_height() const { return m_height; }

    const GlyphInfo* get_glyph(uint32_t codepoint) const;
    float get_kerning(uint32_t left, uint32_t right) const;

    const FontMetrics& get_metrics() const { return m_metrics; }
    float get_font_size() const { return m_font_size; }

private:
    bgfx::TextureHandle m_texture = BGFX_INVALID_HANDLE;
    int m_width = 0;
    int m_height = 0;
    float m_font_size = 0.0f;

    FontMetrics m_metrics;
    std::unordered_map<uint32_t, GlyphInfo> m_glyphs;
    std::unordered_map<uint64_t, float> m_kerning;  // (left << 32 | right) -> kerning

    std::vector<uint8_t> m_ttf_data;  // Keep font data for additional operations
};

// Text layout result
struct TextLayoutGlyph {
    const GlyphInfo* glyph;
    float x, y;          // Position
    float width, height; // Size
};

struct TextLayout {
    std::vector<TextLayoutGlyph> glyphs;
    float width;         // Total width
    float height;        // Total height
    int line_count;      // Number of lines
};

// Font manager - handles font loading and caching
class FontManager {
public:
    FontManager();
    ~FontManager();

    void init();
    void shutdown();

    // Load font from file
    FontHandle load_font(const std::string& path, float size_pixels);

    // Get font atlas
    FontAtlas* get_font(FontHandle handle);
    const FontAtlas* get_font(FontHandle handle) const;

    // Get default font
    FontHandle get_default_font() const { return m_default_font; }
    void set_default_font(FontHandle font) { m_default_font = font; }

    // Layout text (compute glyph positions)
    TextLayout layout_text(FontHandle font, const std::string& text,
                           float max_width = 0.0f, bool wrap = false);

    // Measure text dimensions
    Vec2 measure_text(FontHandle font, const std::string& text,
                      float max_width = 0.0f, bool wrap = false);

private:
    std::unordered_map<FontHandle, std::unique_ptr<FontAtlas>> m_fonts;
    FontHandle m_next_handle = 1;
    FontHandle m_default_font = INVALID_FONT_HANDLE;
};

} // namespace engine::ui
