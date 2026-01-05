#include <engine/data/data_table.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>

// Include nlohmann/json
#include <nlohmann/json.hpp>

namespace engine::data {

// Static null value for invalid accesses
DataValue DataTable::s_null_value;

// ============================================================================
// DataValue Implementation
// ============================================================================

std::string DataValue::to_string() const {
    switch (m_type) {
        case Type::Null:
            return "";
        case Type::Bool:
            return std::get<bool>(m_value) ? "true" : "false";
        case Type::Int:
            return std::to_string(std::get<int64_t>(m_value));
        case Type::Float:
            return std::to_string(std::get<double>(m_value));
        case Type::String:
            return std::get<std::string>(m_value);
        case Type::AssetId:
            return std::get<core::UUID>(m_value).to_string();
        default:
            return "";
    }
}

DataValue DataValue::parse(const std::string& str, Type hint) {
    if (str.empty()) {
        return DataValue();
    }

    switch (hint) {
        case Type::Bool: {
            std::string lower = str;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower == "true" || lower == "1" || lower == "yes") {
                return DataValue(true);
            }
            return DataValue(false);
        }
        case Type::Int: {
            try {
                return DataValue(std::stoll(str));
            } catch (...) {
                return DataValue(int64_t(0));
            }
        }
        case Type::Float: {
            try {
                return DataValue(std::stod(str));
            } catch (...) {
                return DataValue(0.0);
            }
        }
        case Type::AssetId: {
            // Parse UUID from string
            return DataValue(core::UUID::from_string(str));
        }
        case Type::String:
        default:
            return DataValue(str);
    }
}

// ============================================================================
// DataRow Implementation
// ============================================================================

DataRow::DataRow(const DataTable* table, size_t row_index)
    : m_table(table), m_row_index(row_index) {}

const DataValue& DataRow::operator[](const std::string& column) const {
    return get(column);
}

const DataValue& DataRow::get(const std::string& column) const {
    if (!m_table) {
        static DataValue null_value;
        return null_value;
    }
    return m_table->get_cell(m_row_index, column);
}

bool DataRow::has(const std::string& column) const {
    return m_table && m_table->has_column(column);
}

bool DataRow::get_bool(const std::string& col, bool def) const {
    return get(col).get_bool(def);
}

int64_t DataRow::get_int(const std::string& col, int64_t def) const {
    return get(col).get_int(def);
}

double DataRow::get_float(const std::string& col, double def) const {
    return get(col).get_float(def);
}

std::string DataRow::get_string(const std::string& col, const std::string& def) const {
    return get(col).get_string(def);
}

core::UUID DataRow::get_asset(const std::string& col) const {
    return get(col).get_asset();
}

std::string DataRow::get_id() const {
    if (!m_table) return "";

    const auto& id_col = m_table->get_id_column();
    if (!id_col.empty()) {
        return get_string(id_col);
    }

    // Default to first column
    const auto& columns = m_table->get_columns();
    if (!columns.empty()) {
        return get_string(columns[0].name);
    }

    return "";
}

// ============================================================================
// DataTable Implementation
// ============================================================================

DataTable::DataTable() = default;

void DataTable::define_column(const ColumnDef& def) {
    m_column_indices[def.name] = m_columns.size();
    m_columns.push_back(def);
}

void DataTable::define_column(const std::string& name, DataValue::Type type) {
    ColumnDef def;
    def.name = name;
    def.type = type;
    define_column(def);
}

void DataTable::set_id_column(const std::string& column) {
    m_id_column = column;
}

bool DataTable::has_column(const std::string& name) const {
    return m_column_indices.find(name) != m_column_indices.end();
}

size_t DataTable::get_column_index(const std::string& name) const {
    auto it = m_column_indices.find(name);
    return it != m_column_indices.end() ? it->second : static_cast<size_t>(-1);
}

bool DataTable::load_csv(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    m_source_path = path;

    std::stringstream buffer;
    buffer << file.rdbuf();
    return load_csv_string(buffer.str());
}

