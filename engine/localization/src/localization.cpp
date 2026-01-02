#include <engine/localization/localization.hpp>
#include <engine/core/log.hpp>
#include <fstream>
#include <sstream>
#include <cctype>
#include <nlohmann/json.hpp>

namespace engine::localization {

using json = nlohmann::json;

namespace {

// Validate language code to prevent path traversal attacks
// Allows: a-z, A-Z, 0-9, underscore, hyphen (e.g., "en", "en_US", "zh-CN")
bool is_valid_language_code(const std::string& code) {
    if (code.empty()) return false;
    for (char c : code) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            return false;
        }
    }
    return true;
}

} // anonymous namespace

LocalizationManager& get_localization() {
    static LocalizationManager instance;
    return instance;
}

// LocalizationTable implementation

bool LocalizationTable::load_from_json(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::stringstream buffer;
    buffer << file.rdbuf();
    return load_from_string(buffer.str(), "json");
}

bool LocalizationTable::load_from_csv(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::string key;
        std::string value;

        // Find the separator - handle quoted values
        size_t pos = 0;

        // Parse key (may be quoted)
        if (!line.empty() && line[0] == '"') {
            size_t end_quote = line.find('"', 1);
            if (end_quote == std::string::npos) continue;
            key = line.substr(1, end_quote - 1);
            pos = end_quote + 1;
            if (pos < line.size() && line[pos] == ',') pos++;
        } else {
            size_t comma = line.find(',');
            if (comma == std::string::npos) continue;
            key = line.substr(0, comma);
            pos = comma + 1;
        }

        // Parse value (may be quoted and contain commas)
        if (pos < line.size() && line[pos] == '"') {
            size_t end_quote = line.find('"', pos + 1);
            if (end_quote != std::string::npos) {
                value = line.substr(pos + 1, end_quote - pos - 1);
            } else {
                value = line.substr(pos + 1);
            }
        } else {
            value = line.substr(pos);
        }

        if (!key.empty()) {
            set(key, value);
        }
    }

    return true;
}

bool LocalizationTable::load_from_string(const std::string& content, const std::string& format) {
    if (format != "json") return false;

    try {
        json j = json::parse(content);

        // Check for language metadata
        if (j.contains("_language")) {
            auto lang = j["_language"];
            if (lang.contains("code")) m_language.code = lang["code"];
            if (lang.contains("region")) m_language.region = lang["region"];
            if (lang.contains("name")) m_language.name = lang["name"];
            if (lang.contains("native_name")) m_language.native_name = lang["native_name"];
        }

        // Load strings
        for (auto& [key, value] : j.items()) {
            if (key.empty() || key[0] == '_') continue;  // Skip metadata

            LocalizedString ls;
            ls.key = key;

            if (value.is_string()) {
                ls.forms[PluralForm::Other] = value.get<std::string>();
            } else if (value.is_object()) {
                // Plural forms
                if (value.contains("zero")) ls.forms[PluralForm::Zero] = value["zero"];
                if (value.contains("one")) ls.forms[PluralForm::One] = value["one"];
                if (value.contains("two")) ls.forms[PluralForm::Two] = value["two"];
                if (value.contains("few")) ls.forms[PluralForm::Few] = value["few"];
                if (value.contains("many")) ls.forms[PluralForm::Many] = value["many"];
                if (value.contains("other")) ls.forms[PluralForm::Other] = value["other"];

                // Single value fallback
                if (value.contains("value")) {
                    ls.forms[PluralForm::Other] = value["value"];
                }
            }

            m_strings[key] = ls;
        }

        return true;
    } catch (const std::exception& e) {
        core::log(core::LogLevel::Error, "Localization: Failed to parse JSON - {}", e.what());
        return false;
    } catch (...) {
        core::log(core::LogLevel::Error, "Localization: Failed to parse JSON - unknown error");
        return false;
    }
}

bool LocalizationTable::save_to_json(const std::string& path) const {
    json j;

    // Add metadata
    json lang_meta;
    lang_meta["code"] = m_language.code;
    lang_meta["region"] = m_language.region;
    lang_meta["name"] = m_language.name;
    lang_meta["native_name"] = m_language.native_name;
    j["_language"] = lang_meta;

    // Add strings
    for (const auto& [key, ls] : m_strings) {
        if (ls.forms.size() == 1 && ls.forms.count(PluralForm::Other)) {
            j[key] = ls.forms.at(PluralForm::Other);
        } else {
            json forms;
            for (const auto& [form, text] : ls.forms) {
                switch (form) {
                    case PluralForm::Zero: forms["zero"] = text; break;
                    case PluralForm::One: forms["one"] = text; break;
                    case PluralForm::Two: forms["two"] = text; break;
                    case PluralForm::Few: forms["few"] = text; break;
                    case PluralForm::Many: forms["many"] = text; break;
                    case PluralForm::Other: forms["other"] = text; break;
                }
            }
            j[key] = forms;
        }
    }

    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << j.dump(2);
    return file.good();
}

