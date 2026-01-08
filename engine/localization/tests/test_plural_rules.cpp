#include <catch2/catch_test_macros.hpp>
#include <engine/localization/localization.hpp>

using namespace engine::localization;

TEST_CASE("English plural rules", "[localization][plurals]") {
    SECTION("Singular for 1") {
        REQUIRE(PluralRules::english(1) == PluralForm::One);
    }

    SECTION("Plural for 0") {
        REQUIRE(PluralRules::english(0) == PluralForm::Other);
    }

    SECTION("Plural for multiple") {
        REQUIRE(PluralRules::english(2) == PluralForm::Other);
        REQUIRE(PluralRules::english(5) == PluralForm::Other);
        REQUIRE(PluralRules::english(100) == PluralForm::Other);
    }

    SECTION("Negative numbers") {
        REQUIRE(PluralRules::english(-1) == PluralForm::Other);
    }
}

TEST_CASE("French plural rules", "[localization][plurals]") {
    SECTION("Singular for 0") {
        REQUIRE(PluralRules::french(0) == PluralForm::One);
    }

    SECTION("Singular for 1") {
        REQUIRE(PluralRules::french(1) == PluralForm::One);
    }

    SECTION("Plural for 2+") {
        REQUIRE(PluralRules::french(2) == PluralForm::Other);
        REQUIRE(PluralRules::french(5) == PluralForm::Other);
    }
}

TEST_CASE("Russian plural rules", "[localization][plurals]") {
    SECTION("One form") {
        REQUIRE(PluralRules::russian(1) == PluralForm::One);
        REQUIRE(PluralRules::russian(21) == PluralForm::One);
        REQUIRE(PluralRules::russian(31) == PluralForm::One);
        REQUIRE(PluralRules::russian(101) == PluralForm::One);
    }

    SECTION("Few form") {
        REQUIRE(PluralRules::russian(2) == PluralForm::Few);
        REQUIRE(PluralRules::russian(3) == PluralForm::Few);
        REQUIRE(PluralRules::russian(4) == PluralForm::Few);
        REQUIRE(PluralRules::russian(22) == PluralForm::Few);
        REQUIRE(PluralRules::russian(23) == PluralForm::Few);
        REQUIRE(PluralRules::russian(24) == PluralForm::Few);
    }

    SECTION("Many form") {
        REQUIRE(PluralRules::russian(0) == PluralForm::Many);
        REQUIRE(PluralRules::russian(5) == PluralForm::Many);
        REQUIRE(PluralRules::russian(6) == PluralForm::Many);
        REQUIRE(PluralRules::russian(10) == PluralForm::Many);
        REQUIRE(PluralRules::russian(11) == PluralForm::Many);
        REQUIRE(PluralRules::russian(12) == PluralForm::Many);
        REQUIRE(PluralRules::russian(14) == PluralForm::Many);
        REQUIRE(PluralRules::russian(20) == PluralForm::Many);
    }

    SECTION("Teen exceptions (11-14 always Many)") {
        REQUIRE(PluralRules::russian(11) == PluralForm::Many);
        REQUIRE(PluralRules::russian(12) == PluralForm::Many);
        REQUIRE(PluralRules::russian(13) == PluralForm::Many);
        REQUIRE(PluralRules::russian(14) == PluralForm::Many);
        REQUIRE(PluralRules::russian(111) == PluralForm::Many);
        REQUIRE(PluralRules::russian(112) == PluralForm::Many);
    }
}

TEST_CASE("CJK plural rules (no plurals)", "[localization][plurals]") {
    SECTION("Always Other") {
        REQUIRE(PluralRules::cjk(0) == PluralForm::Other);
        REQUIRE(PluralRules::cjk(1) == PluralForm::Other);
        REQUIRE(PluralRules::cjk(2) == PluralForm::Other);
        REQUIRE(PluralRules::cjk(100) == PluralForm::Other);
    }
}

TEST_CASE("Arabic plural rules", "[localization][plurals]") {
    SECTION("Zero form") {
        REQUIRE(PluralRules::arabic(0) == PluralForm::Zero);
    }

    SECTION("One form") {
        REQUIRE(PluralRules::arabic(1) == PluralForm::One);
    }

    SECTION("Two form") {
        REQUIRE(PluralRules::arabic(2) == PluralForm::Two);
    }

    SECTION("Few form (3-10 in last two digits)") {
        REQUIRE(PluralRules::arabic(3) == PluralForm::Few);
        REQUIRE(PluralRules::arabic(10) == PluralForm::Few);
        REQUIRE(PluralRules::arabic(103) == PluralForm::Few);
    }

    SECTION("Many form (11+ in last two digits)") {
        REQUIRE(PluralRules::arabic(11) == PluralForm::Many);
        REQUIRE(PluralRules::arabic(99) == PluralForm::Many);
        REQUIRE(PluralRules::arabic(111) == PluralForm::Many);
    }

    SECTION("Other form (100, 200, etc.)") {
        REQUIRE(PluralRules::arabic(100) == PluralForm::Other);
        REQUIRE(PluralRules::arabic(200) == PluralForm::Other);
    }
}

TEST_CASE("Polish plural rules", "[localization][plurals]") {
    SECTION("One form") {
        REQUIRE(PluralRules::polish(1) == PluralForm::One);
    }

    SECTION("Few form") {
        REQUIRE(PluralRules::polish(2) == PluralForm::Few);
        REQUIRE(PluralRules::polish(3) == PluralForm::Few);
        REQUIRE(PluralRules::polish(4) == PluralForm::Few);
        REQUIRE(PluralRules::polish(22) == PluralForm::Few);
        REQUIRE(PluralRules::polish(23) == PluralForm::Few);
        REQUIRE(PluralRules::polish(24) == PluralForm::Few);
    }

    SECTION("Many form") {
        REQUIRE(PluralRules::polish(0) == PluralForm::Many);
        REQUIRE(PluralRules::polish(5) == PluralForm::Many);
        REQUIRE(PluralRules::polish(11) == PluralForm::Many);
        REQUIRE(PluralRules::polish(12) == PluralForm::Many);
        REQUIRE(PluralRules::polish(13) == PluralForm::Many);
        REQUIRE(PluralRules::polish(14) == PluralForm::Many);
        REQUIRE(PluralRules::polish(20) == PluralForm::Many);
        REQUIRE(PluralRules::polish(21) == PluralForm::Many);
    }

    SECTION("Teen exceptions") {
        REQUIRE(PluralRules::polish(12) == PluralForm::Many);
        REQUIRE(PluralRules::polish(13) == PluralForm::Many);
        REQUIRE(PluralRules::polish(14) == PluralForm::Many);
    }
}

TEST_CASE("PluralForm enum values", "[localization][plurals]") {
    REQUIRE(static_cast<uint8_t>(PluralForm::Zero) == 0);
    REQUIRE(static_cast<uint8_t>(PluralForm::One) == 1);
    REQUIRE(static_cast<uint8_t>(PluralForm::Two) == 2);
    REQUIRE(static_cast<uint8_t>(PluralForm::Few) == 3);
    REQUIRE(static_cast<uint8_t>(PluralForm::Many) == 4);
    REQUIRE(static_cast<uint8_t>(PluralForm::Other) == 5);
}
