#include <engine/core/serialize.hpp>
#include <cstring>

namespace engine::core {

namespace {
    constexpr uint32_t MAX_SERIALIZED_SIZE = 256 * 1024 * 1024; // 256MB limit

    template<typename T>
    void write_json_value(json& current, const char* name, const T& value) {
        if (current.is_array()) {
            current.push_back(value);
        } else {
            current[name] = value;
        }
    }

    template<typename T>
    void read_json_value(const json& current, const char* name, std::vector<size_t>& array_indices, T& value) {
        if (current.is_array()) {
            if (array_indices.empty()) {
                return;
            }

            const size_t index = array_indices.back();
            if (index < current.size()) {
                value = current[index].get<T>();
                array_indices.back() = index + 1;
            }
            return;
        }

        if (current.contains(name)) {
            value = current[name].get<T>();
        }
    }
}

// JsonArchive implementation
JsonArchive::JsonArchive()
    : m_writing(true)
{
    m_root = json::object();
    m_stack.push_back(&m_root);
}

JsonArchive::JsonArchive(const std::string& json_str)
    : m_writing(false)
{
    m_root = json::parse(json_str);
    m_stack.push_back(&m_root);
}

JsonArchive::JsonArchive(const json& j)
    : m_root(j)
    , m_writing(false)
{
    m_stack.push_back(&m_root);
}

json& JsonArchive::current() {
    return *m_stack.back();
}

const json& JsonArchive::current() const {
    return *m_stack.back();
}

void JsonArchive::serialize(const char* name, bool& value) {
    if (m_writing) {
        write_json_value(current(), name, value);
    } else {
        read_json_value(current(), name, m_array_indices, value);
    }
}

void JsonArchive::serialize(const char* name, int32_t& value) {
    if (m_writing) {
        write_json_value(current(), name, value);
    } else {
        read_json_value(current(), name, m_array_indices, value);
    }
}

void JsonArchive::serialize(const char* name, uint32_t& value) {
    if (m_writing) {
        write_json_value(current(), name, value);
    } else {
        read_json_value(current(), name, m_array_indices, value);
    }
}

void JsonArchive::serialize(const char* name, int64_t& value) {
    if (m_writing) {
        write_json_value(current(), name, value);
    } else {
        read_json_value(current(), name, m_array_indices, value);
    }
}

void JsonArchive::serialize(const char* name, uint64_t& value) {
    if (m_writing) {
        write_json_value(current(), name, value);
    } else {
        read_json_value(current(), name, m_array_indices, value);
    }
}

void JsonArchive::serialize(const char* name, float& value) {
    if (m_writing) {
        write_json_value(current(), name, value);
    } else {
        read_json_value(current(), name, m_array_indices, value);
    }
}

void JsonArchive::serialize(const char* name, double& value) {
    if (m_writing) {
        write_json_value(current(), name, value);
    } else {
        read_json_value(current(), name, m_array_indices, value);
    }
}

void JsonArchive::serialize(const char* name, std::string& value) {
    if (m_writing) {
        write_json_value(current(), name, value);
    } else {
        read_json_value(current(), name, m_array_indices, value);
    }
}

void JsonArchive::serialize(const char* name, Vec2& value) {
    if (m_writing) {
        write_json_value(current(), name, json::array({value.x, value.y}));
    } else {
        const json* arr = nullptr;
        if (current().is_array()) {
            if (m_array_indices.empty()) {
                return;
            }
            const size_t index = m_array_indices.back();
            if (index < current().size() && current()[index].is_array()) {
                arr = &current()[index];
                m_array_indices.back() = index + 1;
            }
        } else if (current().contains(name) && current()[name].is_array()) {
            arr = &current()[name];
        }

        if (arr && arr->size() >= 2) {
            value.x = (*arr)[0].get<float>();
            value.y = (*arr)[1].get<float>();
        }
    }
}

void JsonArchive::serialize(const char* name, Vec3& value) {
    if (m_writing) {
        write_json_value(current(), name, json::array({value.x, value.y, value.z}));
    } else {
        const json* arr = nullptr;
        if (current().is_array()) {
            if (m_array_indices.empty()) {
                return;
            }
            const size_t index = m_array_indices.back();
            if (index < current().size() && current()[index].is_array()) {
                arr = &current()[index];
                m_array_indices.back() = index + 1;
            }
        } else if (current().contains(name) && current()[name].is_array()) {
            arr = &current()[name];
        }

        if (arr && arr->size() >= 3) {
            value.x = (*arr)[0].get<float>();
            value.y = (*arr)[1].get<float>();
            value.z = (*arr)[2].get<float>();
        }
    }
}

void JsonArchive::serialize(const char* name, Vec4& value) {
    if (m_writing) {
        write_json_value(current(), name, json::array({value.x, value.y, value.z, value.w}));
    } else {
        const json* arr = nullptr;
        if (current().is_array()) {
            if (m_array_indices.empty()) {
                return;
            }
            const size_t index = m_array_indices.back();
            if (index < current().size() && current()[index].is_array()) {
                arr = &current()[index];
                m_array_indices.back() = index + 1;
            }
        } else if (current().contains(name) && current()[name].is_array()) {
            arr = &current()[name];
        }

        if (arr && arr->size() >= 4) {
            value.x = (*arr)[0].get<float>();
            value.y = (*arr)[1].get<float>();
            value.z = (*arr)[2].get<float>();
            value.w = (*arr)[3].get<float>();
        }
    }
}

void JsonArchive::serialize(const char* name, Quat& value) {
    if (m_writing) {
        write_json_value(current(), name, json::array({value.w, value.x, value.y, value.z}));
    } else {
        const json* arr = nullptr;
        if (current().is_array()) {
            if (m_array_indices.empty()) {
                return;
            }
            const size_t index = m_array_indices.back();
            if (index < current().size() && current()[index].is_array()) {
                arr = &current()[index];
                m_array_indices.back() = index + 1;
            }
        } else if (current().contains(name) && current()[name].is_array()) {
            arr = &current()[name];
        }

        if (arr && arr->size() >= 4) {
            value.w = (*arr)[0].get<float>();
            value.x = (*arr)[1].get<float>();
            value.y = (*arr)[2].get<float>();
            value.z = (*arr)[3].get<float>();
        }
    }
}

void JsonArchive::serialize(const char* name, Mat4& value) {
    if (m_writing) {
        json arr = json::array();
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                arr.push_back(value[i][j]);
            }
        }
        write_json_value(current(), name, arr);
    } else {
        const json* arr = nullptr;
        if (current().is_array()) {
            if (m_array_indices.empty()) {
                return;
            }
            const size_t index = m_array_indices.back();
            if (index < current().size() && current()[index].is_array()) {
                arr = &current()[index];
                m_array_indices.back() = index + 1;
            }
        } else if (current().contains(name) && current()[name].is_array()) {
            arr = &current()[name];
        }

        if (arr && arr->size() >= 16) {
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    value[i][j] = (*arr)[i * 4 + j].get<float>();
                }
            }
        }
    }
}

