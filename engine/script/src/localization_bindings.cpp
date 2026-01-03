#include <engine/script/bindings.hpp>
#include <engine/localization/localization.hpp>

namespace engine::script {

void register_localization_bindings(sol::state& lua) {
    auto loc = lua.create_named_table("Loc");

    // String lookup
    loc.set_function("get", sol::overload(
        [](const std::string& key) { return localization::loc(key); },
        [](const std::string& key, int64_t count) { return localization::loc(key, count); }
    ));

    loc.set_function("get_formatted", [](const std::string& key, sol::table args) {
        std::unordered_map<std::string, std::string> map;
        for (auto& pair : args) {
            map[pair.first.as<std::string>()] = pair.second.as<std::string>();
        }
        return localization::get_localization().get_formatted(key, map);
    });

    loc.set_function("has", [](const std::string& key) {
        return localization::get_localization().has(key);
    });

    // Language management
    loc.set_function("set_language", [](const std::string& code) {
        return localization::get_localization().set_language(code);
    });

    loc.set_function("get_language", []() {
        return localization::get_localization().get_current_language().code;
    });

    loc.set_function("get_available_languages", []() {
        auto langs = localization::get_localization().get_available_languages();
        std::vector<std::string> codes;
        codes.reserve(langs.size());
        for (const auto& lang : langs) {
            codes.push_back(lang.code);
        }
        return codes;
    });
}

} // namespace engine::script
