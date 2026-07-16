#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <functional>
#include <map>

#include <nlohmann/json.hpp>

#include "checkpoint/CheckpointStore.hpp"
#include "core/TableMetadata.hpp"
#include "db/IConsistencyChecker.hpp"
#include "db/ISchemaSource.hpp"
#include "engine/ScanEngine.hpp"
#include "engine/ShutdownController.hpp"

using namespace dbscanner;

namespace {

std::filesystem::path makeTempDir() {
    std::string tmpl = (std::filesystem::temp_directory_path() / "dbscanner_engine_test_XXXXXX").string();
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    char* result = mkdtemp(buf.data());
    return std::filesystem::path(result);
}

core::TableInfo makeTable(const std::string& name) {
    core::TableInfo t;
    t.owner = "APP";
    t.name = name;
    core::Column id;
    id.name = "ID";
    id.dataType = "NUMBER";
    id.dataLength = 22;
    id.nullable = false;
    id.columnId = 1;
    t.columns = {id};
    t.primaryKey = core::PrimaryKey{"PK_" + name, {"ID"}};
    return t;
}

class FakeSchemaSource : public db::ISchemaSource {
public:
    std::vector<std::string> tableNames;
    std::map<std::string, core::TableInfo> tables;
    std::vector<core::Relationship> declaredFks;
    std::function<void(const std::string&)> onReadTable;
    std::atomic<int> readTableCallCount{0};

    std::vector<std::string> listTableNames() override { return tableNames; }

    core::TableInfo readTable(const std::string& tableName) override {
        ++readTableCallCount;
        core::TableInfo info = tables.at(tableName);
        if (onReadTable) onReadTable(tableName);
        return info;
    }

    std::vector<core::Relationship> listDeclaredForeignKeys() override { return declaredFks; }
};

class FakeConsistencyChecker : public db::IConsistencyChecker {
public:
    std::atomic<int> checkCallCount{0};
    int64_t orphanCountToReturn = 0;

    core::ConsistencyFinding checkOrphans(const core::Relationship& relationship, int /*sampleLimit*/) override {
        ++checkCallCount;
        core::ConsistencyFinding finding;
        finding.relationship = relationship;
        finding.orphanCount = orphanCountToReturn;
        return finding;
    }
};

class ScanEngineTest : public ::testing::Test {
protected:
    void SetUp() override { tempDir_ = makeTempDir(); }
    void TearDown() override { std::filesystem::remove_all(tempDir_); }

    std::string dbPath() const { return (tempDir_ / "checkpoint.db").string(); }

    std::filesystem::path tempDir_;
};

}  // namespace

TEST_F(ScanEngineTest, FullRunDiscoversTablesAndChecksConsistency) {
    FakeSchemaSource schemaSource;
    schemaSource.tableNames = {"CUSTOMERS", "ORDERS"};
    schemaSource.tables["CUSTOMERS"] = makeTable("CUSTOMERS");
    schemaSource.tables["ORDERS"] = makeTable("ORDERS");

    core::Relationship fk;
    fk.parentTable = "CUSTOMERS";
    fk.parentColumns = {"ID"};
    fk.childTable = "ORDERS";
    fk.childColumns = {"ID"};
    fk.kind = core::RelationshipKind::Declared;
    fk.confidence = 1.0;
    fk.constraintName = "FK_ORDERS_CUSTOMERS";
    schemaSource.declaredFks = {fk};

    FakeConsistencyChecker checker;
    checkpoint::CheckpointStore store(dbPath());

    engine::ScanEngineOptions options;
    options.schemaOwner = "APP";
    options.threads = 2;
    options.inferRelationships = false;

    engine::ScanEngine scanEngine(schemaSource, checker, store, options);
    engine::ShutdownController shutdown;

    const core::ScanResult result = scanEngine.run(shutdown);

    EXPECT_EQ(result.status, core::ScanStatus::Completed);
    EXPECT_EQ(result.tables.size(), 2u);
    ASSERT_EQ(result.relationships.size(), 1u);
    EXPECT_EQ(result.relationships[0].kind, core::RelationshipKind::Declared);
    ASSERT_EQ(result.consistencyFindings.size(), 1u);
    EXPECT_EQ(checker.checkCallCount.load(), 1);

    EXPECT_EQ(store.getTableStatus("CUSTOMERS"), checkpoint::UnitStatus::Done);
    EXPECT_EQ(store.getTableStatus("ORDERS"), checkpoint::UnitStatus::Done);
    EXPECT_EQ(store.getRelationStatus(fk.id()), checkpoint::UnitStatus::Done);
}