bool JsonArchive::begin_object(const char* name) {
    if (m_writing) {
        if (current().is_array()) {
            current().push_back(json::object());
            m_stack.push_back(&current().back());
        } else {
            current()[name] = json::object();
            m_stack.push_back(&current()[name]);
        }
        return true;
    } else {
        if (current().is_array()) {
            if (m_array_indices.empty()) {
                return false;
            }
            const size_t index = m_array_indices.back();
            if (index < current().size() && current()[index].is_object()) {
                m_array_indices.back() = index + 1;
                m_stack.push_back(&current()[index]);
                return true;
            }
            return false;
        }

        if (current().contains(name) && current()[name].is_object()) {
            m_stack.push_back(&current()[name]);
            return true;
        }
        return false;
    }
}

void JsonArchive::end_object() {
    if (m_stack.size() > 1) {
        m_stack.pop_back();
    }
}

size_t JsonArchive::begin_array(const char* name, size_t count) {
    if (m_writing) {
        if (current().is_array()) {
            current().push_back(json::array());
            m_stack.push_back(&current().back());
        } else {
            current()[name] = json::array();
            m_stack.push_back(&current()[name]);
        }
        m_array_indices.push_back(0);
        return count;  // JSON doesn't need to store count, but return it for consistency
    } else {
        if (current().is_array()) {
            if (m_array_indices.empty()) {
                return 0;
            }
            const size_t index = m_array_indices.back();
            if (index < current().size() && current()[index].is_array()) {
                json* array_ptr = &current()[index];
                m_array_indices.back() = index + 1;
                m_stack.push_back(array_ptr);
                m_array_indices.push_back(0);
                return array_ptr->size();
            }
            return 0;
        }

        if (current().contains(name) && current()[name].is_array()) {
            json* array_ptr = &current()[name];
            m_stack.push_back(array_ptr);
            m_array_indices.push_back(0);
            return array_ptr->size();
        }
        return 0;
    }
}