const LocalizedString* LocalizationTable::get(const std::string& key) const {
    auto it = m_strings.find(key);
    return it != m_strings.end() ? &it->second : nullptr;
}

bool LocalizationTable::has(const std::string& key) const {
    return m_strings.find(key) != m_strings.end();
}

void LocalizationTable::set(const std::string& key, const std::string& value) {
    LocalizedString ls;
    ls.key = key;
    ls.forms[PluralForm::Other] = value;
    m_strings[key] = ls;
}

void LocalizationTable::set(const std::string& key, const LocalizedString& localized) {
    m_strings[key] = localized;
}

void LocalizationTable::remove(const std::string& key) {
    m_strings.erase(key);
}

void LocalizationTable::clear() {
    m_strings.clear();
}

// LocalizationManager implementation

void LocalizationManager::init(const LocalizationConfig& config) {
    std::unique_lock lock(m_mutex);
    if (m_initialized) return;

    m_config = config;

    // Set up default plural rules
    m_plural_rules["en"] = PluralRules::english;
    m_plural_rules["es"] = PluralRules::english;  // Spanish uses same as English
    m_plural_rules["de"] = PluralRules::english;  // German too
    m_plural_rules["fr"] = PluralRules::french;
    m_plural_rules["ru"] = PluralRules::russian;
    m_plural_rules["ja"] = PluralRules::cjk;
    m_plural_rules["zh"] = PluralRules::cjk;
    m_plural_rules["ko"] = PluralRules::cjk;
    m_plural_rules["ar"] = PluralRules::arabic;
    m_plural_rules["pl"] = PluralRules::polish;

    // Mark initialized before auto-load; fallback behavior handles missing strings
    m_initialized = true;
    lock.unlock();

    // Load default language if auto_load enabled
    if (config.auto_load && !config.default_language.empty()) {
        load_language(config.default_language);
        set_language(config.default_language);
    }
}

void LocalizationManager::shutdown() {
    std::unique_lock lock(m_mutex);
    if (!m_initialized) return;

    m_tables.clear();
    m_callbacks.clear();

    m_initialized = false;
}

bool LocalizationManager::set_language(const std::string& code) {
    LanguageCode lang;
    lang.code = code;
    return set_language(lang);
}

bool LocalizationManager::set_language(const LanguageCode& lang) {
    std::unique_lock lock(m_mutex);

    // Check if language is loaded
    std::string code = lang.code;
    if (m_tables.find(code) == m_tables.end()) {
        lock.unlock();
        if (!load_language(lang)) {
            return false;
        }
        lock.lock();
    }

    LanguageCode old_lang = m_current_language;
    m_current_language = lang;

    // Copy callbacks to avoid holding lock during invocation
    auto callbacks_copy = m_callbacks;
    lock.unlock();

    // Notify callbacks with exception safety
    for (const auto& [name, callback] : callbacks_copy) {
        try {
            callback(old_lang, lang);
        } catch (const std::exception& e) {
            core::log(core::LogLevel::Error, "Localization: Callback '{}' threw exception: {}", name, e.what());
        } catch (...) {
            core::log(core::LogLevel::Error, "Localization: Callback '{}' threw unknown exception", name);
        }
    }

    return true;
}

std::vector<LanguageCode> LocalizationManager::get_available_languages() const {
    std::shared_lock lock(m_mutex);
    std::vector<LanguageCode> languages;
    for (const auto& [code, table] : m_tables) {
        languages.push_back(table->get_language());
    }
    return languages;
}

bool LocalizationManager::is_language_available(const std::string& code) const {
    std::shared_lock lock(m_mutex);
    return m_tables.find(code) != m_tables.end();
}

bool LocalizationManager::load_language(const std::string& code) {
    LanguageCode lang;
    lang.code = code;
    return load_language(lang);
}

bool LocalizationManager::load_language(const LanguageCode& lang) {
    std::string code = lang.code;

    // Validate language code to prevent path traversal
    if (!is_valid_language_code(code)) {
        core::log(core::LogLevel::Error, "Localization: Invalid language code '{}'", code);
        return false;
    }

    std::string path;

    {
        std::shared_lock lock(m_mutex);
        // Already loaded?
        if (m_tables.find(code) != m_tables.end()) {
            return true;
        }
        path = m_config.localization_path + "/" + code + m_config.file_extension;
    }

    // Load file outside of lock
    auto table = std::make_unique<LocalizationTable>();
    table->set_language(lang);

    if (!table->load_from_json(path)) {
        core::log(core::LogLevel::Warn, "Localization: Failed to load language file '{}'", path);
        return false;
    }

    std::unique_lock lock(m_mutex);
    // Check again in case another thread loaded it
    if (m_tables.find(code) != m_tables.end()) {
        return true;
    }

    m_tables[code] = std::move(table);
    m_stats.loaded_languages++;
    m_stats.total_strings += m_tables[code]->size();

    return true;
}

