#pragma once

#include <engine/core/uuid.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <variant>
#include <optional>
#include <stdexcept>

namespace engine::data {

// ============================================================================
// DataValue - Variant type for table cell values
// ============================================================================

class DataValue {
public:
    enum class Type {
        Null,
        Bool,
        Int,
        Float,
        String,
        AssetId
    };

    DataValue() : m_type(Type::Null) {}
    explicit DataValue(bool v) : m_type(Type::Bool), m_value(v) {}
    explicit DataValue(int64_t v) : m_type(Type::Int), m_value(v) {}
    explicit DataValue(int v) : m_type(Type::Int), m_value(static_cast<int64_t>(v)) {}
    explicit DataValue(double v) : m_type(Type::Float), m_value(v) {}
    explicit DataValue(float v) : m_type(Type::Float), m_value(static_cast<double>(v)) {}
    explicit DataValue(const std::string& v) : m_type(Type::String), m_value(v) {}
    explicit DataValue(std::string&& v) : m_type(Type::String), m_value(std::move(v)) {}
    explicit DataValue(const char* v) : m_type(Type::String), m_value(std::string(v)) {}
    explicit DataValue(core::UUID v) : m_type(Type::AssetId), m_value(v) {}

    Type type() const { return m_type; }
    bool is_null() const { return m_type == Type::Null; }
    bool is_bool() const { return m_type == Type::Bool; }
    bool is_int() const { return m_type == Type::Int; }
    bool is_float() const { return m_type == Type::Float; }
    bool is_string() const { return m_type == Type::String; }
    bool is_asset() const { return m_type == Type::AssetId; }
    bool is_numeric() const { return m_type == Type::Int || m_type == Type::Float; }

    // Type-checked getters (throw on type mismatch)
    bool as_bool() const {
        if (m_type != Type::Bool) throw std::runtime_error("DataValue is not a bool");
        return std::get<bool>(m_value);
    }

    int64_t as_int() const {
        if (m_type != Type::Int) throw std::runtime_error("DataValue is not an int");
        return std::get<int64_t>(m_value);
    }

    double as_float() const {
        if (m_type == Type::Float) return std::get<double>(m_value);
        if (m_type == Type::Int) return static_cast<double>(std::get<int64_t>(m_value));
        throw std::runtime_error("DataValue is not numeric");
    }

    const std::string& as_string() const {
        if (m_type != Type::String) throw std::runtime_error("DataValue is not a string");
        return std::get<std::string>(m_value);
    }

    core::UUID as_asset() const {
        if (m_type != Type::AssetId) throw std::runtime_error("DataValue is not an asset ID");
        return std::get<core::UUID>(m_value);
    }

    // Safe getters with defaults
    bool get_bool(bool def = false) const {
        return m_type == Type::Bool ? std::get<bool>(m_value) : def;
    }

    int64_t get_int(int64_t def = 0) const {
        return m_type == Type::Int ? std::get<int64_t>(m_value) : def;
    }

    double get_float(double def = 0.0) const {
        if (m_type == Type::Float) return std::get<double>(m_value);
        if (m_type == Type::Int) return static_cast<double>(std::get<int64_t>(m_value));
        return def;
    }

    std::string get_string(const std::string& def = "") const {
        return m_type == Type::String ? std::get<std::string>(m_value) : def;
    }

    core::UUID get_asset(core::UUID def = {}) const {
        return m_type == Type::AssetId ? std::get<core::UUID>(m_value) : def;
    }

    // Convert to string representation
    std::string to_string() const;

    // Parse from string (with type hint)
    static DataValue parse(const std::string& str, Type hint = Type::String);

private:
    Type m_type;
    std::variant<std::monostate, bool, int64_t, double, std::string, core::UUID> m_value;
};

// ============================================================================
// ColumnDef - Column definition/schema
// ============================================================================

struct ColumnDef {
    std::string name;
    DataValue::Type type = DataValue::Type::String;
    bool required = false;
    DataValue default_value;

    ColumnDef() = default;
    ColumnDef(std::string n, DataValue::Type t)
        : name(std::move(n)), type(t) {}
    ColumnDef(std::string n, DataValue::Type t, DataValue def)
        : name(std::move(n)), type(t), default_value(std::move(def)) {}
};

// ============================================================================
// Forward declarations
// ============================================================================

class DataTable;

// ============================================================================
// DataRow - Lightweight view into a table row
// ============================================================================

class DataRow {
public:
    DataRow(const DataTable* table, size_t row_index);

    // Column access by name
    const DataValue& operator[](const std::string& column) const;
    const DataValue& get(const std::string& column) const;

    // Check if column exists
    bool has(const std::string& column) const;

    // Typed accessors
    bool get_bool(const std::string& col, bool def = false) const;
    int64_t get_int(const std::string& col, int64_t def = 0) const;
    double get_float(const std::string& col, double def = 0.0) const;
    std::string get_string(const std::string& col, const std::string& def = "") const;
    core::UUID get_asset(const std::string& col) const;

    // Row ID (first column by convention, or explicit ID column)
    std::string get_id() const;

    // Row index
    size_t index() const { return m_row_index; }

    // Check if row is valid
    bool valid() const { return m_table != nullptr; }

private:
    const DataTable* m_table;
    size_t m_row_index;
};

// ============================================================================
// DataTable - Tabular data structure
// ============================================================================

class DataTable {
public:
    DataTable();
    ~DataTable() = default;