bool DataTable::load_csv_string(const std::string& content) {
    m_rows.clear();
    m_id_index.clear();

    std::istringstream stream(content);
    std::string line;

    // Parse header line
    if (!std::getline(stream, line)) {
        return false;
    }

    // Remove BOM if present
    if (line.size() >= 3 &&
        static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB &&
        static_cast<unsigned char>(line[2]) == 0xBF) {
        line = line.substr(3);
    }

    // Trim trailing \r if present
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    // Parse column names from header
    std::vector<std::string> header_names;
    {
        std::istringstream header_stream(line);
        std::string col;
        while (std::getline(header_stream, col, ',')) {
            // Trim whitespace
            col.erase(0, col.find_first_not_of(" \t"));
            col.erase(col.find_last_not_of(" \t") + 1);
            header_names.push_back(col);
        }
    }

    // Create column definitions if not already defined
    if (m_columns.empty()) {
        for (const auto& name : header_names) {
            define_column(name, DataValue::Type::String);
        }
    }

    // Set default ID column to first column
    if (m_id_column.empty() && !header_names.empty()) {
        m_id_column = header_names[0];
    }

    // Parse data rows
    while (std::getline(stream, line)) {
        // Skip empty lines
        if (line.empty() || (line.size() == 1 && line[0] == '\r')) {
            continue;
        }

        // Trim trailing \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::vector<DataValue> row;
        std::istringstream row_stream(line);
        std::string cell;
        size_t col_idx = 0;

        while (std::getline(row_stream, cell, ',')) {
            // Trim whitespace
            cell.erase(0, cell.find_first_not_of(" \t"));
            cell.erase(cell.find_last_not_of(" \t") + 1);

            DataValue::Type type = DataValue::Type::String;
            if (col_idx < m_columns.size()) {
                type = m_columns[col_idx].type;
            }

            row.push_back(parse_value(cell, type));
            col_idx++;
        }

        // Pad row if needed
        while (row.size() < m_columns.size()) {
            row.push_back(DataValue());
        }

        m_rows.push_back(std::move(row));
    }

    build_id_index();

    // Update modified time
    if (!m_source_path.empty() && std::filesystem::exists(m_source_path)) {
        auto ftime = std::filesystem::last_write_time(m_source_path);
        m_last_modified = ftime.time_since_epoch().count();
    }

    return true;
}

bool DataTable::load_json(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    m_source_path = path;

    std::stringstream buffer;
    buffer << file.rdbuf();
    return load_json_string(buffer.str());
}

bool DataTable::load_json_string(const std::string& content) {
    m_rows.clear();
    m_id_index.clear();

    try {
        auto json = nlohmann::json::parse(content);

        // Expect array of objects
        if (!json.is_array()) {
            return false;
        }

        // Auto-detect columns from first row if not defined
        if (m_columns.empty() && !json.empty() && json[0].is_object()) {
            for (auto& [key, value] : json[0].items()) {
                DataValue::Type type = DataValue::Type::String;
                if (value.is_boolean()) type = DataValue::Type::Bool;
                else if (value.is_number_integer()) type = DataValue::Type::Int;
                else if (value.is_number_float()) type = DataValue::Type::Float;

                define_column(key, type);
            }
        }

        // Set default ID column
        if (m_id_column.empty() && !m_columns.empty()) {
            // Look for common ID column names
            for (const auto& col : m_columns) {
                if (col.name == "id" || col.name == "ID" || col.name == "Id") {
                    m_id_column = col.name;
                    break;
                }
            }
            if (m_id_column.empty()) {
                m_id_column = m_columns[0].name;
            }
        }

        // Parse rows
        for (const auto& obj : json) {
            if (!obj.is_object()) continue;

            std::vector<DataValue> row;
            for (const auto& col : m_columns) {
                if (obj.contains(col.name)) {
                    const auto& value = obj[col.name];
                    if (value.is_null()) {
                        row.push_back(DataValue());
                    } else if (value.is_boolean()) {
                        row.push_back(DataValue(value.get<bool>()));
                    } else if (value.is_number_integer()) {
                        row.push_back(DataValue(value.get<int64_t>()));
                    } else if (value.is_number_float()) {
                        row.push_back(DataValue(value.get<double>()));
                    } else if (value.is_string()) {
                        row.push_back(parse_value(value.get<std::string>(), col.type));
                    } else {
                        row.push_back(DataValue());
                    }
                } else {
                    row.push_back(col.default_value);
                }
            }
            m_rows.push_back(std::move(row));
        }

        build_id_index();

        // Update modified time
        if (!m_source_path.empty() && std::filesystem::exists(m_source_path)) {
            auto ftime = std::filesystem::last_write_time(m_source_path);
            m_last_modified = ftime.time_since_epoch().count();
        }

        return true;
    } catch (...) {
        return false;
    }
}

DataRow DataTable::get_row(size_t index) const {
    if (index >= m_rows.size()) {
        return DataRow(nullptr, 0);
    }
    return DataRow(this, index);
}

DataRow DataTable::find_row(const std::string& id) const {
    auto it = m_id_index.find(id);
    if (it != m_id_index.end()) {
        return DataRow(this, it->second);
    }
    return DataRow(nullptr, 0);
}

bool DataTable::has_row(const std::string& id) const {
    return m_id_index.find(id) != m_id_index.end();
}

