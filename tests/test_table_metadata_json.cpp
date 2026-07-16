#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "core/Relationship.hpp"
#include "core/ScanResult.hpp"
#include "core/TableMetadata.hpp"

using namespace dbscanner::core;

TEST(TableMetadataJson, ColumnRoundTrip) {
    Column col;
    col.name = "CUSTOMER_ID";
    col.dataType = "NUMBER";
    col.dataLength = 22;
    col.dataPrecision = 10;
    col.dataScale = 0;
    col.nullable = false;
    col.columnId = 1;
    col.defaultValue = "0";
    col.comment = "primary key";

    nlohmann::json j = col;
    Column roundTripped = j.get<Column>();

    EXPECT_EQ(roundTripped.name, col.name);
    EXPECT_EQ(roundTripped.dataType, col.dataType);
    EXPECT_EQ(roundTripped.dataLength, col.dataLength);
    ASSERT_TRUE(roundTripped.dataPrecision.has_value());
    EXPECT_EQ(*roundTripped.dataPrecision, 10);
    EXPECT_EQ(roundTripped.nullable, false);
    ASSERT_TRUE(roundTripped.defaultValue.has_value());
    EXPECT_EQ(*roundTripped.defaultValue, "0");
}

TEST(TableMetadataJson, ColumnWithoutOptionalsRoundTrips) {
    Column col;
    col.name = "NOTES";
    col.dataType = "VARCHAR2";
    col.dataLength = 4000;
    col.nullable = true;
    col.columnId = 2;

    nlohmann::json j = col;
    Column roundTripped = j.get<Column>();

    EXPECT_FALSE(roundTripped.dataPrecision.has_value());
    EXPECT_FALSE(roundTripped.dataScale.has_value());
    EXPECT_FALSE(roundTripped.defaultValue.has_value());
    EXPECT_FALSE(roundTripped.comment.has_value());
}

TEST(TableMetadataJson, TableInfoFindColumn) {
    TableInfo table;
    table.owner = "APP";
    table.name = "CUSTOMERS";
    table.columns.push_back(Column{"ID", "NUMBER", 22, 10, 0, false, 1, std::nullopt, std::nullopt});
    table.columns.push_back(Column{"NAME", "VARCHAR2", 100, std::nullopt, std::nullopt, true, 2,
                                    std::nullopt, std::nullopt});

    EXPECT_NE(table.findColumn("ID"), nullptr);
    EXPECT_EQ(table.findColumn("ID")->dataType, "NUMBER");
    EXPECT_EQ(table.findColumn("DOES_NOT_EXIST"), nullptr);

    nlohmann::json j = table;
    TableInfo roundTripped = j.get<TableInfo>();
    ASSERT_EQ(roundTripped.columns.size(), 2u);
    EXPECT_EQ(roundTripped.columns[0].name, "ID");
}

TEST(TableMetadataJson, RelationshipIdIsStable) {
    Relationship rel;
    rel.parentTable = "CUSTOMERS";
    rel.parentColumns = {"ID"};
    rel.childTable = "ORDERS";
    rel.childColumns = {"CUSTOMER_ID"};
    rel.kind = RelationshipKind::Declared;
    rel.confidence = 1.0;
    rel.constraintName = "FK_ORDERS_CUSTOMER";

    EXPECT_EQ(rel.id(), "ORDERS(CUSTOMER_ID) -> CUSTOMERS(ID)");

    nlohmann::json j = rel;
    Relationship roundTripped = j.get<Relationship>();
    EXPECT_EQ(roundTripped.kind, RelationshipKind::Declared);
    ASSERT_TRUE(roundTripped.constraintName.has_value());
    EXPECT_EQ(*roundTripped.constraintName, "FK_ORDERS_CUSTOMER");
}

TEST(TableMetadataJson, ScanResultRoundTrip) {
    ScanResult result;
    result.schemaOwner = "APP";
    result.startedAtIso = "2026-01-01T00:00:00Z";
    result.finishedAtIso = "2026-01-01T00:05:00Z";
    result.status = ScanStatus::Completed;

    ConsistencyFinding finding;
    finding.relationship.parentTable = "CUSTOMERS";
    finding.relationship.parentColumns = {"ID"};
    finding.relationship.childTable = "ORDERS";
    finding.relationship.childColumns = {"CUSTOMER_ID"};
    finding.relationship.kind = RelationshipKind::Declared;
    finding.orphanCount = 2;
    finding.sampleKeys = {{"999"}, {"1000"}};
    result.consistencyFindings.push_back(finding);

    result.cycles.push_back(DependencyCycleFinding{{"A", "B", "C"}});

    nlohmann::json j = result;
    ScanResult roundTripped = j.get<ScanResult>();

    EXPECT_EQ(roundTripped.status, ScanStatus::Completed);
    ASSERT_EQ(roundTripped.consistencyFindings.size(), 1u);
    EXPECT_EQ(roundTripped.consistencyFindings[0].orphanCount, 2);
    ASSERT_EQ(roundTripped.cycles.size(), 1u);
    EXPECT_EQ(roundTripped.cycles[0].tablesInCycle.size(), 3u);
}
