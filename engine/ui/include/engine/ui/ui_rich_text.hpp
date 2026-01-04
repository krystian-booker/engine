#pragma once

#include <engine/ui/ui_types.hpp>
#include <engine/ui/ui_font.hpp>
#include <string>
#include <vector>
#include <functional>

namespace engine::ui {

class FontManager;

// Style applied to a text run
struct RichTextStyle {
    FontHandle font = INVALID_FONT_HANDLE;
    float font_size = 14.0f;
    Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikethrough = false;
};

// A run of text with consistent styling
struct RichTextRun {
    std::string text;
    RichTextStyle style;
    float x = 0.0f;      // Computed X position
    float y = 0.0f;      // Computed Y position
    float width = 0.0f;  // Computed width
    float height = 0.0f; // Computed height (line height)
};

// Parsed and laid out rich text result
struct RichTextLayout {
    std::vector<RichTextRun> runs;
    float total_width = 0.0f;
    float total_height = 0.0f;
    int line_count = 1;
};

// Rich text parser
// Supported tags:
//   <b>bold</b>
//   <i>italic</i>
//   <u>underline</u>
//   <s>strikethrough</s>
//   <color=red>named color</color>
//   <color=#FF0000>hex color</color>
//   <color=#FF0000FF>hex color with alpha</color>
//   <size=20>font size</size>
class RichTextParser {
public:
    // Parse markup and compute layout
    static RichTextLayout parse(const std::string& markup,
                                 const RichTextStyle& base_style,
                                 FontManager* font_manager,
                                 float max_width = 0.0f,
                                 bool wrap = false);

    // Parse a color string (named or hex)
    static Vec4 parse_color(const std::string& color_str);

    // Escape special characters in text for literal display
    static std::string escape(const std::string& text);

private:
    struct ParseState {
        std::vector<RichTextStyle> style_stack;
        std::vector<RichTextRun> runs;
        std::string current_text;
        FontManager* font_manager;

        void flush_text();
        void push_style();
        void pop_style();
        RichTextStyle& current_style();
    };

    static void parse_tag(const std::string& tag, ParseState& state);
    static void layout_runs(RichTextLayout& layout, FontManager* font_manager,
                            float max_width, bool wrap);
};

} // namespace engine::ui
