#pragma once

#include <engine/scene/entity.hpp>
#include <engine/core/math.hpp>
#include <string>
#include <unordered_map>
#include <any>
#include <optional>
#include <typeindex>

namespace engine::ai {

// ============================================================================
// Blackboard - Key-value store for AI state
// ============================================================================

class Blackboard {
public:
    Blackboard() = default;
    ~Blackboard() = default;

    // ========================================================================
    // Generic value access
    // ========================================================================

    template<typename T>
    void set(const std::string& key, T value) {
        m_data[key] = std::move(value);
    }

    template<typename T>
    T get(const std::string& key, T default_value = T{}) const {
        auto it = m_data.find(key);
        if (it != m_data.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return default_value;
            }
        }
        return default_value;
    }

    template<typename T>
    T* try_get(const std::string& key) {
        auto it = m_data.find(key);
        if (it != m_data.end()) {
            return std::any_cast<T>(&it->second);
        }
        return nullptr;
    }

    template<typename T>
    const T* try_get(const std::string& key) const {
        auto it = m_data.find(key);
        if (it != m_data.end()) {
            return std::any_cast<T>(&it->second);
        }
        return nullptr;
    }

    template<typename T>
    std::optional<T> get_optional(const std::string& key) const {
        auto it = m_data.find(key);
        if (it != m_data.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    // ========================================================================
    // Common type shortcuts
    // ========================================================================

    // Float
    void set_float(const std::string& key, float value) { set<float>(key, value); }
    float get_float(const std::string& key, float default_value = 0.0f) const {
        return get<float>(key, default_value);
    }

    // Int
    void set_int(const std::string& key, int value) { set<int>(key, value); }
    int get_int(const std::string& key, int default_value = 0) const {
        return get<int>(key, default_value);
    }

    // Bool
    void set_bool(const std::string& key, bool value) { set<bool>(key, value); }
    bool get_bool(const std::string& key, bool default_value = false) const {
        return get<bool>(key, default_value);
    }

    // String
    void set_string(const std::string& key, const std::string& value) {
        set<std::string>(key, value);
    }
    std::string get_string(const std::string& key, const std::string& default_value = "") const {
        return get<std::string>(key, default_value);
    }

    // Entity
    void set_entity(const std::string& key, scene::Entity entity) {
        set<scene::Entity>(key, entity);
    }
    scene::Entity get_entity(const std::string& key) const {
        return get<scene::Entity>(key, scene::NullEntity);
    }

    // Vec3
    void set_position(const std::string& key, const Vec3& pos) { set<Vec3>(key, pos); }
    Vec3 get_position(const std::string& key, const Vec3& default_value = Vec3(0.0f)) const {
        return get<Vec3>(key, default_value);
    }

    // ========================================================================
    // Management
    // ========================================================================

    bool has(const std::string& key) const {
        return m_data.find(key) != m_data.end();
    }

    void remove(const std::string& key) {
        m_data.erase(key);
    }

    void clear() {
        m_data.clear();
    }

    // Get all keys
    std::vector<std::string> get_keys() const {
        std::vector<std::string> keys;
        keys.reserve(m_data.size());
        for (const auto& [key, _] : m_data) {
            keys.push_back(key);
        }
        return keys;
    }

    size_t size() const { return m_data.size(); }
    bool empty() const { return m_data.empty(); }

    // ========================================================================
    // Copy and merge
    // ========================================================================

    void copy_from(const Blackboard& other) {
        for (const auto& [key, value] : other.m_data) {
            m_data[key] = value;
        }
    }

    void merge(const Blackboard& other) {
        // Only adds keys that don't exist
        for (const auto& [key, value] : other.m_data) {
            if (m_data.find(key) == m_data.end()) {
                m_data[key] = value;
            }
        }
    }

private:
    std::unordered_map<std::string, std::any> m_data;
};

// ============================================================================
// Common blackboard keys
// ============================================================================

namespace bb {

// Target/threat
constexpr const char* TARGET_ENTITY = "target_entity";
constexpr const char* TARGET_POSITION = "target_position";
constexpr const char* TARGET_DISTANCE = "target_distance";
constexpr const char* THREAT_LEVEL = "threat_level";

// Self state
constexpr const char* SELF_POSITION = "self_position";
constexpr const char* SELF_HEALTH = "self_health";
constexpr const char* SELF_HEALTH_PERCENT = "self_health_percent";

// Movement
constexpr const char* MOVE_TARGET = "move_target";
constexpr const char* MOVE_SPEED = "move_speed";
constexpr const char* PATH_FOUND = "path_found";

// Combat
constexpr const char* IN_ATTACK_RANGE = "in_attack_range";
constexpr const char* CAN_ATTACK = "can_attack";
constexpr const char* LAST_ATTACK_TIME = "last_attack_time";
constexpr const char* ATTACK_COOLDOWN = "attack_cooldown";

// Perception
constexpr const char* CAN_SEE_TARGET = "can_see_target";
constexpr const char* CAN_HEAR_TARGET = "can_hear_target";
constexpr const char* LAST_KNOWN_POSITION = "last_known_position";
constexpr const char* TIME_SINCE_SEEN = "time_since_seen";

// State
constexpr const char* CURRENT_STATE = "current_state";
constexpr const char* IS_ALERTED = "is_alerted";
constexpr const char* IS_INVESTIGATING = "is_investigating";

} // namespace bb

} // namespace engine::ai
