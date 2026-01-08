#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/reflect/type_registry.hpp>
#include <engine/core/math.hpp>

using namespace engine::reflect;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// Test types for reflection
namespace test_types {

struct SimpleComponent {
    float value = 0.0f;
    int count = 0;
    std::string name;
};

struct TransformLike {
    Vec3 position{0.0f};
    Vec3 scale{1.0f};

    float get_x() const { return position.x; }
    void set_x(float x) { position.x = x; }

    void reset() {
        position = Vec3{0.0f};
        scale = Vec3{1.0f};
    }

    float length() const {
        return glm::length(position);
    }
};

enum class TestEnum : uint8_t {
    Value1 = 0,
    Value2 = 1,
    Value3 = 2
};

struct ComponentWithEnum {
    TestEnum mode = TestEnum::Value1;
    int data = 0;
};

struct UniqueTestType {
    int x = 0;
};

} // namespace test_types

class TypeRegistryFixture {
protected:
    TypeRegistryFixture() {
        // Note: TypeRegistry is a singleton, so we can't fully reset it
        // Tests should be designed to work with accumulated state
    }

    void register_test_types() {
        auto& registry = TypeRegistry::instance();

        // Register SimpleComponent
        registry.register_component<test_types::SimpleComponent>(
            "TestSimpleComponent",
            TypeMeta().set_display_name("Simple Component").set_category(TypeCategory::Component)
        );
        registry.register_property<test_types::SimpleComponent, &test_types::SimpleComponent::value>(
            "value", PropertyMeta().set_display_name("Value").set_range(0.0f, 100.0f)
        );
        registry.register_property<test_types::SimpleComponent, &test_types::SimpleComponent::count>(
            "count", PropertyMeta().set_display_name("Count")
        );
        registry.register_property<test_types::SimpleComponent, &test_types::SimpleComponent::name>(
            "name", PropertyMeta().set_display_name("Name")
        );

        // Register TransformLike
        registry.register_type<test_types::TransformLike>(
            "TestTransformLike",
            TypeMeta().set_display_name("Transform Like")
        );
        registry.register_property<test_types::TransformLike, &test_types::TransformLike::position>(
            "position", PropertyMeta().set_display_name("Position")
        );
        registry.register_property<test_types::TransformLike, &test_types::TransformLike::scale>(
            "scale", PropertyMeta().set_display_name("Scale")
        );
        registry.register_method<test_types::TransformLike, &test_types::TransformLike::reset>("reset");
        registry.register_method<test_types::TransformLike, &test_types::TransformLike::length>("length");

        // Register enum
        registry.register_enum<test_types::TestEnum>("TestEnum", {
            {test_types::TestEnum::Value1, "Value1"},
            {test_types::TestEnum::Value2, "Value2"},
            {test_types::TestEnum::Value3, "Value3"}
        });
    }
};

TEST_CASE_METHOD(TypeRegistryFixture, "TypeRegistry singleton", "[reflect][registry]") {
    auto& registry1 = TypeRegistry::instance();
    auto& registry2 = TypeRegistry::instance();

    REQUIRE(&registry1 == &registry2);
}

TEST_CASE_METHOD(TypeRegistryFixture, "TypeRegistry register and query type", "[reflect][registry]") {
    auto& registry = TypeRegistry::instance();

    // Register a unique type for this test
    registry.register_type<test_types::UniqueTestType>(
        "UniqueTestType",
        TypeMeta().set_display_name("Unique Test")
    );

    SECTION("has_type by name") {
        REQUIRE(registry.has_type("UniqueTestType"));
        REQUIRE_FALSE(registry.has_type("NonExistentType12345"));
    }

    SECTION("find_type by name") {
        auto type = registry.find_type("UniqueTestType");
        REQUIRE(static_cast<bool>(type));
    }

    SECTION("get_type_info") {
        const auto* info = registry.get_type_info("UniqueTestType");
        REQUIRE(info != nullptr);
        REQUIRE(info->name == "UniqueTestType");
        REQUIRE(info->meta.display_name == "Unique Test");
    }
}

TEST_CASE_METHOD(TypeRegistryFixture, "TypeRegistry register component", "[reflect][registry]") {
    register_test_types();
    auto& registry = TypeRegistry::instance();

    SECTION("Component is registered") {
        REQUIRE(registry.has_type("TestSimpleComponent"));
        auto type = registry.find_type("TestSimpleComponent");
        REQUIRE(static_cast<bool>(type));
    }

    SECTION("Component type info has is_component flag") {
        const auto* info = registry.get_type_info("TestSimpleComponent");
        REQUIRE(info != nullptr);
        REQUIRE(info->is_component == true);
    }

    SECTION("Get all component names includes registered component") {
        auto components = registry.get_all_component_names();
        bool found = std::find(components.begin(), components.end(), "TestSimpleComponent") != components.end();
        REQUIRE(found);
    }
}

