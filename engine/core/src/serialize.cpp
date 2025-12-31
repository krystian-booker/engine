#include <engine/core/serialize.hpp>
#include <cstring>

namespace engine::core {

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
        current()[name] = value;
    } else {
        if (current().contains(name)) {
            value = current()[name].get<bool>();
        }
    }
}

void JsonArchive::serialize(const char* name, int32_t& value) {
    if (m_writing) {
        current()[name] = value;
    } else {
        if (current().contains(name)) {
            value = current()[name].get<int32_t>();
        }
    }
}

void JsonArchive::serialize(const char* name, uint32_t& value) {
    if (m_writing) {
        current()[name] = value;
    } else {
        if (current().contains(name)) {
            value = current()[name].get<uint32_t>();
        }
    }
}

void JsonArchive::serialize(const char* name, int64_t& value) {
    if (m_writing) {
        current()[name] = value;
    } else {
        if (current().contains(name)) {
            value = current()[name].get<int64_t>();
        }
    }
}

void JsonArchive::serialize(const char* name, uint64_t& value) {
    if (m_writing) {
        current()[name] = value;
    } else {
        if (current().contains(name)) {
            value = current()[name].get<uint64_t>();
        }
    }
}

void JsonArchive::serialize(const char* name, float& value) {
    if (m_writing) {
        current()[name] = value;
    } else {
        if (current().contains(name)) {
            value = current()[name].get<float>();
        }
    }
}

void JsonArchive::serialize(const char* name, double& value) {
    if (m_writing) {
        current()[name] = value;
    } else {
        if (current().contains(name)) {
            value = current()[name].get<double>();
        }
    }
}

void JsonArchive::serialize(const char* name, std::string& value) {
    if (m_writing) {
        current()[name] = value;
    } else {
        if (current().contains(name)) {
            value = current()[name].get<std::string>();
        }
    }
}

void JsonArchive::serialize(const char* name, Vec2& value) {
    if (m_writing) {
        current()[name] = {value.x, value.y};
    } else {
        if (current().contains(name) && current()[name].is_array()) {
            auto& arr = current()[name];
            if (arr.size() >= 2) {
                value.x = arr[0].get<float>();
                value.y = arr[1].get<float>();
            }
        }
    }
}

void JsonArchive::serialize(const char* name, Vec3& value) {
    if (m_writing) {
        current()[name] = {value.x, value.y, value.z};
    } else {
        if (current().contains(name) && current()[name].is_array()) {
            auto& arr = current()[name];
            if (arr.size() >= 3) {
                value.x = arr[0].get<float>();
                value.y = arr[1].get<float>();
                value.z = arr[2].get<float>();
            }
        }
    }
}

void JsonArchive::serialize(const char* name, Vec4& value) {
    if (m_writing) {
        current()[name] = {value.x, value.y, value.z, value.w};
    } else {
        if (current().contains(name) && current()[name].is_array()) {
            auto& arr = current()[name];
            if (arr.size() >= 4) {
                value.x = arr[0].get<float>();
                value.y = arr[1].get<float>();
                value.z = arr[2].get<float>();
                value.w = arr[3].get<float>();
            }
        }
    }
}

void JsonArchive::serialize(const char* name, Quat& value) {
    if (m_writing) {
        current()[name] = {value.w, value.x, value.y, value.z};
    } else {
        if (current().contains(name) && current()[name].is_array()) {
            auto& arr = current()[name];
            if (arr.size() >= 4) {
                value.w = arr[0].get<float>();
                value.x = arr[1].get<float>();
                value.y = arr[2].get<float>();
                value.z = arr[3].get<float>();
            }
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
        current()[name] = arr;
    } else {
        if (current().contains(name) && current()[name].is_array()) {
            auto& arr = current()[name];
            if (arr.size() >= 16) {
                for (int i = 0; i < 4; ++i) {
                    for (int j = 0; j < 4; ++j) {
                        value[i][j] = arr[i * 4 + j].get<float>();
                    }
                }
            }
        }
    }
}

bool JsonArchive::begin_object(const char* name) {
    if (m_writing) {
        current()[name] = json::object();
        m_stack.push_back(&current()[name]);
        return true;
    } else {
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

size_t JsonArchive::begin_array(const char* name) {
    if (m_writing) {
        current()[name] = json::array();
        m_stack.push_back(&current()[name]);
        m_array_index = 0;
        return 0;
    } else {
        if (current().contains(name) && current()[name].is_array()) {
            m_stack.push_back(&current()[name]);
            m_array_index = 0;
            return current().size();
        }
        return 0;
    }
}

void JsonArchive::end_array() {
    if (m_stack.size() > 1) {
        m_stack.pop_back();
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

size_t BinaryArchive::begin_array(const char*) {
    if (m_writing) {
        return 0;
    } else {
        uint32_t size;
        read_bytes(&size, sizeof(size));
        return size;
    }
}

void BinaryArchive::end_array() {
    // No-op for binary
}

} // namespace engine::core
