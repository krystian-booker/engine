#include <engine/ui/ui_font.hpp>
#include <engine/core/logging.hpp>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <bgfx/bgfx.h>
#include <fstream>
#include <algorithm>

namespace engine::ui {

FontAtlas::FontAtlas() = default;
FontAtlas::~FontAtlas() {
    shutdown();
}

bool FontAtlas::load_from_ttf(const std::string& path, float size_pixels, const std::string& charset) {
    // Read TTF file
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        core::log_error("FontAtlas: Failed to open font file: {}", path);
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    m_ttf_data.resize(file_size);
    file.read(reinterpret_cast<char*>(m_ttf_data.data()), file_size);

    // Initialize stb_truetype
    stbtt_fontinfo font_info;
    if (!stbtt_InitFont(&font_info, m_ttf_data.data(), 0)) {
        core::log_error("FontAtlas: Failed to initialize font");
        return false;
    }

    m_font_size = size_pixels;

    // Get font metrics
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&font_info, &ascent, &descent, &line_gap);

    float scale = stbtt_ScaleForPixelHeight(&font_info, size_pixels);

    m_metrics.ascent = ascent * scale;
    m_metrics.descent = descent * scale;
    m_metrics.line_height = (ascent - descent + line_gap) * scale;
    m_metrics.cap_height = size_pixels * 0.7f;  // Approximate
    m_metrics.x_height = size_pixels * 0.5f;    // Approximate

    // Determine which characters to include
    std::string chars_to_pack = charset;
    if (chars_to_pack.empty()) {
        // Default: ASCII printable characters
        for (int c = 32; c < 127; ++c) {
            chars_to_pack += static_cast<char>(c);
        }
    }

    // Calculate atlas size
    // Start with a reasonable size and grow if needed
    m_width = 512;
    m_height = 512;

    // Pack glyphs into atlas
    std::vector<stbtt_packedchar> packed_chars(chars_to_pack.size());
    std::vector<uint8_t> atlas_data(m_width * m_height);

    stbtt_pack_context pack_ctx;
    if (!stbtt_PackBegin(&pack_ctx, atlas_data.data(), m_width, m_height, 0, 1, nullptr)) {
        core::log_error("FontAtlas: Failed to begin packing");
        return false;
    }

    stbtt_PackSetOversampling(&pack_ctx, 2, 2);

    // Pack characters
    stbtt_pack_range range;
    range.font_size = size_pixels;
    range.first_unicode_codepoint_in_range = 0;
    range.array_of_unicode_codepoints = nullptr;
    range.num_chars = static_cast<int>(chars_to_pack.size());
    range.chardata_for_range = packed_chars.data();

    // Create codepoint array
    std::vector<int> codepoints(chars_to_pack.size());
    for (size_t i = 0; i < chars_to_pack.size(); ++i) {
        codepoints[i] = static_cast<unsigned char>(chars_to_pack[i]);
    }
    range.array_of_unicode_codepoints = codepoints.data();

    if (!stbtt_PackFontRanges(&pack_ctx, m_ttf_data.data(), 0, &range, 1)) {
        stbtt_PackEnd(&pack_ctx);
        core::log_error("FontAtlas: Failed to pack font ranges");
        return false;
    }

    stbtt_PackEnd(&pack_ctx);

    // Store glyph info
    float inv_w = 1.0f / m_width;
    float inv_h = 1.0f / m_height;

    for (size_t i = 0; i < chars_to_pack.size(); ++i) {
        uint32_t codepoint = static_cast<unsigned char>(chars_to_pack[i]);
        const stbtt_packedchar& pc = packed_chars[i];

        GlyphInfo glyph;
        glyph.codepoint = codepoint;
        glyph.x0 = pc.x0 * inv_w;
        glyph.y0 = pc.y0 * inv_h;
        glyph.x1 = pc.x1 * inv_w;
        glyph.y1 = pc.y1 * inv_h;
        glyph.offset_x = pc.xoff;
        glyph.offset_y = pc.yoff;
        glyph.width = static_cast<float>(pc.x1 - pc.x0);
        glyph.height = static_cast<float>(pc.y1 - pc.y0);
        glyph.advance = pc.xadvance;

        m_glyphs[codepoint] = glyph;
    }

    // Create texture
    m_texture = bgfx::createTexture2D(
        static_cast<uint16_t>(m_width),
        static_cast<uint16_t>(m_height),
        false, 1, bgfx::TextureFormat::R8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT,
        bgfx::copy(atlas_data.data(), static_cast<uint32_t>(atlas_data.size())));

