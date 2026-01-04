#include <engine/ui/ui_rich_text.hpp>
#include <unordered_map>
#include <algorithm>
#include <cstdlib>

namespace engine::ui {

namespace {

// Named color map
const std::unordered_map<std::string, Vec4> s_named_colors = {
    {"red",       Vec4(1.0f, 0.0f, 0.0f, 1.0f)},
    {"green",     Vec4(0.0f, 1.0f, 0.0f, 1.0f)},
    {"blue",      Vec4(0.0f, 0.0f, 1.0f, 1.0f)},
    {"yellow",    Vec4(1.0f, 1.0f, 0.0f, 1.0f)},
    {"white",     Vec4(1.0f, 1.0f, 1.0f, 1.0f)},
    {"black",     Vec4(0.0f, 0.0f, 0.0f, 1.0f)},
    {"gray",      Vec4(0.5f, 0.5f, 0.5f, 1.0f)},
    {"grey",      Vec4(0.5f, 0.5f, 0.5f, 1.0f)},
    {"orange",    Vec4(1.0f, 0.65f, 0.0f, 1.0f)},
    {"purple",    Vec4(0.5f, 0.0f, 0.5f, 1.0f)},
    {"cyan",      Vec4(0.0f, 1.0f, 1.0f, 1.0f)},
    {"magenta",   Vec4(1.0f, 0.0f, 1.0f, 1.0f)},
    {"pink",      Vec4(1.0f, 0.75f, 0.8f, 1.0f)},
    {"brown",     Vec4(0.6f, 0.3f, 0.0f, 1.0f)},
    {"gold",      Vec4(1.0f, 0.84f, 0.0f, 1.0f)},
    {"silver",    Vec4(0.75f, 0.75f, 0.75f, 1.0f)},
};

std::string to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

} // anonymous namespace

Vec4 RichTextParser::parse_color(const std::string& color_str) {
    if (color_str.empty()) {
        return Vec4(1.0f);
    }

    // Check for hex color
    if (color_str[0] == '#') {
        std::string hex = color_str.substr(1);
        unsigned long value = 0;
        try {
            value = std::stoul(hex, nullptr, 16);
        } catch (...) {
            return Vec4(1.0f);
        }

        if (hex.length() == 6) {
            // #RRGGBB
            return Vec4(
                ((value >> 16) & 0xFF) / 255.0f,
                ((value >> 8) & 0xFF) / 255.0f,
                (value & 0xFF) / 255.0f,
                1.0f
            );
        } else if (hex.length() == 8) {
            // #RRGGBBAA
            return Vec4(
                ((value >> 24) & 0xFF) / 255.0f,
                ((value >> 16) & 0xFF) / 255.0f,
                ((value >> 8) & 0xFF) / 255.0f,
                (value & 0xFF) / 255.0f
            );
        } else if (hex.length() == 3) {
            // #RGB shorthand
            return Vec4(
                ((value >> 8) & 0xF) / 15.0f,
                ((value >> 4) & 0xF) / 15.0f,
                (value & 0xF) / 15.0f,
                1.0f
            );
        }
    }

    // Check named colors
    auto it = s_named_colors.find(to_lower(color_str));
    if (it != s_named_colors.end()) {
        return it->second;
    }

    return Vec4(1.0f);
}

std::string RichTextParser::escape(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (char c : text) {
        if (c == '<') {
            result += "&lt;";
        } else if (c == '>') {
            result += "&gt;";
        } else if (c == '&') {
            result += "&amp;";
        } else {
            result += c;
        }
    }
    return result;
}

void RichTextParser::ParseState::flush_text() {
    if (!current_text.empty()) {
        RichTextRun run;
        run.text = current_text;
        run.style = current_style();
        runs.push_back(run);
        current_text.clear();
    }
}

void RichTextParser::ParseState::push_style() {
    style_stack.push_back(current_style());
}

void RichTextParser::ParseState::pop_style() {
    if (style_stack.size() > 1) {
        style_stack.pop_back();
    }
}

RichTextStyle& RichTextParser::ParseState::current_style() {
    return style_stack.back();
}

void RichTextParser::parse_tag(const std::string& tag, ParseState& state) {
    if (tag.empty()) return;

    // Closing tag
    if (tag[0] == '/') {
        state.flush_text();
        state.pop_style();
        return;
    }

    // Opening tag
    state.flush_text();
    state.push_style();

    std::string tag_name = tag;
    std::string tag_value;

    // Check for attribute
    size_t eq_pos = tag.find('=');
    if (eq_pos != std::string::npos) {
        tag_name = tag.substr(0, eq_pos);
        tag_value = tag.substr(eq_pos + 1);
    }

    tag_name = to_lower(tag_name);

    if (tag_name == "b" || tag_name == "bold") {
        state.current_style().bold = true;
    } else if (tag_name == "i" || tag_name == "italic") {
        state.current_style().italic = true;
    } else if (tag_name == "u" || tag_name == "underline") {
        state.current_style().underline = true;
    } else if (tag_name == "s" || tag_name == "strike" || tag_name == "strikethrough") {
        state.current_style().strikethrough = true;
    } else if (tag_name == "color" || tag_name == "c") {
        state.current_style().color = parse_color(tag_value);
    } else if (tag_name == "size") {
        try {
            state.current_style().font_size = std::stof(tag_value);
        } catch (...) {
            // Ignore invalid size
        }
    }
}

RichTextLayout RichTextParser::parse(const std::string& markup,
                                      const RichTextStyle& base_style,
                                      FontManager* font_manager,
                                      float max_width,
                                      bool wrap) {
    RichTextLayout layout;

    ParseState state;
    state.style_stack.push_back(base_style);
    state.font_manager = font_manager;

    size_t pos = 0;
    while (pos < markup.length()) {
        char c = markup[pos];

        if (c == '<') {
            // Find closing >
            size_t end = markup.find('>', pos);
            if (end == std::string::npos) {
                // Malformed tag, treat as literal text
                state.current_text += c;
                ++pos;
                continue;
            }

            std::string tag = markup.substr(pos + 1, end - pos - 1);
            parse_tag(tag, state);
            pos = end + 1;
        } else if (c == '&') {
            // HTML entity
            if (markup.compare(pos, 4, "&lt;") == 0) {
                state.current_text += '<';
                pos += 4;
            } else if (markup.compare(pos, 4, "&gt;") == 0) {
                state.current_text += '>';
                pos += 4;
            } else if (markup.compare(pos, 5, "&amp;") == 0) {
                state.current_text += '&';
                pos += 5;
            } else {
                state.current_text += c;
                ++pos;
            }
        } else {
            state.current_text += c;
            ++pos;
        }
    }

    // Flush remaining text
    state.flush_text();

    // Transfer runs to layout
    layout.runs = std::move(state.runs);

    // Compute layout positions
    layout_runs(layout, font_manager, max_width, wrap);

    return layout;
}

void RichTextParser::layout_runs(RichTextLayout& layout, FontManager* font_manager,
                                  float max_width, bool wrap) {
    if (!font_manager || layout.runs.empty()) {
        return;
    }

    float x = 0.0f;
    float y = 0.0f;
    float line_height = 0.0f;
    float max_x = 0.0f;
    layout.line_count = 1;

    for (auto& run : layout.runs) {
        if (run.text.empty()) continue;

        // Get font metrics
        FontHandle font = run.style.font;
        if (font == INVALID_FONT_HANDLE) {
            font = font_manager->get_default_font();
        }

        FontAtlas* atlas = font_manager->get_font(font);
        if (!atlas) continue;

        // Calculate run dimensions
        Vec2 size = font_manager->measure_text(font, run.text);

        // Scale by font size ratio
        float scale = run.style.font_size / atlas->get_font_size();
        size.x *= scale;
        size.y *= scale;

        run.width = size.x;
        run.height = size.y;

        // Word wrap if needed
        if (wrap && max_width > 0.0f && x + run.width > max_width && x > 0.0f) {
            x = 0.0f;
            y += line_height;
            line_height = 0.0f;
            layout.line_count++;
        }

        run.x = x;
        run.y = y;

        x += run.width;
        line_height = std::max(line_height, run.height);
        max_x = std::max(max_x, x);
    }

    layout.total_width = max_x;
    layout.total_height = y + line_height;
}

} // namespace engine::ui
