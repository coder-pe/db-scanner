#include <gtest/gtest.h>

#include "core/TableMetadata.hpp"
#include "relations/RelationshipInference.hpp"

using namespace dbscanner::core;
using namespace dbscanner::relations;

namespace {

Column makeColumn(const std::string& name, const std::string& type, int64_t length, int columnId) {
    Column c;
    c.name = name;
    c.dataType = type;
    c.dataLength = length;
    c.nullable = true;
    c.columnId = columnId;
    return c;
}

TableInfo makeTable(const std::string& name, std::vector<Column> columns,
                     std::optional<PrimaryKey> pk = std::nullopt) {
    TableInfo t;
    t.owner = "APP";
    t.name = name;
    t.columns = std::move(columns);
    t.primaryKey = std::move(pk);
    return t;
}

}  // namespace

TEST(RelationshipInference, MatchesExactSingularTableName) {
    std::vector<TableInfo> tables = {
        makeTable("CUSTOMER", {makeColumn("ID", "NUMBER", 22, 1)}, PrimaryKey{"PK_CUSTOMER", {"ID"}}),
        makeTable("ORDERS", {makeColumn("ID", "NUMBER", 22, 1), makeColumn("CUSTOMER_ID", "NUMBER", 22, 2)},
                   PrimaryKey{"PK_ORDERS", {"ID"}}),
    };

    auto inferred = inferRelationships(tables, {});

    ASSERT_EQ(inferred.size(), 1u);
    EXPECT_EQ(inferred[0].childTable, "ORDERS");
    EXPECT_EQ(inferred[0].childColumns, std::vector<std::string>{"CUSTOMER_ID"});
    EXPECT_EQ(inferred[0].parentTable, "CUSTOMER");
    EXPECT_EQ(inferred[0].parentColumns, std::vector<std::string>{"ID"});
    EXPECT_EQ(inferred[0].kind, RelationshipKind::Inferred);
    EXPECT_GT(inferred[0].confidence, 0.8);
}

TEST(RelationshipInference, MatchesPluralTableName) {
    std::vector<TableInfo> tables = {
        makeTable("CUSTOMERS", {makeColumn("ID", "NUMBER", 22, 1)}, PrimaryKey{"PK_CUSTOMERS", {"ID"}}),
        makeTable("ORDERS", {makeColumn("ID", "NUMBER", 22, 1), makeColumn("CUSTOMER_ID", "NUMBER", 22, 2)},
                   PrimaryKey{"PK_ORDERS", {"ID"}}),
    };

    auto inferred = inferRelationships(tables, {});

    ASSERT_EQ(inferred.size(), 1u);
    EXPECT_EQ(inferred[0].parentTable, "CUSTOMERS");
}

TEST(RelationshipInference, SkipsColumnsAlreadyDeclared) {
    std::vector<TableInfo> tables = {
        makeTable("CUSTOMER", {makeColumn("ID", "NUMBER", 22, 1)}, PrimaryKey{"PK_CUSTOMER", {"ID"}}),
        makeTable("ORDERS", {makeColumn("ID", "NUMBER", 22, 1), makeColumn("CUSTOMER_ID", "NUMBER", 22, 2)},
                   PrimaryKey{"PK_ORDERS", {"ID"}}),
    };

    Relationship declared;
    declared.parentTable = "CUSTOMER";
    declared.parentColumns = {"ID"};
    declared.childTable = "ORDERS";
    declared.childColumns = {"CUSTOMER_ID"};
    declared.kind = RelationshipKind::Declared;
    declared.confidence = 1.0;

    auto inferred = inferRelationships(tables, {declared});

    EXPECT_TRUE(inferred.empty());
}

TEST(RelationshipInference, SkipsWhenTypeFamiliesDiffer) {
    std::vector<TableInfo> tables = {
        makeTable("CUSTOMER", {makeColumn("ID", "VARCHAR2", 32, 1)}, PrimaryKey{"PK_CUSTOMER", {"ID"}}),
        makeTable("ORDERS", {makeColumn("ID", "NUMBER", 22, 1), makeColumn("CUSTOMER_ID", "NUMBER", 22, 2)},
                   PrimaryKey{"PK_ORDERS", {"ID"}}),
    };

    auto inferred = inferRelationships(tables, {});

    EXPECT_TRUE(inferred.empty());
}

TEST(RelationshipInference, NoMatchWhenNoCandidateTable) {
    std::vector<TableInfo> tables = {
        makeTable("ORDERS", {makeColumn("ID", "NUMBER", 22, 1), makeColumn("WAREHOUSE_ID", "NUMBER", 22, 2)},
                   PrimaryKey{"PK_ORDERS", {"ID"}}),
    };

    auto inferred = inferRelationships(tables, {});

    EXPECT_TRUE(inferred.empty());
}

TEST(RelationshipInference, SkipsWhenCandidateHasCompositePrimaryKey) {
    std::vector<TableInfo> tables = {
        makeTable("CUSTOMER", {makeColumn("ID", "NUMBER", 22, 1), makeColumn("REGION", "NUMBER", 22, 2)},
                   PrimaryKey{"PK_CUSTOMER", {"ID", "REGION"}}),
        makeTable("ORDERS", {makeColumn("ID", "NUMBER", 22, 1), makeColumn("CUSTOMER_ID", "NUMBER", 22, 2)},
                   PrimaryKey{"PK_ORDERS", {"ID"}}),
    };

    auto inferred = inferRelationships(tables, {});

    EXPECT_TRUE(inferred.empty());
}
