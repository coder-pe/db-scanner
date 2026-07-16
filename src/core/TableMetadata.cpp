#include "core/TableMetadata.hpp"

namespace dbscanner::core {

namespace {
template <typename T>
void setOptional(nlohmann::json& j, const char* key, const std::optional<T>& value) {
    if (value.has_value()) {
        j[key] = *value;
    } else {
        j[key] = nullptr;
    }
}

template <typename T>
void getOptional(const nlohmann::json& j, const char* key, std::optional<T>& value) {
    if (j.contains(key) && !j.at(key).is_null()) {
        value = j.at(key).get<T>();
    } else {
        value.reset();
    }
}
}  // namespace

void to_json(nlohmann::json& j, const Column& c) {
    j = nlohmann::json{
        {"name", c.name},        {"dataType", c.dataType}, {"dataLength", c.dataLength},
        {"nullable", c.nullable}, {"columnId", c.columnId},
    };
    setOptional(j, "dataPrecision", c.dataPrecision);
    setOptional(j, "dataScale", c.dataScale);
    setOptional(j, "defaultValue", c.defaultValue);
    setOptional(j, "comment", c.comment);
}

void from_json(const nlohmann::json& j, Column& c) {
    j.at("name").get_to(c.name);
    j.at("dataType").get_to(c.dataType);
    j.at("dataLength").get_to(c.dataLength);
    j.at("nullable").get_to(c.nullable);
    j.at("columnId").get_to(c.columnId);
    getOptional(j, "dataPrecision", c.dataPrecision);
    getOptional(j, "dataScale", c.dataScale);
    getOptional(j, "defaultValue", c.defaultValue);
    getOptional(j, "comment", c.comment);
}

void to_json(nlohmann::json& j, const PrimaryKey& pk) {
    j = nlohmann::json{{"constraintName", pk.constraintName}, {"columns", pk.columns}};
}

void from_json(const nlohmann::json& j, PrimaryKey& pk) {
    j.at("constraintName").get_to(pk.constraintName);
    j.at("columns").get_to(pk.columns);
}

void to_json(nlohmann::json& j, const UniqueKey& uk) {
    j = nlohmann::json{{"constraintName", uk.constraintName}, {"columns", uk.columns}};
}

void from_json(const nlohmann::json& j, UniqueKey& uk) {
    j.at("constraintName").get_to(uk.constraintName);
    j.at("columns").get_to(uk.columns);
}

void to_json(nlohmann::json& j, const IndexInfo& idx) {
    j = nlohmann::json{{"name", idx.name}, {"columns", idx.columns}, {"isUnique", idx.isUnique}};
}

void from_json(const nlohmann::json& j, IndexInfo& idx) {
    j.at("name").get_to(idx.name);
    j.at("columns").get_to(idx.columns);
    j.at("isUnique").get_to(idx.isUnique);
}

void to_json(nlohmann::json& j, const TableInfo& t) {
    j = nlohmann::json{
        {"owner", t.owner},         {"name", t.name},           {"columns", t.columns},
        {"uniqueKeys", t.uniqueKeys}, {"indexes", t.indexes},
    };
    setOptional(j, "primaryKey", t.primaryKey);
    setOptional(j, "approxRowCount", t.approxRowCount);
    setOptional(j, "comment", t.comment);
}

void from_json(const nlohmann::json& j, TableInfo& t) {
    j.at("owner").get_to(t.owner);
    j.at("name").get_to(t.name);
    j.at("columns").get_to(t.columns);
    j.at("uniqueKeys").get_to(t.uniqueKeys);
    j.at("indexes").get_to(t.indexes);
    getOptional(j, "primaryKey", t.primaryKey);
    getOptional(j, "approxRowCount", t.approxRowCount);
    getOptional(j, "comment", t.comment);
}

}  // namespace dbscanner::core
