#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace dbscanner::core {

struct Column {
    std::string name;
    std::string dataType;
    int64_t dataLength = 0;
    std::optional<int> dataPrecision;
    std::optional<int> dataScale;
    bool nullable = true;
    int columnId = 0;
    std::optional<std::string> defaultValue;
    std::optional<std::string> comment;
};

struct PrimaryKey {
    std::string constraintName;
    std::vector<std::string> columns;
};

struct UniqueKey {
    std::string constraintName;
    std::vector<std::string> columns;
};

struct IndexInfo {
    std::string name;
    std::vector<std::string> columns;
    bool isUnique = false;
};

struct TableInfo {
    std::string owner;
    std::string name;
    std::vector<Column> columns;
    std::optional<PrimaryKey> primaryKey;
    std::vector<UniqueKey> uniqueKeys;
    std::vector<IndexInfo> indexes;
    std::optional<int64_t> approxRowCount;
    std::optional<std::string> comment;

    const Column* findColumn(const std::string& columnName) const {
        for (const auto& col : columns) {
            if (col.name == columnName) return &col;
        }
        return nullptr;
    }
};

void to_json(nlohmann::json& j, const Column& c);
void from_json(const nlohmann::json& j, Column& c);

void to_json(nlohmann::json& j, const PrimaryKey& pk);
void from_json(const nlohmann::json& j, PrimaryKey& pk);

void to_json(nlohmann::json& j, const UniqueKey& uk);
void from_json(const nlohmann::json& j, UniqueKey& uk);

void to_json(nlohmann::json& j, const IndexInfo& idx);
void from_json(const nlohmann::json& j, IndexInfo& idx);

void to_json(nlohmann::json& j, const TableInfo& t);
void from_json(const nlohmann::json& j, TableInfo& t);

}  // namespace dbscanner::core
