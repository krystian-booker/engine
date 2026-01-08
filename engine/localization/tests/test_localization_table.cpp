#include <catch2/catch_test_macros.hpp>
#include <engine/localization/localization.hpp>

using namespace engine::localization;

TEST_CASE("LanguageCode construction", "[localization][language]") {
    SECTION("Manual construction") {
        LanguageCode lang{"en", "US", "English", "English"};
        REQUIRE(lang.code == "en");
        REQUIRE(lang.region == "US");
        REQUIRE(lang.name == "English");
        REQUIRE(lang.native_name == "English");
    }

    SECTION("Full code generation") {
        LanguageCode lang{"en", "US", "English", "English"};
        REQUIRE(lang.full_code() == "en_US");

        LanguageCode lang_no_region{"en", "", "English", "English"};
        REQUIRE(lang_no_region.full_code() == "en");
    }

    SECTION("Equality comparison") {
        LanguageCode en1{"en", "US", "English", "English"};
        LanguageCode en2{"en", "US", "English", "English"};
        LanguageCode en_uk{"en", "UK", "English (UK)", "English"};

        REQUIRE(en1 == en2);
        REQUIRE_FALSE(en1 == en_uk);
    }
}

TEST_CASE("Common language codes", "[localization][language]") {
    SECTION("English") {
        auto lang = Languages::english();
        REQUIRE(lang.code == "en");
        REQUIRE(lang.region == "US");
    }

    SECTION("Japanese") {
        auto lang = Languages::japanese();
        REQUIRE(lang.code == "ja");
        REQUIRE(lang.native_name == "日本語");
    }

    SECTION("German") {
        auto lang = Languages::german();
        REQUIRE(lang.code == "de");
        REQUIRE(lang.native_name == "Deutsch");
    }

    SECTION("Spanish") {
        auto lang = Languages::spanish();
        REQUIRE(lang.code == "es");
    }

    SECTION("French") {
        auto lang = Languages::french();
        REQUIRE(lang.code == "fr");
    }

    SECTION("Chinese Simplified") {
        auto lang = Languages::chinese_simplified();
        REQUIRE(lang.code == "zh");
    }

    SECTION("Korean") {
        auto lang = Languages::korean();
        REQUIRE(lang.code == "ko");
    }

    SECTION("Russian") {
        auto lang = Languages::russian();
        REQUIRE(lang.code == "ru");
    }

    SECTION("Polish") {
        auto lang = Languages::polish();
        REQUIRE(lang.code == "pl");
    }
}

TEST_CASE("LocalizedString", "[localization][string]") {
    SECTION("Simple string") {
        LocalizedString str;
        str.key = "greeting";
        str.forms[PluralForm::Other] = "Hello";

        REQUIRE(str.get() == "Hello");
    }

    SECTION("Plural forms") {
        LocalizedString str;
        str.key = "items";
        str.forms[PluralForm::One] = "1 item";
        str.forms[PluralForm::Other] = "{count} items";

        REQUIRE(str.get(1, PluralRules::english) == "1 item");
        REQUIRE(str.get(5, PluralRules::english) == "{count} items");
    }

    SECTION("Fallback to Other form") {
        LocalizedString str;
        str.key = "test";
        str.forms[PluralForm::Other] = "default";

        // Request a form that doesn't exist, should fall back to Other
        REQUIRE(str.get(0, PluralRules::english) == "default");
    }
}

TEST_CASE("LocalizationTable basic operations", "[localization][table]") {
    LocalizationTable table;

    SECTION("Empty table") {
        REQUIRE(table.size() == 0);
        REQUIRE_FALSE(table.has("key"));
        REQUIRE(table.get("key") == nullptr);
    }

    SECTION("Set and get string") {
        table.set("greeting", "Hello, World!");
        REQUIRE(table.has("greeting"));
        REQUIRE(table.size() == 1);

        const auto* str = table.get("greeting");
        REQUIRE(str != nullptr);
        REQUIRE(str->get() == "Hello, World!");
    }

    SECTION("Set localized string") {
        LocalizedString str;
        str.key = "items";
        str.forms[PluralForm::One] = "1 item";
        str.forms[PluralForm::Other] = "{n} items";

        table.set("items", str);

        const auto* retrieved = table.get("items");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->forms.size() == 2);
    }

    SECTION("Remove string") {
        table.set("temp", "temporary");
        REQUIRE(table.has("temp"));

        table.remove("temp");
        REQUIRE_FALSE(table.has("temp"));
    }

    SECTION("Clear table") {
        table.set("key1", "value1");
        table.set("key2", "value2");
        REQUIRE(table.size() == 2);

        table.clear();
        REQUIRE(table.size() == 0);
    }

    SECTION("Language metadata") {
        auto lang = Languages::english();
        table.set_language(lang);

        REQUIRE(table.get_language().code == "en");
        REQUIRE(table.get_language().region == "US");
    }
}

TEST_CASE("LocalizationTable iteration", "[localization][table]") {
    LocalizationTable table;
    table.set("key1", "value1");
    table.set("key2", "value2");
    table.set("key3", "value3");

    SECTION("Count via iteration") {
        int count = 0;
        for (const auto& [key, str] : table) {
            count++;
        }
        REQUIRE(count == 3);
    }
}

TEST_CASE("LocalizationTable load from string", "[localization][table][json]") {
    LocalizationTable table;

    SECTION("Load JSON content") {
        std::string json = R"({
            "greeting": "Hello",
            "farewell": "Goodbye",
            "items": {
                "one": "1 item",
                "other": "{count} items"
            }
        })";

        bool loaded = table.load_from_string(json, "json");
        REQUIRE(loaded);

        REQUIRE(table.has("greeting"));
        REQUIRE(table.get("greeting")->get() == "Hello");
        REQUIRE(table.has("farewell"));
    }
}
