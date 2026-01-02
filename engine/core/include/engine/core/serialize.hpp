#pragma once

#include <engine/core/math.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <type_traits>

namespace engine::core {

using json = nlohmann::json;

// Serialization archive interface
class IArchive {
public:
    virtual ~IArchive() = default;

    virtual bool is_writing() const = 0;
    virtual bool is_reading() const { return !is_writing(); }

    // Primitive types
    virtual void serialize(const char* name, bool& value) = 0;
    virtual void serialize(const char* name, int32_t& value) = 0;
    virtual void serialize(const char* name, uint32_t& value) = 0;
    virtual void serialize(const char* name, int64_t& value) = 0;
    virtual void serialize(const char* name, uint64_t& value) = 0;
    virtual void serialize(const char* name, float& value) = 0;
    virtual void serialize(const char* name, double& value) = 0;
    virtual void serialize(const char* name, std::string& value) = 0;

    // Math types
    virtual void serialize(const char* name, Vec2& value) = 0;
    virtual void serialize(const char* name, Vec3& value) = 0;
    virtual void serialize(const char* name, Vec4& value) = 0;
    virtual void serialize(const char* name, Quat& value) = 0;
    virtual void serialize(const char* name, Mat4& value) = 0;

    // Begin/end object for nested structures
    virtual bool begin_object(const char* name) = 0;
    virtual void end_object() = 0;

    // Array support
    // count: When writing, pass the array size to serialize. When reading, count is ignored.
    // Returns: When reading, returns the array size from the data. When writing, returns count.
    virtual size_t begin_array(const char* name, size_t count = 0) = 0;
    virtual void end_array() = 0;
};

// JSON Archive for reading/writing JSON
class JsonArchive : public IArchive {
public:
    // Create a writing archive
    JsonArchive();

    // Create a reading archive from JSON string
    explicit JsonArchive(const std::string& json_str);

    // Create a reading archive from JSON object
    explicit JsonArchive(const json& j);

    bool is_writing() const override { return m_writing; }

    void serialize(const char* name, bool& value) override;
    void serialize(const char* name, int32_t& value) override;
    void serialize(const char* name, uint32_t& value) override;
    void serialize(const char* name, int64_t& value) override;
    void serialize(const char* name, uint64_t& value) override;
    void serialize(const char* name, float& value) override;
    void serialize(const char* name, double& value) override;
    void serialize(const char* name, std::string& value) override;

    void serialize(const char* name, Vec2& value) override;
    void serialize(const char* name, Vec3& value) override;
    void serialize(const char* name, Vec4& value) override;
    void serialize(const char* name, Quat& value) override;
    void serialize(const char* name, Mat4& value) override;

    bool begin_object(const char* name) override;
    void end_object() override;

    size_t begin_array(const char* name, size_t count = 0) override;
    void end_array() override;

    // Get the JSON string (for writing)
    std::string to_string(int indent = 4) const;

    // Get the JSON object
    const json& get_json() const { return m_root; }

private:
    json& current();
    const json& current() const;

    json m_root;
    std::vector<json*> m_stack;
    bool m_writing;
    size_t m_array_index = 0;
};

// Binary Archive for compact serialization
class BinaryArchive : public IArchive {
public:
    // Create a writing archive
    BinaryArchive();

    // Create a reading archive from binary data
    explicit BinaryArchive(const std::vector<uint8_t>& data);

    bool is_writing() const override { return m_writing; }

    void serialize(const char* name, bool& value) override;
    void serialize(const char* name, int32_t& value) override;
    void serialize(const char* name, uint32_t& value) override;
    void serialize(const char* name, int64_t& value) override;
    void serialize(const char* name, uint64_t& value) override;
    void serialize(const char* name, float& value) override;
    void serialize(const char* name, double& value) override;
    void serialize(const char* name, std::string& value) override;

    void serialize(const char* name, Vec2& value) override;
    void serialize(const char* name, Vec3& value) override;
    void serialize(const char* name, Vec4& value) override;
    void serialize(const char* name, Quat& value) override;
    void serialize(const char* name, Mat4& value) override;

    bool begin_object(const char* name) override;
    void end_object() override;

    size_t begin_array(const char* name, size_t count = 0) override;
    void end_array() override;

    // Get the binary data (for writing)
    const std::vector<uint8_t>& get_data() const { return m_data; }

private:
    void write_bytes(const void* data, size_t size);
    void read_bytes(void* data, size_t size);

    std::vector<uint8_t> m_data;
    size_t m_read_pos = 0;
    bool m_writing;
};

} // namespace engine::core
