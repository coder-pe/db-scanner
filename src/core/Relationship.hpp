#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace dbscanner::core {

enum class RelationshipKind { Declared, Inferred };

std::string toString(RelationshipKind kind);

struct Relationship {
    std::string parentTable;
    std::vector<std::string> parentColumns;
    std::string childTable;
    std::vector<std::string> childColumns;
    RelationshipKind kind = RelationshipKind::Declared;
    double confidence = 1.0;
    std::optional<std::string> constraintName;  // set when kind == Declared

    std::string id() const;
};

void to_json(nlohmann::json& j, const Relationship& r);
void from_json(const nlohmann::json& j, Relationship& r);

}  // namespace dbscanner::core