void JsonArchive::end_array() {
    if (m_stack.size() > 1) {
        m_stack.pop_back();
    }
    if (!m_array_indices.empty()) {
        m_array_indices.pop_back();
    }
}

std::string JsonArchive::to_string(int indent) const {
    return m_root.dump(indent);
}

// BinaryArchive implementation
BinaryArchive::BinaryArchive()
    : m_writing(true)
{
}

BinaryArchive::BinaryArchive(const std::vector<uint8_t>& data)
    : m_data(data)
    , m_writing(false)
{
}

void BinaryArchive::write_bytes(const void* data, size_t size) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    m_data.insert(m_data.end(), ptr, ptr + size);
}

void BinaryArchive::read_bytes(void* data, size_t size) {
    if (m_read_pos + size <= m_data.size()) {
        std::memcpy(data, m_data.data() + m_read_pos, size);
        m_read_pos += size;
    }
}

void BinaryArchive::serialize(const char*, bool& value) {
    if (m_writing) {
        uint8_t v = value ? 1 : 0;
        write_bytes(&v, 1);
    } else {
        uint8_t v;
        read_bytes(&v, 1);
        value = v != 0;
    }
}

void BinaryArchive::serialize(const char*, int32_t& value) {
    if (m_writing) {
        write_bytes(&value, sizeof(value));
    } else {
        read_bytes(&value, sizeof(value));
    }
}

void BinaryArchive::serialize(const char*, uint32_t& value) {
    if (m_writing) {
        write_bytes(&value, sizeof(value));
    } else {
        read_bytes(&value, sizeof(value));
    }
}

void BinaryArchive::serialize(const char*, int64_t& value) {
    if (m_writing) {
        write_bytes(&value, sizeof(value));
    } else {
        read_bytes(&value, sizeof(value));
    }
}

void BinaryArchive::serialize(const char*, uint64_t& value) {
    if (m_writing) {
        write_bytes(&value, sizeof(value));
    } else {
        read_bytes(&value, sizeof(value));
    }
}

void BinaryArchive::serialize(const char*, float& value) {
    if (m_writing) {
        write_bytes(&value, sizeof(value));
    } else {
        read_bytes(&value, sizeof(value));
    }
}

void BinaryArchive::serialize(const char*, double& value) {
    if (m_writing) {
        write_bytes(&value, sizeof(value));
    } else {
        read_bytes(&value, sizeof(value));
    }
}

void BinaryArchive::serialize(const char*, std::string& value) {
    if (m_writing) {
        uint32_t len = static_cast<uint32_t>(value.size());
        write_bytes(&len, sizeof(len));
        write_bytes(value.data(), len);
    } else {
        uint32_t len;
        read_bytes(&len, sizeof(len));
        if (len > MAX_SERIALIZED_SIZE) {
            value.clear();
            return;
        }
        value.resize(len);
        read_bytes(value.data(), len);
    }
}

void BinaryArchive::serialize(const char*, Vec2& value) {
    if (m_writing) {
        write_bytes(&value, sizeof(value));
    } else {
        read_bytes(&value, sizeof(value));
    }
}

void BinaryArchive::serialize(const char*, Vec3& value) {
    if (m_writing) {
        write_bytes(&value, sizeof(value));
    } else {
        read_bytes(&value, sizeof(value));
    }
}

void BinaryArchive::serialize(const char*, Vec4& value) {
    if (m_writing) {
        write_bytes(&value, sizeof(value));
    } else {
        read_bytes(&value, sizeof(value));
    }
}

void BinaryArchive::serialize(const char*, Quat& value) {
    if (m_writing) {
        write_bytes(&value, sizeof(value));
    } else {
        read_bytes(&value, sizeof(value));
    }
}

void BinaryArchive::serialize(const char*, Mat4& value) {
    if (m_writing) {
        write_bytes(&value, sizeof(value));
    } else {
        read_bytes(&value, sizeof(value));
    }
}

bool BinaryArchive::begin_object(const char*) {
    return true;  // No-op for binary
}

void BinaryArchive::end_object() {
    // No-op for binary
}

size_t BinaryArchive::begin_array(const char*, size_t count) {
    if (m_writing) {
        uint32_t size = static_cast<uint32_t>(count);
        write_bytes(&size, sizeof(size));
        return count;
    } else {
        uint32_t size;
        read_bytes(&size, sizeof(size));
        if (size > MAX_SERIALIZED_SIZE) return 0;
        return size;
    }
}

void BinaryArchive::end_array() {
    // No-op for binary
}

} // namespace engine::core
