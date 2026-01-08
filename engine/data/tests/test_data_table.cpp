#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/data/data_table.hpp>

using namespace engine::data;
using Catch::Matchers::WithinAbs;

TEST_CASE("DataTable construction and schema", "[data][table]") {
    DataTable table;

    SECTION("Empty table") {
        REQUIRE(table.empty());
        REQUIRE(table.row_count() == 0);
        REQUIRE(table.get_columns().empty());
    }

    SECTION("Define columns") {
        table.define_column("id", DataValue::Type::String);
        table.define_column("name", DataValue::Type::String);
        table.define_column("value", DataValue::Type::Int);

        REQUIRE(table.get_columns().size() == 3);
        REQUIRE(table.has_column("id"));
        REQUIRE(table.has_column("name"));
        REQUIRE(table.has_column("value"));
        REQUIRE_FALSE(table.has_column("nonexistent"));
    }

    SECTION("Set ID column") {
        table.define_column("item_id", DataValue::Type::String);
        table.set_id_column("item_id");
        REQUIRE(table.get_id_column() == "item_id");
    }

    SECTION("Column index lookup") {
        table.define_column("col_a", DataValue::Type::String);
        table.define_column("col_b", DataValue::Type::Int);
        table.define_column("col_c", DataValue::Type::Float);

        REQUIRE(table.get_column_index("col_a") == 0);
        REQUIRE(table.get_column_index("col_b") == 1);
        REQUIRE(table.get_column_index("col_c") == 2);
    }
}

TEST_CASE("DataTable load CSV string", "[data][table][csv]") {
    DataTable table;

    SECTION("Simple CSV") {
        std::string csv = R"(id,name,score
item1,Sword,100
item2,Shield,50
item3,Potion,25)";

        REQUIRE(table.load_csv_string(csv));
        REQUIRE(table.row_count() == 3);
        REQUIRE(table.has_column("id"));
        REQUIRE(table.has_column("name"));
        REQUIRE(table.has_column("score"));
    }

    SECTION("CSV with typed columns") {
        table.define_column("id", DataValue::Type::String);
        table.define_column("count", DataValue::Type::Int);
        table.define_column("price", DataValue::Type::Float);
        table.set_id_column("id");

        std::string csv = R"(id,count,price
item1,10,9.99
item2,5,19.99)";

        REQUIRE(table.load_csv_string(csv));
        REQUIRE(table.row_count() == 2);

        auto row = table.get_row(0);
        REQUIRE(row.get_string("id") == "item1");
        REQUIRE(row.get_int("count") == 10);
        REQUIRE_THAT(row.get_float("price"), WithinAbs(9.99, 0.001));
    }
}

TEST_CASE("DataTable load JSON string", "[data][table][json]") {
    DataTable table;

    SECTION("JSON array of objects") {
        std::string json = R"([
            {"id": "item1", "name": "Sword", "damage": 50},
            {"id": "item2", "name": "Shield", "defense": 30}
        ])";

        REQUIRE(table.load_json_string(json));
        REQUIRE(table.row_count() == 2);
    }

    SECTION("JSON with typed columns") {
        table.define_column("id", DataValue::Type::String);
        table.define_column("enabled", DataValue::Type::Bool);
        table.define_column("value", DataValue::Type::Int);
        table.set_id_column("id");

        std::string json = R"([
            {"id": "setting1", "enabled": true, "value": 100},
            {"id": "setting2", "enabled": false, "value": 200}
        ])";

        REQUIRE(table.load_json_string(json));
        REQUIRE(table.row_count() == 2);

        auto row = table.get_row(0);
        REQUIRE(row.get_bool("enabled") == true);
        REQUIRE(row.get_int("value") == 100);
    }
}

TEST_CASE("DataTable row access", "[data][table]") {
    DataTable table;
    table.define_column("id", DataValue::Type::String);
    table.define_column("name", DataValue::Type::String);
    table.define_column("value", DataValue::Type::Int);
    table.set_id_column("id");

    std::string csv = R"(id,name,value
item1,Sword,100
item2,Shield,50
item3,Potion,25)";
    table.load_csv_string(csv);

    SECTION("Get row by index") {
        auto row0 = table.get_row(0);
        REQUIRE(row0.valid());
        REQUIRE(row0.index() == 0);
        REQUIRE(row0.get_string("id") == "item1");

        auto row2 = table.get_row(2);
        REQUIRE(row2.get_string("id") == "item3");
    }

    SECTION("Find row by ID") {
        auto row = table.find_row("item2");
        REQUIRE(row.valid());
        REQUIRE(row.get_string("name") == "Shield");
        REQUIRE(row.get_int("value") == 50);
    }

    SECTION("Has row check") {
        REQUIRE(table.has_row("item1"));
        REQUIRE(table.has_row("item2"));
        REQUIRE_FALSE(table.has_row("nonexistent"));
    }

    SECTION("Row get_id returns ID column value") {
        auto row = table.get_row(1);
        REQUIRE(row.get_id() == "item2");
    }
}

