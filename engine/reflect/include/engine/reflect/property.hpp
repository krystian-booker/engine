#pragma once

#include <string>
#include <cstdint>

namespace engine::reflect {

// Property metadata for editor/serialization
struct PropertyMeta {
    std::string name;
    std::string display_name;
    std::string category;
    std::string tooltip;

    // Value constraints
    float min_value = 0.0f;
    float max_value = 0.0f;
    float step = 0.0f;

    // Display hints
    bool read_only = false;
    bool hidden = false;
    bool is_angle = false;    // Display in degrees, store in radians
    bool is_color = false;    // Use color picker
    bool is_asset = false;    // Show asset picker

    // For asset references
    std::string asset_type;   // e.g., "Mesh", "Texture", "Material"

    PropertyMeta() = default;

    PropertyMeta& set_display_name(const std::string& name) { display_name = name; return *this; }
    PropertyMeta& set_category(const std::string& cat) { category = cat; return *this; }
    PropertyMeta& set_tooltip(const std::string& tip) { tooltip = tip; return *this; }
    PropertyMeta& set_range(float min, float max, float s = 0.0f) {
        min_value = min; max_value = max; step = s; return *this;
    }
    PropertyMeta& set_read_only(bool ro = true) { read_only = ro; return *this; }
    PropertyMeta& set_hidden(bool h = true) { hidden = h; return *this; }
    PropertyMeta& set_angle(bool a = true) { is_angle = a; return *this; }
    PropertyMeta& set_color(bool c = true) { is_color = c; return *this; }
    PropertyMeta& set_asset(const std::string& type) { is_asset = true; asset_type = type; return *this; }
};

// Type categories for grouping in editor
enum class TypeCategory : uint8_t {
    Unknown = 0,
    Component,
    Resource,
    Event,
    System
};

// Type metadata
struct TypeMeta {
    std::string name;
    std::string display_name;
    std::string description;
    std::string icon;          // Icon name for editor
    TypeCategory category = TypeCategory::Unknown;
    bool is_component = false;
    bool is_abstract = false;

    TypeMeta() = default;

    TypeMeta& set_display_name(const std::string& name) { display_name = name; return *this; }
    TypeMeta& set_description(const std::string& desc) { description = desc; return *this; }
    TypeMeta& set_icon(const std::string& i) { icon = i; return *this; }
    TypeMeta& set_category(TypeCategory cat) { category = cat; return *this; }
};

} // namespace engine::reflect
