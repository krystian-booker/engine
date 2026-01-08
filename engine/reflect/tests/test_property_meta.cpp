#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/reflect/property.hpp>

using namespace engine::reflect;
using Catch::Matchers::WithinAbs;

TEST_CASE("PropertyMeta default values", "[reflect][property]") {
    PropertyMeta meta;

    REQUIRE(meta.name.empty());
    REQUIRE(meta.display_name.empty());
    REQUIRE(meta.category.empty());
    REQUIRE(meta.tooltip.empty());
    REQUIRE_THAT(meta.min_value, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(meta.max_value, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(meta.step, WithinAbs(0.0f, 0.001f));
    REQUIRE(meta.read_only == false);
    REQUIRE(meta.hidden == false);
    REQUIRE(meta.is_angle == false);
    REQUIRE(meta.is_color == false);
    REQUIRE(meta.is_asset == false);
    REQUIRE(meta.is_entity_ref == false);
}

TEST_CASE("PropertyMeta fluent setters", "[reflect][property]") {
    PropertyMeta meta;

    SECTION("set_display_name") {
        meta.set_display_name("My Property");
        REQUIRE(meta.display_name == "My Property");
    }

    SECTION("set_category") {
        meta.set_category("Transform");
        REQUIRE(meta.category == "Transform");
    }

    SECTION("set_tooltip") {
        meta.set_tooltip("This is a helpful tooltip");
        REQUIRE(meta.tooltip == "This is a helpful tooltip");
    }

    SECTION("set_range") {
        meta.set_range(0.0f, 100.0f, 0.1f);
        REQUIRE_THAT(meta.min_value, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(meta.max_value, WithinAbs(100.0f, 0.001f));
        REQUIRE_THAT(meta.step, WithinAbs(0.1f, 0.001f));
    }

    SECTION("set_read_only") {
        meta.set_read_only();
        REQUIRE(meta.read_only == true);

        meta.set_read_only(false);
        REQUIRE(meta.read_only == false);
    }

    SECTION("set_hidden") {
        meta.set_hidden();
        REQUIRE(meta.hidden == true);
    }

    SECTION("set_angle") {
        meta.set_angle();
        REQUIRE(meta.is_angle == true);
    }

    SECTION("set_color") {
        meta.set_color();
        REQUIRE(meta.is_color == true);
    }

    SECTION("set_asset") {
        meta.set_asset("Texture");
        REQUIRE(meta.is_asset == true);
        REQUIRE(meta.asset_type == "Texture");
    }

    SECTION("set_entity_ref") {
        meta.set_entity_ref();
        REQUIRE(meta.is_entity_ref == true);
    }
}

TEST_CASE("PropertyMeta chained setters", "[reflect][property]") {
    auto meta = PropertyMeta()
        .set_display_name("Health")
        .set_category("Stats")
        .set_tooltip("Current health points")
        .set_range(0.0f, 100.0f, 1.0f);

    REQUIRE(meta.display_name == "Health");
    REQUIRE(meta.category == "Stats");
    REQUIRE(meta.tooltip == "Current health points");
    REQUIRE_THAT(meta.min_value, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(meta.max_value, WithinAbs(100.0f, 0.001f));
}

TEST_CASE("TypeCategory enum", "[reflect][type]") {
    REQUIRE(static_cast<uint8_t>(TypeCategory::Unknown) == 0);
    REQUIRE(static_cast<uint8_t>(TypeCategory::Component) == 1);
    REQUIRE(static_cast<uint8_t>(TypeCategory::Resource) == 2);
    REQUIRE(static_cast<uint8_t>(TypeCategory::Event) == 3);
    REQUIRE(static_cast<uint8_t>(TypeCategory::System) == 4);
}

TEST_CASE("TypeMeta default values", "[reflect][type]") {
    TypeMeta meta;

    REQUIRE(meta.name.empty());
    REQUIRE(meta.display_name.empty());
    REQUIRE(meta.description.empty());
    REQUIRE(meta.icon.empty());
    REQUIRE(meta.category == TypeCategory::Unknown);
    REQUIRE(meta.is_component == false);
    REQUIRE(meta.is_abstract == false);
}

TEST_CASE("TypeMeta fluent setters", "[reflect][type]") {
    TypeMeta meta;

    SECTION("set_display_name") {
        meta.set_display_name("Transform Component");
        REQUIRE(meta.display_name == "Transform Component");
    }

    SECTION("set_description") {
        meta.set_description("Represents position, rotation, and scale");
        REQUIRE(meta.description == "Represents position, rotation, and scale");
    }

    SECTION("set_icon") {
        meta.set_icon("transform_icon");
        REQUIRE(meta.icon == "transform_icon");
    }

    SECTION("set_category") {
        meta.set_category(TypeCategory::Component);
        REQUIRE(meta.category == TypeCategory::Component);
    }
}

TEST_CASE("TypeMeta chained setters", "[reflect][type]") {
    auto meta = TypeMeta()
        .set_display_name("Rigid Body")
        .set_description("Physics rigid body component")
        .set_icon("physics_icon")
        .set_category(TypeCategory::Component);

    REQUIRE(meta.display_name == "Rigid Body");
    REQUIRE(meta.description == "Physics rigid body component");
    REQUIRE(meta.icon == "physics_icon");
    REQUIRE(meta.category == TypeCategory::Component);
}