    if (!bgfx::isValid(m_texture)) {
        core::log_error("FontAtlas: Failed to create texture");
        return false;
    }

    core::log_info("FontAtlas: Loaded {} with {} glyphs", path, m_glyphs.size());
    return true;
}

void FontAtlas::shutdown() {
    if (bgfx::isValid(m_texture)) {
        bgfx::destroy(m_texture);
        m_texture = BGFX_INVALID_HANDLE;
    }
    m_glyphs.clear();
    m_kerning.clear();
    m_ttf_data.clear();
}

const GlyphInfo* FontAtlas::get_glyph(uint32_t codepoint) const {
    auto it = m_glyphs.find(codepoint);
    return it != m_glyphs.end() ? &it->second : nullptr;
}

float FontAtlas::get_kerning(uint32_t left, uint32_t right) const {
    uint64_t key = (static_cast<uint64_t>(left) << 32) | right;
    auto it = m_kerning.find(key);
    return it != m_kerning.end() ? it->second : 0.0f;
}

// FontManager implementation

FontManager::FontManager() = default;
FontManager::~FontManager() {
    shutdown();
}

void FontManager::init() {
    // Could load a default embedded font here
}

void FontManager::shutdown() {
    m_fonts.clear();
    m_next_handle = 1;
    m_default_font = INVALID_FONT_HANDLE;
}

FontHandle FontManager::load_font(const std::string& path, float size_pixels) {
    auto atlas = std::make_unique<FontAtlas>();

    if (!atlas->load_from_ttf(path, size_pixels)) {
        return INVALID_FONT_HANDLE;
    }

    FontHandle handle = m_next_handle++;
    m_fonts[handle] = std::move(atlas);

    // Set as default if first font loaded
    if (m_default_font == INVALID_FONT_HANDLE) {
        m_default_font = handle;
    }

    return handle;
}

FontAtlas* FontManager::get_font(FontHandle handle) {
    auto it = m_fonts.find(handle);
    return it != m_fonts.end() ? it->second.get() : nullptr;
}

const FontAtlas* FontManager::get_font(FontHandle handle) const {
    auto it = m_fonts.find(handle);
    return it != m_fonts.end() ? it->second.get() : nullptr;
}

TextLayout FontManager::layout_text(FontHandle font, const std::string& text,
                                     float max_width, bool wrap) {
    TextLayout layout;
    layout.width = 0;
    layout.height = 0;
    layout.line_count = 1;

    const FontAtlas* atlas = get_font(font);
    if (!atlas) {
        atlas = get_font(m_default_font);
        if (!atlas) return layout;
    }

    float x = 0;
    float y = atlas->get_metrics().ascent;
    float line_height = atlas->get_metrics().line_height;
    float max_line_width = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];

        // Handle newlines
        if (c == '\n') {
            max_line_width = std::max(max_line_width, x);
            x = 0;
            y += line_height;
            layout.line_count++;
            continue;
        }

        const GlyphInfo* glyph = atlas->get_glyph(static_cast<uint32_t>(c));
        if (!glyph) {
            glyph = atlas->get_glyph('?');  // Fallback
            if (!glyph) continue;
        }

        // Check for word wrap
        if (wrap && max_width > 0 && x + glyph->advance > max_width && x > 0) {
            max_line_width = std::max(max_line_width, x);
            x = 0;
            y += line_height;
            layout.line_count++;
        }

        // Add glyph to layout
        TextLayoutGlyph lg;
        lg.glyph = glyph;
        lg.x = x + glyph->offset_x;
        lg.y = y + glyph->offset_y;
        lg.width = glyph->width;
        lg.height = glyph->height;
        layout.glyphs.push_back(lg);

        // Apply kerning
        if (i + 1 < text.size()) {
            x += atlas->get_kerning(c, text[i + 1]);
        }

        x += glyph->advance;
    }

    max_line_width = std::max(max_line_width, x);
    layout.width = max_line_width;
    layout.height = y + (line_height - atlas->get_metrics().ascent);

    return layout;
}

Vec2 FontManager::measure_text(FontHandle font, const std::string& text,
                                float max_width, bool wrap) {
    TextLayout layout = layout_text(font, text, max_width, wrap);
    return Vec2(layout.width, layout.height);
}

} // namespace engine::ui
