#include "relations/RelationshipInference.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace dbscanner::relations {

namespace {

std::string toUpper(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                    [](unsigned char c) { return std::toupper(c); });
    return result;
}

enum class TypeFamily { Numeric, Text, DateTime, Other };

TypeFamily classify(const std::string& dataType) {
    const std::string t = toUpper(dataType);
    static const std::set<std::string> numeric = {"NUMBER",        "FLOAT",  "INTEGER",
                                                    "BINARY_FLOAT",  "BINARY_DOUBLE", "LONG"};
    static const std::set<std::string> text = {"VARCHAR2", "CHAR", "NVARCHAR2", "NCHAR", "CLOB"};
    static const std::set<std::string> dateTime = {"DATE", "TIMESTAMP"};

    if (numeric.count(t)) return TypeFamily::Numeric;
    if (text.count(t)) return TypeFamily::Text;
    if (t.rfind("TIMESTAMP", 0) == 0) return TypeFamily::DateTime;
    if (dateTime.count(t)) return TypeFamily::DateTime;
    return TypeFamily::Other;
}

// Candidate singular/plural spellings for a naming-convention match, e.g.
// "CUSTOMER" -> {"CUSTOMER", "CUSTOMERS"}, "CATEGORY" -> {"CATEGORY", "CATEGORIES"}.
std::vector<std::string> nameVariants(const std::string& base) {
    std::vector<std::string> variants = {base};
    if (base.size() > 1 && base.back() == 'Y' && base[base.size() - 2] != 'A' &&
        base[base.size() - 2] != 'E' && base[base.size() - 2] != 'O') {
        variants.push_back(base.substr(0, base.size() - 1) + "IES");
    } else {
        variants.push_back(base + "S");
        variants.push_back(base + "ES");
    }
    if (base.size() > 1 && base.back() == 'S') {
        variants.push_back(base.substr(0, base.size() - 1));
    }
    if (base.size() > 3 && base.substr(base.size() - 3) == "IES") {
        variants.push_back(base.substr(0, base.size() - 3) + "Y");
    }
    return variants;
}

// Strips a trailing "_ID" or "ID" suffix from a column name, returning the
// candidate table-name stem, or empty if the column doesn't look like a
// foreign key column.
std::string candidateStemFromColumn(const std::string& columnName) {
    const std::string upper = toUpper(columnName);
    if (upper.size() > 3 && upper.substr(upper.size() - 3) == "_ID") {
        return upper.substr(0, upper.size() - 3);
    }
    if (upper.size() > 2 && upper.substr(upper.size() - 2) == "ID") {
        return upper.substr(0, upper.size() - 2);
    }
    return "";
}

struct DeclaredKey {
    std::string childTable;
    std::string childColumn;

    bool operator<(const DeclaredKey& other) const {
        return std::tie(childTable, childColumn) < std::tie(other.childTable, other.childColumn);
    }
};

}  // namespace

std::vector<core::Relationship> inferRelationships(
    const std::vector<core::TableInfo>& tables,
    const std::vector<core::Relationship>& declaredRelationships) {
    std::map<std::string, const core::TableInfo*> byName;
    for (const auto& table : tables) {
        byName[toUpper(table.name)] = &table;
    }

    std::set<DeclaredKey> alreadyDeclared;
    for (const auto& rel : declaredRelationships) {
        for (const auto& col : rel.childColumns) {
            alreadyDeclared.insert({toUpper(rel.childTable), toUpper(col)});
        }
    }

    std::vector<core::Relationship> inferred;

    for (const auto& child : tables) {
        for (const auto& column : child.columns) {
            const DeclaredKey key{toUpper(child.name), toUpper(column.name)};
            if (alreadyDeclared.count(key)) continue;

            const std::string stem = candidateStemFromColumn(column.name);
            if (stem.empty()) continue;

            for (const auto& variant : nameVariants(stem)) {
                const auto it = byName.find(variant);
                if (it == byName.end()) continue;

                const core::TableInfo* parent = it->second;
                if (!parent->primaryKey.has_value() || parent->primaryKey->columns.size() != 1) {
                    continue;
                }
                const std::string& pkColumnName = parent->primaryKey->columns.front();
                const core::Column* pkColumn = parent->findColumn(pkColumnName);
                if (!pkColumn) continue;

                if (classify(column.dataType) != classify(pkColumn->dataType)) continue;
                if (classify(column.dataType) == TypeFamily::Other) continue;

                const bool exactStemMatch = (variant == toUpper(parent->name));
                double confidence = exactStemMatch ? 0.9 : 0.75;
                if (column.dataLength != pkColumn->dataLength &&
                    classify(column.dataType) == TypeFamily::Text) {
                    confidence -= 0.15;
                }

                core::Relationship rel;
                rel.parentTable = parent->name;
                rel.parentColumns = {pkColumnName};
                rel.childTable = child.name;
                rel.childColumns = {column.name};
                rel.kind = core::RelationshipKind::Inferred;
                rel.confidence = confidence;
                inferred.push_back(std::move(rel));
                break;  // stop at first matching variant for this column
            }
        }
    }

    return inferred;
}

}  // namespace dbscanner::relations