TEST_F(ScanEngineTest, ResumeSkipsTablesAlreadyMarkedDone) {
    FakeSchemaSource schemaSource;
    schemaSource.tableNames = {"CUSTOMERS", "ORDERS"};
    schemaSource.tables["CUSTOMERS"] = makeTable("CUSTOMERS");
    schemaSource.tables["ORDERS"] = makeTable("ORDERS");

    FakeConsistencyChecker checker;
    checkpoint::CheckpointStore store(dbPath());

    // Pre-seed the checkpoint as if CUSTOMERS was already scanned in a prior run.
    const nlohmann::json cachedPayload = makeTable("CUSTOMERS");
    store.markTableDone("CUSTOMERS", cachedPayload.dump());

    engine::ScanEngineOptions options;
    options.schemaOwner = "APP";
    options.threads = 2;
    options.inferRelationships = false;

    engine::ScanEngine scanEngine(schemaSource, checker, store, options);
    engine::ShutdownController shutdown;

    const core::ScanResult result = scanEngine.run(shutdown);

    EXPECT_EQ(result.status, core::ScanStatus::Completed);
    EXPECT_EQ(result.tables.size(), 2u);
    // Only ORDERS should have triggered a real Oracle read; CUSTOMERS came from cache.
    EXPECT_EQ(schemaSource.readTableCallCount.load(), 1);
}

TEST_F(ScanEngineTest, InterruptionStopsBeforeStartingNewTables) {
    FakeSchemaSource schemaSource;
    schemaSource.tableNames = {"CUSTOMERS", "ORDERS"};
    schemaSource.tables["CUSTOMERS"] = makeTable("CUSTOMERS");
    schemaSource.tables["ORDERS"] = makeTable("ORDERS");

    FakeConsistencyChecker checker;
    checkpoint::CheckpointStore store(dbPath());

    engine::ScanEngineOptions options;
    options.schemaOwner = "APP";
    options.threads = 1;  // deterministic sequential processing
    options.inferRelationships = false;

    engine::ScanEngine scanEngine(schemaSource, checker, store, options);
    engine::ShutdownController shutdown;

    // Simulate Ctrl+C landing right after CUSTOMERS finishes but before ORDERS starts.
    schemaSource.onReadTable = [&](const std::string& tableName) {
        if (tableName == "CUSTOMERS") shutdown.requestStop();
    };

    const core::ScanResult result = scanEngine.run(shutdown);

    EXPECT_EQ(result.status, core::ScanStatus::Interrupted);
    EXPECT_EQ(store.getTableStatus("CUSTOMERS"), checkpoint::UnitStatus::Done);
    EXPECT_EQ(store.getTableStatus("ORDERS"), checkpoint::UnitStatus::Pending);

    // A fresh (non-stopped) shutdown controller resuming from the same store
    // should only need to process ORDERS.
    engine::ShutdownController freshShutdown;
    const core::ScanResult resumed = scanEngine.run(freshShutdown);
    EXPECT_EQ(resumed.status, core::ScanStatus::Completed);
    EXPECT_EQ(resumed.tables.size(), 2u);
}

TEST_F(ScanEngineTest, ExcludedTablesAreSkippedEntirely) {
    FakeSchemaSource schemaSource;
    schemaSource.tableNames = {"CUSTOMERS", "TMP_STAGING"};
    schemaSource.tables["CUSTOMERS"] = makeTable("CUSTOMERS");
    schemaSource.tables["TMP_STAGING"] = makeTable("TMP_STAGING");

    FakeConsistencyChecker checker;
    checkpoint::CheckpointStore store(dbPath());

    engine::ScanEngineOptions options;
    options.schemaOwner = "APP";
    options.threads = 2;
    options.excludeTablePatterns = {"TMP_*"};

    engine::ScanEngine scanEngine(schemaSource, checker, store, options);
    engine::ShutdownController shutdown;

    const core::ScanResult result = scanEngine.run(shutdown);

    ASSERT_EQ(result.tables.size(), 1u);
    EXPECT_EQ(result.tables[0].name, "CUSTOMERS");
}