void LocalizationManager::unload_language(const std::string& code) {
    std::unique_lock lock(m_mutex);
    auto it = m_tables.find(code);
    if (it != m_tables.end()) {
        m_stats.total_strings -= it->second->size();
        m_stats.loaded_languages--;
        m_tables.erase(it);
    }
}

std::string LocalizationManager::get(const std::string& key) const {
    std::shared_lock lock(m_mutex);
    return resolve_key_unlocked(key);
}

std::string LocalizationManager::get(const std::string& key, int64_t count) const {
    std::shared_lock lock(m_mutex);

    // Find in current language
    auto table_it = m_tables.find(m_current_language.code);
    if (table_it != m_tables.end()) {
        const LocalizedString* ls = table_it->second->get(key);
        if (ls) {
            PluralForm form = get_plural_form_unlocked(m_current_language.code, count);
            auto form_it = ls->forms.find(form);
            if (form_it != ls->forms.end()) {
                return form_it->second;
            }
            // Fallback to other
            form_it = ls->forms.find(PluralForm::Other);
            if (form_it != ls->forms.end()) {
                return form_it->second;
            }
        }
    }

    // Try fallback language
    if (m_current_language.code != m_config.fallback_language) {
        table_it = m_tables.find(m_config.fallback_language);
        if (table_it != m_tables.end()) {
            const LocalizedString* ls = table_it->second->get(key);
            if (ls) {
                return ls->get(count, [this](int64_t n) {
                    return get_plural_form_unlocked(m_config.fallback_language, n);
                });
            }
        }
    }

    m_stats.missing_lookups++;
    return m_config.show_missing_keys ? m_config.missing_prefix + key : "";
}

std::string LocalizationManager::get_formatted(const std::string& key,
                                                 const std::unordered_map<std::string, std::string>& args) const {
    std::string str = get(key);
    return format(str, args);
}

bool LocalizationManager::has(const std::string& key) const {
    std::shared_lock lock(m_mutex);

    auto table_it = m_tables.find(m_current_language.code);
    if (table_it != m_tables.end() && table_it->second->has(key)) {
        return true;
    }

    if (m_current_language.code != m_config.fallback_language) {
        table_it = m_tables.find(m_config.fallback_language);
        if (table_it != m_tables.end() && table_it->second->has(key)) {
            return true;
        }
    }

    return false;
}

std::string LocalizationManager::format(const std::string& str,
                                          const std::unordered_map<std::string, std::string>& args) {
    std::string result = str;

    // Replace {key} patterns
    for (const auto& [key, value] : args) {
        std::string pattern = "{" + key + "}";
        size_t pos = 0;
        while ((pos = result.find(pattern, pos)) != std::string::npos) {
            result.replace(pos, pattern.length(), value);
            pos += value.length();
        }
    }

    return result;
}

void LocalizationManager::add_callback(const std::string& name, LanguageChangeCallback callback) {
    std::unique_lock lock(m_mutex);
    m_callbacks[name] = callback;
}

void LocalizationManager::remove_callback(const std::string& name) {
    std::unique_lock lock(m_mutex);
    m_callbacks.erase(name);
}

void LocalizationManager::set_plural_rule(const std::string& language, PluralRuleFunc rule) {
    std::unique_lock lock(m_mutex);
    m_plural_rules[language] = rule;
}

LocalizationTable* LocalizationManager::get_table(const std::string& code) {
    std::unique_lock lock(m_mutex);
    auto it = m_tables.find(code);
    return it != m_tables.end() ? it->second.get() : nullptr;
}

const LocalizationTable* LocalizationManager::get_table(const std::string& code) const {
    std::shared_lock lock(m_mutex);
    auto it = m_tables.find(code);
    return it != m_tables.end() ? it->second.get() : nullptr;
}

std::string LocalizationManager::resolve_key_unlocked(const std::string& key) const {
    // Find in current language
    auto table_it = m_tables.find(m_current_language.code);
    if (table_it != m_tables.end()) {
        const LocalizedString* ls = table_it->second->get(key);
        if (ls) {
            return ls->get();
        }
    }

    // Try fallback language
    if (m_current_language.code != m_config.fallback_language) {
        table_it = m_tables.find(m_config.fallback_language);
        if (table_it != m_tables.end()) {
            const LocalizedString* ls = table_it->second->get(key);
            if (ls) {
                return ls->get();
            }
        }
    }

    m_stats.missing_lookups++;
    return m_config.show_missing_keys ? m_config.missing_prefix + key : "";
}

PluralForm LocalizationManager::get_plural_form_unlocked(const std::string& language, int64_t count) const {
    auto it = m_plural_rules.find(language);
    if (it != m_plural_rules.end()) {
        return it->second(count);
    }
    // Default to English rules
    return PluralRules::english(count);
}

} // namespace engine::localization