TEST_CASE("DataRow typed accessors", "[data][table][row]") {
    DataTable table;
    table.define_column("id", DataValue::Type::String);
    table.define_column("flag", DataValue::Type::Bool);
    table.define_column("count", DataValue::Type::Int);
    table.define_column("rate", DataValue::Type::Float);
    table.set_id_column("id");

    std::string json = R"([
        {"id": "test", "flag": true, "count": 42, "rate": 3.14}
    ])";
    table.load_json_string(json);

    auto row = table.get_row(0);

    SECTION("get_bool") {
        REQUIRE(row.get_bool("flag") == true);
        REQUIRE(row.get_bool("nonexistent", false) == false);
    }

    SECTION("get_int") {
        REQUIRE(row.get_int("count") == 42);
        REQUIRE(row.get_int("nonexistent", 99) == 99);
    }

    SECTION("get_float") {
        REQUIRE_THAT(row.get_float("rate"), WithinAbs(3.14, 0.001));
        REQUIRE_THAT(row.get_float("nonexistent", 1.5), WithinAbs(1.5, 0.001));
    }

    SECTION("get_string") {
        REQUIRE(row.get_string("id") == "test");
        REQUIRE(row.get_string("nonexistent", "default") == "default");
    }

    SECTION("has column check") {
        REQUIRE(row.has("id"));
        REQUIRE(row.has("flag"));
        REQUIRE_FALSE(row.has("nonexistent"));
    }

    SECTION("operator[] access") {
        const auto& val = row["count"];
        REQUIRE(val.is_int());
        REQUIRE(val.as_int() == 42);
    }
}

TEST_CASE("DataTable queries", "[data][table][query]") {
    DataTable table;
    table.define_column("id", DataValue::Type::String);
    table.define_column("category", DataValue::Type::String);
    table.define_column("price", DataValue::Type::Int);
    table.set_id_column("id");

    std::string csv = R"(id,category,price
item1,weapon,100
item2,armor,80
item3,weapon,150
item4,consumable,10
item5,armor,120)";
    table.load_csv_string(csv);

    SECTION("Find rows by column value") {
        auto weapons = table.find_rows("category", DataValue("weapon"));
        REQUIRE(weapons.size() == 2);

        auto armors = table.find_rows("category", DataValue("armor"));
        REQUIRE(armors.size() == 2);

        auto consumables = table.find_rows("category", DataValue("consumable"));
        REQUIRE(consumables.size() == 1);
    }

    SECTION("Filter with predicate") {
        auto expensive = table.filter([](const DataRow& row) {
            return row.get_int("price") > 100;
        });
        REQUIRE(expensive.size() == 2); // item3 (150), item5 (120)

        auto cheap = table.filter([](const DataRow& row) {
            return row.get_int("price") < 50;
        });
        REQUIRE(cheap.size() == 1); // item4 (10)
    }
}

TEST_CASE("DataTable iteration", "[data][table][iteration]") {
    DataTable table;
    table.define_column("id", DataValue::Type::String);
    table.define_column("value", DataValue::Type::Int);
    table.set_id_column("id");

    std::string csv = R"(id,value
a,1
b,2
c,3)";
    table.load_csv_string(csv);

    SECTION("Range-based for loop") {
        int sum = 0;
        for (const auto& row : table) {
            sum += row.get_int("value");
        }
        REQUIRE(sum == 6);
    }

    SECTION("Iterator access") {
        auto it = table.begin();
        REQUIRE((*it).get_string("id") == "a");
        ++it;
        REQUIRE((*it).get_string("id") == "b");
        ++it;
        REQUIRE((*it).get_string("id") == "c");
        ++it;
        REQUIRE(it == table.end());
    }
}

TEST_CASE("DataTable metadata", "[data][table]") {
    DataTable table;

    SECTION("Name") {
        table.set_name("items");
        REQUIRE(table.get_name() == "items");
    }

    SECTION("Source path") {
        table.set_source_path("data/items.csv");
        REQUIRE(table.get_source_path() == "data/items.csv");
    }
}
