#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/data/data_table.hpp>

using namespace engine::data;
namespace core = engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("DataValue type construction", "[data][value]") {
    SECTION("Default construction is null") {
        DataValue v;
        REQUIRE(v.is_null());
        REQUIRE(v.type() == DataValue::Type::Null);
    }

    SECTION("Bool construction") {
        DataValue v_true(true);
        DataValue v_false(false);

        REQUIRE(v_true.is_bool());
        REQUIRE(v_true.type() == DataValue::Type::Bool);
        REQUIRE(v_true.as_bool() == true);

        REQUIRE(v_false.as_bool() == false);
    }

    SECTION("Int construction") {
        DataValue v(42);
        REQUIRE(v.is_int());
        REQUIRE(v.type() == DataValue::Type::Int);
        REQUIRE(v.as_int() == 42);

        DataValue v64(int64_t(1234567890123LL));
        REQUIRE(v64.as_int() == 1234567890123LL);
    }

    SECTION("Float construction") {
        DataValue v_double(3.14159);
        REQUIRE(v_double.is_float());
        REQUIRE(v_double.type() == DataValue::Type::Float);
        REQUIRE_THAT(v_double.as_float(), WithinAbs(3.14159, 0.00001));

        DataValue v_float(2.5f);
        REQUIRE(v_float.is_float());
        REQUIRE_THAT(v_float.as_float(), WithinAbs(2.5, 0.00001));
    }

    SECTION("String construction") {
        DataValue v_str("hello");
        REQUIRE(v_str.is_string());
        REQUIRE(v_str.type() == DataValue::Type::String);
        REQUIRE(v_str.as_string() == "hello");

        std::string s = "world";
        DataValue v_str2(s);
        REQUIRE(v_str2.as_string() == "world");

        DataValue v_str3(std::string("moved"));
        REQUIRE(v_str3.as_string() == "moved");
    }

    SECTION("UUID/Asset construction") {
        engine::core::UUID uuid;
        DataValue v(uuid);
        REQUIRE(v.is_asset());
        REQUIRE(v.type() == DataValue::Type::AssetId);
    }
}

TEST_CASE("DataValue type checking", "[data][value]") {
    DataValue v_null;
    DataValue v_bool(true);
    DataValue v_int(42);
    DataValue v_float(3.14);
    DataValue v_string("test");

    SECTION("is_numeric") {
        REQUIRE_FALSE(v_null.is_numeric());
        REQUIRE_FALSE(v_bool.is_numeric());
        REQUIRE(v_int.is_numeric());
        REQUIRE(v_float.is_numeric());
        REQUIRE_FALSE(v_string.is_numeric());
    }
}

TEST_CASE("DataValue type-checked getters throw on mismatch", "[data][value]") {
    DataValue v_int(42);

    REQUIRE_THROWS(v_int.as_bool());
    REQUIRE_THROWS(v_int.as_string());
    REQUIRE_NOTHROW(v_int.as_int());
    REQUIRE_NOTHROW(v_int.as_float()); // Int can convert to float
}

TEST_CASE("DataValue safe getters with defaults", "[data][value]") {
    DataValue v_int(42);
    DataValue v_null;

    SECTION("get_bool with default") {
        REQUIRE(v_int.get_bool(true) == true);  // Returns default (not a bool)
        REQUIRE(v_null.get_bool(false) == false);
    }

    SECTION("get_int with default") {
        REQUIRE(v_int.get_int(0) == 42);
        REQUIRE(v_null.get_int(99) == 99);
    }

    SECTION("get_float with default") {
        DataValue v_float(3.14);
        REQUIRE_THAT(v_float.get_float(0.0), WithinAbs(3.14, 0.001));
        REQUIRE_THAT(v_int.get_float(0.0), WithinAbs(42.0, 0.001)); // Int converts
        REQUIRE_THAT(v_null.get_float(1.5), WithinAbs(1.5, 0.001));
    }

    SECTION("get_string with default") {
        DataValue v_str("hello");
        REQUIRE(v_str.get_string("default") == "hello");
        REQUIRE(v_null.get_string("default") == "default");
    }
}

TEST_CASE("DataValue to_string conversion", "[data][value]") {
    REQUIRE(DataValue().to_string() == "null");
    REQUIRE(DataValue(true).to_string() == "true");
    REQUIRE(DataValue(false).to_string() == "false");
    REQUIRE(DataValue(42).to_string() == "42");
    REQUIRE(DataValue("hello").to_string() == "hello");
    // Float formatting may vary, just check it doesn't crash
    REQUIRE_FALSE(DataValue(3.14).to_string().empty());
}
