#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <atomic>

namespace engine::localization {

// Language code (ISO 639-1)
struct LanguageCode {
    std::string code;          // e.g., "en", "es", "ja"
    std::string region;        // e.g., "US", "MX", "JP"
    std::string name;          // Display name: "English", "Español"
    std::string native_name;   // Native name: "English", "日本語"

    std::string full_code() const {
        return region.empty() ? code : code + "_" + region;
    }

    bool operator==(const LanguageCode& other) const {
        return code == other.code && region == other.region;
    }
};

// Plural form rules
enum class PluralForm : uint8_t {
    Zero,
    One,
    Two,
    Few,
    Many,
    Other
};

// Get plural form for a language
using PluralRuleFunc = std::function<PluralForm(int64_t count)>;

// Localized string with plural support
struct LocalizedString {
    std::string key;
    std::unordered_map<PluralForm, std::string> forms;

    // Get singular form
    const std::string& get() const {
        auto it = forms.find(PluralForm::Other);
        if (it != forms.end()) return it->second;
        if (!forms.empty()) return forms.begin()->second;
        static std::string empty;
        return empty;
    }

    // Get form for count
    const std::string& get(int64_t count, PluralRuleFunc rule) const {
        PluralForm form = rule(count);

        auto it = forms.find(form);
        if (it != forms.end()) return it->second;

        // Fallback to Other
        it = forms.find(PluralForm::Other);
        if (it != forms.end()) return it->second;

        static std::string empty;
        return empty;
    }
};

// Localization table - collection of strings for one language
class LocalizationTable {
public:
    LocalizationTable() = default;
    ~LocalizationTable() = default;

    // Load from file
    bool load_from_json(const std::string& path);
    bool load_from_csv(const std::string& path);
    bool load_from_string(const std::string& content, const std::string& format = "json");

    // Save to file
    bool save_to_json(const std::string& path) const;

    // String access
    const LocalizedString* get(const std::string& key) const;
    bool has(const std::string& key) const;

    // Add/update strings
    void set(const std::string& key, const std::string& value);
    void set(const std::string& key, const LocalizedString& localized);
    void remove(const std::string& key);
    void clear();

    // Iteration
    size_t size() const { return m_strings.size(); }
    auto begin() const { return m_strings.begin(); }
    auto end() const { return m_strings.end(); }

    // Language info
    void set_language(const LanguageCode& lang) { m_language = lang; }
    const LanguageCode& get_language() const { return m_language; }

private:
    LanguageCode m_language;
    std::unordered_map<std::string, LocalizedString> m_strings;
};

// Localization manager configuration
struct LocalizationConfig {
    std::string default_language = "en";
    std::string fallback_language = "en";
    std::string localization_path = "localization";
    std::string file_extension = ".json";
    bool auto_load = true;
    bool show_missing_keys = true;       // Show key if string not found
    std::string missing_prefix = "[!]";  // Prefix for missing strings
};

// Language change callback
using LanguageChangeCallback = std::function<void(const LanguageCode& old_lang,
                                                    const LanguageCode& new_lang)>;

// Localization manager
class LocalizationManager {
public:
    LocalizationManager() = default;
    ~LocalizationManager() = default;

    // Non-copyable
    LocalizationManager(const LocalizationManager&) = delete;
    LocalizationManager& operator=(const LocalizationManager&) = delete;

    // Initialize/shutdown
    void init(const LocalizationConfig& config = {});
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Configuration
    void set_config(const LocalizationConfig& config) { m_config = config; }
    const LocalizationConfig& get_config() const { return m_config; }

    // Language management
    bool set_language(const std::string& code);
    bool set_language(const LanguageCode& lang);
    const LanguageCode& get_current_language() const { return m_current_language; }

    // Get available languages
    std::vector<LanguageCode> get_available_languages() const;
    bool is_language_available(const std::string& code) const;

    // Load language
    bool load_language(const std::string& code);
    bool load_language(const LanguageCode& lang);
    void unload_language(const std::string& code);

    // String lookup
    std::string get(const std::string& key) const;
    std::string get(const std::string& key, int64_t count) const;
    std::string get_formatted(const std::string& key,
                               const std::unordered_map<std::string, std::string>& args) const;

    // Quick lookup operator
    std::string operator[](const std::string& key) const { return get(key); }

    // Check if key exists
    bool has(const std::string& key) const;

    // Format string with arguments
    static std::string format(const std::string& str,
                               const std::unordered_map<std::string, std::string>& args);

    // Language change callbacks
    void add_callback(const std::string& name, LanguageChangeCallback callback);
    void remove_callback(const std::string& name);