std::vector<DataRow> DataTable::find_rows(const std::string& column, const DataValue& value) const {
    std::vector<DataRow> results;

    size_t col_idx = get_column_index(column);
    if (col_idx == static_cast<size_t>(-1)) {
        return results;
    }

    for (size_t i = 0; i < m_rows.size(); ++i) {
        const auto& cell = m_rows[i][col_idx];
        bool match = false;

        // Compare based on type
        if (cell.type() == value.type()) {
            switch (cell.type()) {
                case DataValue::Type::Bool:
                    match = cell.get_bool() == value.get_bool();
                    break;
                case DataValue::Type::Int:
                    match = cell.get_int() == value.get_int();
                    break;
                case DataValue::Type::Float:
                    match = cell.get_float() == value.get_float();
                    break;
                case DataValue::Type::String:
                    match = cell.get_string() == value.get_string();
                    break;
                default:
                    break;
            }
        }

        if (match) {
            results.push_back(DataRow(this, i));
        }
    }

    return results;
}

std::vector<DataRow> DataTable::filter(std::function<bool(const DataRow&)> predicate) const {
    std::vector<DataRow> results;

    for (size_t i = 0; i < m_rows.size(); ++i) {
        DataRow row(this, i);
        if (predicate(row)) {
            results.push_back(row);
        }
    }

    return results;
}

const DataValue& DataTable::get_cell(size_t row, size_t col) const {
    if (row >= m_rows.size() || col >= m_columns.size()) {
        return s_null_value;
    }
    return m_rows[row][col];
}

const DataValue& DataTable::get_cell(size_t row, const std::string& col) const {
    size_t col_idx = get_column_index(col);
    if (col_idx == static_cast<size_t>(-1)) {
        return s_null_value;
    }
    return get_cell(row, col_idx);
}

bool DataTable::reload() {
    if (m_source_path.empty()) {
        return false;
    }

    // Determine format from extension
    std::string ext = m_source_path.substr(m_source_path.rfind('.'));
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".csv") {
        return load_csv(m_source_path);
    } else if (ext == ".json") {
        return load_json(m_source_path);
    }

    return false;
}

void DataTable::build_id_index() {
    m_id_index.clear();

    size_t id_col_idx = get_column_index(m_id_column);
    if (id_col_idx == static_cast<size_t>(-1) && !m_columns.empty()) {
        id_col_idx = 0;  // Default to first column
    }

    if (id_col_idx == static_cast<size_t>(-1)) {
        return;
    }

    for (size_t i = 0; i < m_rows.size(); ++i) {
        std::string id = m_rows[i][id_col_idx].get_string();
        if (!id.empty()) {
            m_id_index[id] = i;
        }
    }
}

DataValue DataTable::parse_value(const std::string& str, DataValue::Type type) {
    return DataValue::parse(str, type);
}

// ============================================================================
// DataTableManager Implementation
// ============================================================================

DataTableManager& DataTableManager::instance() {
    static DataTableManager s_instance;
    return s_instance;
}

DataTable* DataTableManager::load(const std::string& name, const std::string& path) {
    auto table = std::make_unique<DataTable>();
    table->set_name(name);

    // Determine format from extension
    std::string ext = path.substr(path.rfind('.'));
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    bool loaded = false;
    if (ext == ".csv") {
        loaded = table->load_csv(path);
    } else if (ext == ".json") {
        loaded = table->load_json(path);
    }

    if (!loaded) {
        return nullptr;
    }

    auto* ptr = table.get();
    m_tables[name] = std::move(table);
    return ptr;
}

DataTable* DataTableManager::get(const std::string& name) {
    auto it = m_tables.find(name);
    return it != m_tables.end() ? it->second.get() : nullptr;
}

const DataTable* DataTableManager::get(const std::string& name) const {
    auto it = m_tables.find(name);
    return it != m_tables.end() ? it->second.get() : nullptr;
}

bool DataTableManager::has(const std::string& name) const {
    return m_tables.find(name) != m_tables.end();
}

void DataTableManager::unload(const std::string& name) {
    m_tables.erase(name);
}

void DataTableManager::clear() {
    m_tables.clear();
}

void DataTableManager::poll_changes() {
    if (!m_hot_reload_enabled) {
        return;
    }

    for (auto& [name, table] : m_tables) {
        const std::string& path = table->get_source_path();
        if (path.empty() || !std::filesystem::exists(path)) {
            continue;
        }

        auto ftime = std::filesystem::last_write_time(path);
        uint64_t current_time = ftime.time_since_epoch().count();

        if (current_time > table->get_last_modified()) {
            table->reload();
        }
    }
}

void DataTableManager::reload_all() {
    for (auto& [name, table] : m_tables) {
        table->reload();
    }
}

std::vector<std::string> DataTableManager::get_table_names() const {
    std::vector<std::string> names;
    names.reserve(m_tables.size());
    for (const auto& [name, table] : m_tables) {
        names.push_back(name);
    }
    return names;
}

size_t DataTableManager::table_count() const {
    return m_tables.size();
}

} // namespace engine::data