    // Non-copyable but movable
    DataTable(const DataTable&) = delete;
    DataTable& operator=(const DataTable&) = delete;
    DataTable(DataTable&&) = default;
    DataTable& operator=(DataTable&&) = default;

    // ========================================================================
    // Loading
    // ========================================================================

    // Load from CSV file
    bool load_csv(const std::string& path);

    // Load from JSON file
    bool load_json(const std::string& path);

    // Load from string content
    bool load_csv_string(const std::string& content);
    bool load_json_string(const std::string& content);

    // ========================================================================
    // Schema
    // ========================================================================

    // Define column (must be called before loading for type validation)
    void define_column(const ColumnDef& def);
    void define_column(const std::string& name, DataValue::Type type);

    // Set which column is the ID column (for find_row lookups)
    void set_id_column(const std::string& column);
    const std::string& get_id_column() const { return m_id_column; }

    // Get schema
    const std::vector<ColumnDef>& get_columns() const { return m_columns; }
    bool has_column(const std::string& name) const;
    size_t get_column_index(const std::string& name) const;

    // ========================================================================
    // Row Access
    // ========================================================================

    // Get row count
    size_t row_count() const { return m_rows.size(); }
    bool empty() const { return m_rows.empty(); }

    // Get row by index
    DataRow get_row(size_t index) const;

    // Find row by ID column value
    DataRow find_row(const std::string& id) const;

    // Check if row with ID exists
    bool has_row(const std::string& id) const;

    // ========================================================================
    // Query
    // ========================================================================

    // Find all rows where column equals value
    std::vector<DataRow> find_rows(const std::string& column, const DataValue& value) const;

    // Find all rows matching predicate
    std::vector<DataRow> filter(std::function<bool(const DataRow&)> predicate) const;

    // ========================================================================
    // Iteration
    // ========================================================================

    class Iterator {
    public:
        Iterator(const DataTable* table, size_t index) : m_table(table), m_index(index) {}

        DataRow operator*() const { return DataRow(m_table, m_index); }
        Iterator& operator++() { ++m_index; return *this; }
        bool operator!=(const Iterator& other) const { return m_index != other.m_index; }

    private:
        const DataTable* m_table;
        size_t m_index;
    };

    Iterator begin() const { return Iterator(this, 0); }
    Iterator end() const { return Iterator(this, m_rows.size()); }

    // ========================================================================
    // Metadata
    // ========================================================================

    const std::string& get_name() const { return m_name; }
    void set_name(const std::string& name) { m_name = name; }

    const std::string& get_source_path() const { return m_source_path; }
    void set_source_path(const std::string& path) { m_source_path = path; }

    // ========================================================================
    // Hot Reload
    // ========================================================================

    bool reload();
    uint64_t get_last_modified() const { return m_last_modified; }

    // ========================================================================
    // Internal Access (for DataRow)
    // ========================================================================

    const DataValue& get_cell(size_t row, size_t col) const;
    const DataValue& get_cell(size_t row, const std::string& col) const;

private:
    void build_id_index();
    DataValue parse_value(const std::string& str, DataValue::Type type);

    std::string m_name;
    std::string m_source_path;
    std::string m_id_column;

    std::vector<ColumnDef> m_columns;
    std::unordered_map<std::string, size_t> m_column_indices;

    std::vector<std::vector<DataValue>> m_rows;
    std::unordered_map<std::string, size_t> m_id_index;

    uint64_t m_last_modified = 0;
    static DataValue s_null_value;
};

// ============================================================================
// DataTableManager - Manages multiple tables
// ============================================================================

class DataTableManager {
public:
    // Singleton access
    static DataTableManager& instance();

    // Delete copy/move
    DataTableManager(const DataTableManager&) = delete;
    DataTableManager& operator=(const DataTableManager&) = delete;
    DataTableManager(DataTableManager&&) = delete;
    DataTableManager& operator=(DataTableManager&&) = delete;

    // ========================================================================
    // Table Management
    // ========================================================================

    // Load table from file
    DataTable* load(const std::string& name, const std::string& path);

    // Get table by name
    DataTable* get(const std::string& name);
    const DataTable* get(const std::string& name) const;

    // Check if table exists
    bool has(const std::string& name) const;

    // Unload table
    void unload(const std::string& name);

    // Clear all tables
    void clear();

    // ========================================================================
    // Hot Reload
    // ========================================================================

    // Enable/disable hot reload
    void enable_hot_reload(bool enabled) { m_hot_reload_enabled = enabled; }
    bool is_hot_reload_enabled() const { return m_hot_reload_enabled; }

    // Check for modified files and reload
    void poll_changes();

    // Reload all tables
    void reload_all();

    // ========================================================================
    // Query
    // ========================================================================

    // Get list of all table names
    std::vector<std::string> get_table_names() const;

    // Get total table count
    size_t table_count() const;

private:
    DataTableManager() = default;
    ~DataTableManager() = default;

    std::unordered_map<std::string, std::unique_ptr<DataTable>> m_tables;
    bool m_hot_reload_enabled = false;
};

// ============================================================================
// Global Access
// ============================================================================

inline DataTableManager& data_tables() { return DataTableManager::instance(); }

} // namespace engine::data