    // Plural rules
    void set_plural_rule(const std::string& language, PluralRuleFunc rule);

    // Table access (for editors)
    // NOTE: Returned pointer is NOT thread-safe. Only use from single-threaded
    // editor/tool contexts. Do not modify the table while other threads may be
    // calling get() or other lookup methods.
    LocalizationTable* get_table(const std::string& code);
    const LocalizationTable* get_table(const std::string& code) const;

    // Statistics
    struct Stats {
        size_t loaded_languages = 0;
        size_t total_strings = 0;
        std::atomic<size_t> missing_lookups{0};  // Atomic for thread-safe increment under shared_lock

        Stats() = default;
        Stats(const Stats& other) {
            loaded_languages = other.loaded_languages;
            total_strings = other.total_strings;
            missing_lookups.store(other.missing_lookups.load(std::memory_order_relaxed));
        }
    };
    Stats get_stats() const {
        std::shared_lock lock(m_mutex);
        Stats result;
        result.loaded_languages = m_stats.loaded_languages;
        result.total_strings = m_stats.total_strings;
        result.missing_lookups.store(m_stats.missing_lookups.load(std::memory_order_relaxed));
        return result;
    }

private:
    std::string resolve_key_unlocked(const std::string& key) const;
    PluralForm get_plural_form_unlocked(const std::string& language, int64_t count) const;

    LocalizationConfig m_config;
    bool m_initialized = false;

    LanguageCode m_current_language;
    std::unordered_map<std::string, std::unique_ptr<LocalizationTable>> m_tables;
    std::unordered_map<std::string, PluralRuleFunc> m_plural_rules;
    std::unordered_map<std::string, LanguageChangeCallback> m_callbacks;

    mutable Stats m_stats;
    mutable std::shared_mutex m_mutex;
};

// Global localization manager
LocalizationManager& get_localization();

// Convenience function for string lookup
inline std::string loc(const std::string& key) {
    return get_localization().get(key);
}

inline std::string loc(const std::string& key, int64_t count) {
    return get_localization().get(key, count);
}

// Common plural rules
namespace PluralRules {

// English: singular for 1, plural for everything else
inline PluralForm english(int64_t n) {
    return n == 1 ? PluralForm::One : PluralForm::Other;
}

// French: singular for 0-1, plural for 2+
inline PluralForm french(int64_t n) {
    return n <= 1 ? PluralForm::One : PluralForm::Other;
}

// Russian: complex rules
inline PluralForm russian(int64_t n) {
    if (n % 10 == 1 && n % 100 != 11) return PluralForm::One;
    if (n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 10 || n % 100 >= 20)) return PluralForm::Few;
    return PluralForm::Many;
}

// Japanese/Chinese/Korean: no plural forms
inline PluralForm cjk(int64_t /*n*/) {
    return PluralForm::Other;
}

// Arabic: 6 forms
inline PluralForm arabic(int64_t n) {
    if (n == 0) return PluralForm::Zero;
    if (n == 1) return PluralForm::One;
    if (n == 2) return PluralForm::Two;
    if (n % 100 >= 3 && n % 100 <= 10) return PluralForm::Few;
    if (n % 100 >= 11) return PluralForm::Many;
    return PluralForm::Other;
}

// Polish
inline PluralForm polish(int64_t n) {
    if (n == 1) return PluralForm::One;
    if (n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 10 || n % 100 >= 20)) return PluralForm::Few;
    return PluralForm::Many;
}

} // namespace PluralRules

// Common language codes
namespace Languages {

inline LanguageCode english() {
    return {"en", "US", "English", "English"};
}

inline LanguageCode spanish() {
    return {"es", "ES", "Spanish", "Español"};
}

inline LanguageCode french() {
    return {"fr", "FR", "French", "Français"};
}

inline LanguageCode german() {
    return {"de", "DE", "German", "Deutsch"};
}

inline LanguageCode japanese() {
    return {"ja", "JP", "Japanese", "日本語"};
}

inline LanguageCode chinese_simplified() {
    return {"zh", "CN", "Chinese (Simplified)", "简体中文"};
}

inline LanguageCode korean() {
    return {"ko", "KR", "Korean", "한국어"};
}

inline LanguageCode russian() {
    return {"ru", "RU", "Russian", "Русский"};
}

inline LanguageCode portuguese_brazil() {
    return {"pt", "BR", "Portuguese (Brazil)", "Português"};
}

inline LanguageCode polish() {
    return {"pl", "PL", "Polish", "Polski"};
}

} // namespace Languages

} // namespace engine::localization
