#include "core/Relationship.hpp"

#include <numeric>

namespace dbscanner::core {

namespace {
std::string joinColumns(const std::vector<std::string>& columns) {
    if (columns.empty()) return "";
    return std::accumulate(std::next(columns.begin()), columns.end(), columns.front(),
                            [](std::string acc, const std::string& col) { return acc + "," + col; });
}
}  // namespace

std::string toString(RelationshipKind kind) {
    switch (kind) {
        case RelationshipKind::Declared:
            return "Declared";
        case RelationshipKind::Inferred:
            return "Inferred";
    }
    return "Unknown";
}

std::string Relationship::id() const {
    return childTable + "(" + joinColumns(childColumns) + ") -> " + parentTable + "(" +
           joinColumns(parentColumns) + ")";
}

void to_json(nlohmann::json& j, const Relationship& r) {
    j = nlohmann::json{
        {"parentTable", r.parentTable},   {"parentColumns", r.parentColumns},
        {"childTable", r.childTable},     {"childColumns", r.childColumns},
        {"kind", toString(r.kind)},       {"confidence", r.confidence},
    };
    if (r.constraintName.has_value()) {
        j["constraintName"] = *r.constraintName;
    } else {
        j["constraintName"] = nullptr;
    }
}

void from_json(const nlohmann::json& j, Relationship& r) {
    j.at("parentTable").get_to(r.parentTable);
    j.at("parentColumns").get_to(r.parentColumns);
    j.at("childTable").get_to(r.childTable);
    j.at("childColumns").get_to(r.childColumns);
    r.kind = j.at("kind").get<std::string>() == "Inferred" ? RelationshipKind::Inferred
                                                             : RelationshipKind::Declared;
    j.at("confidence").get_to(r.confidence);
    if (j.contains("constraintName") && !j.at("constraintName").is_null()) {
        r.constraintName = j.at("constraintName").get<std::string>();
    } else {
        r.constraintName.reset();
    }
}

}  // namespace dbscanner::core