TEST_CASE_METHOD(TypeRegistryFixture, "TypeRegistry properties", "[reflect][registry]") {
    register_test_types();
    auto& registry = TypeRegistry::instance();

    SECTION("Type info contains properties") {
        const auto* info = registry.get_type_info("TestSimpleComponent");
        REQUIRE(info != nullptr);
        REQUIRE(info->properties.size() >= 3);
    }

    SECTION("Get property info") {
        const auto* prop = registry.get_property_info("TestSimpleComponent", "value");
        REQUIRE(prop != nullptr);
        REQUIRE(prop->name == "value");
        REQUIRE(prop->meta.display_name == "Value");
    }

    SECTION("Property getter works") {
        const auto* prop = registry.get_property_info("TestSimpleComponent", "value");
        REQUIRE(prop != nullptr);

        test_types::SimpleComponent comp;
        comp.value = 42.0f;

        entt::meta_any obj{comp};
        auto result = prop->getter(obj);
        REQUIRE(result);

        auto* value = result.try_cast<float>();
        REQUIRE(value != nullptr);
        REQUIRE_THAT(*value, WithinAbs(42.0f, 0.001f));
    }

    SECTION("Property setter works") {
        const auto* prop = registry.get_property_info("TestSimpleComponent", "value");
        REQUIRE(prop != nullptr);

        test_types::SimpleComponent comp;
        comp.value = 0.0f;

        entt::meta_any obj{comp};
        prop->setter(obj, entt::meta_any{99.0f});

        auto* modified = obj.try_cast<test_types::SimpleComponent>();
        REQUIRE(modified != nullptr);
        REQUIRE_THAT(modified->value, WithinAbs(99.0f, 0.001f));
    }
}

TEST_CASE_METHOD(TypeRegistryFixture, "TypeRegistry methods", "[reflect][registry]") {
    register_test_types();
    auto& registry = TypeRegistry::instance();

    SECTION("Type info contains methods") {
        const auto* info = registry.get_type_info("TestTransformLike");
        REQUIRE(info != nullptr);
        REQUIRE(info->methods.size() >= 2);
    }

    SECTION("Get method info") {
        const auto* method = registry.get_method_info("TestTransformLike", "reset");
        REQUIRE(method != nullptr);
        REQUIRE(method->name == "reset");
    }

    SECTION("Invoke void method") {
        test_types::TransformLike obj;
        obj.position = Vec3{10.0f, 20.0f, 30.0f};

        entt::meta_any any_obj{obj};
        registry.invoke_method(any_obj, "TestTransformLike", "reset");

        auto* modified = any_obj.try_cast<test_types::TransformLike>();
        REQUIRE(modified != nullptr);
        REQUIRE_THAT(modified->position.x, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Invoke method with return value") {
        test_types::TransformLike obj;
        obj.position = Vec3{3.0f, 4.0f, 0.0f};

        entt::meta_any any_obj{obj};
        auto result = registry.invoke_method(any_obj, "TestTransformLike", "length");

        REQUIRE(result);
        auto* length = result.try_cast<float>();
        REQUIRE(length != nullptr);
        REQUIRE_THAT(*length, WithinAbs(5.0f, 0.001f)); // 3-4-5 triangle
    }
}

TEST_CASE_METHOD(TypeRegistryFixture, "TypeRegistry enum registration", "[reflect][registry]") {
    register_test_types();
    auto& registry = TypeRegistry::instance();

    SECTION("Enum is registered") {
        REQUIRE(registry.has_type("TestEnum"));
    }

    SECTION("Enum type info") {
        const auto* info = registry.get_type_info("TestEnum");
        REQUIRE(info != nullptr);
        REQUIRE(info->is_enum == true);
        REQUIRE(info->enum_values.size() == 3);
    }

    SECTION("Enum values are correct") {
        const auto* info = registry.get_type_info("TestEnum");
        REQUIRE(info != nullptr);

        bool found_value1 = false;
        bool found_value2 = false;
        bool found_value3 = false;

        for (const auto& [name, value] : info->enum_values) {
            if (name == "Value1" && value == 0) found_value1 = true;
            if (name == "Value2" && value == 1) found_value2 = true;
            if (name == "Value3" && value == 2) found_value3 = true;
        }

        REQUIRE(found_value1);
        REQUIRE(found_value2);
        REQUIRE(found_value3);
    }
}

TEST_CASE_METHOD(TypeRegistryFixture, "TypeRegistry get all type names", "[reflect][registry]") {
    register_test_types();
    auto& registry = TypeRegistry::instance();

    auto names = registry.get_all_type_names();
    REQUIRE_FALSE(names.empty());

    // Check some of our registered types are present
    bool has_simple = std::find(names.begin(), names.end(), "TestSimpleComponent") != names.end();
    bool has_transform = std::find(names.begin(), names.end(), "TestTransformLike") != names.end();

    REQUIRE(has_simple);
    REQUIRE(has_transform);
}
