#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>

#include "checkpoint/CheckpointStore.hpp"

using namespace dbscanner::checkpoint;

namespace {
std::filesystem::path makeTempDir() {
    std::string tmpl = (std::filesystem::temp_directory_path() / "dbscanner_ckpt_test_XXXXXX").string();
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    char* result = mkdtemp(buf.data());
    return std::filesystem::path(result);
}
}  // namespace

class CheckpointStoreTest : public ::testing::Test {
protected:
    void SetUp() override { tempDir_ = makeTempDir(); }
    void TearDown() override { std::filesystem::remove_all(tempDir_); }

    std::string dbPath() const { return (tempDir_ / "checkpoint.db").string(); }

    std::filesystem::path tempDir_;
};

TEST_F(CheckpointStoreTest, FreshStoreHasNoRunInfoAndPendingStatuses) {
    CheckpointStore store(dbPath());
    EXPECT_TRUE(store.isFreshRun());
    EXPECT_EQ(store.getTableStatus("CUSTOMERS"), UnitStatus::Pending);
}

TEST_F(CheckpointStoreTest, SetRunInfoMakesItNoLongerFresh) {
    CheckpointStore store(dbPath());
    store.setRunInfo({{"schemaOwner", "APP"}});
    EXPECT_FALSE(store.isFreshRun());
    EXPECT_EQ(store.getRunInfo().at("schemaOwner"), "APP");
}

TEST_F(CheckpointStoreTest, MarkTableDoneStoresPayload) {
    CheckpointStore store(dbPath());
    store.markTableStatus("CUSTOMERS", UnitStatus::InProgress);
    EXPECT_EQ(store.getTableStatus("CUSTOMERS"), UnitStatus::InProgress);

    store.markTableDone("CUSTOMERS", R"({"name":"CUSTOMERS"})");
    EXPECT_EQ(store.getTableStatus("CUSTOMERS"), UnitStatus::Done);
    ASSERT_TRUE(store.getTablePayload("CUSTOMERS").has_value());
    EXPECT_EQ(*store.getTablePayload("CUSTOMERS"), R"({"name":"CUSTOMERS"})");
}

TEST_F(CheckpointStoreTest, ErrorStatusRecordsMessageAndIsRetried) {
    CheckpointStore store(dbPath());
    store.markTableStatus("ORDERS", UnitStatus::Error, "ORA-03113: end-of-file on communication channel");
    EXPECT_EQ(store.getTableStatus("ORDERS"), UnitStatus::Error);
}

TEST_F(CheckpointStoreTest, ResetInProgressToPendingRequeuesCrashedUnits) {
    CheckpointStore store(dbPath());
    store.markTableStatus("CUSTOMERS", UnitStatus::InProgress);
    store.markTableDone("ORDERS", "{}");
    store.markRelationStatus("ORDERS(CUSTOMER_ID) -> CUSTOMERS(ID)", UnitStatus::InProgress);

    store.resetInProgressToPending();

    EXPECT_EQ(store.getTableStatus("CUSTOMERS"), UnitStatus::Pending);
    EXPECT_EQ(store.getTableStatus("ORDERS"), UnitStatus::Done);  // untouched
    EXPECT_EQ(store.getRelationStatus("ORDERS(CUSTOMER_ID) -> CUSTOMERS(ID)"), UnitStatus::Pending);
}

TEST_F(CheckpointStoreTest, ReopeningExistingDbFilePreservesState) {
    {
        CheckpointStore store(dbPath());
        store.setRunInfo({{"schemaOwner", "APP"}});
        store.markTableDone("CUSTOMERS", R"({"name":"CUSTOMERS"})");
    }

    CheckpointStore reopened(dbPath());
    EXPECT_FALSE(reopened.isFreshRun());
    EXPECT_EQ(reopened.getTableStatus("CUSTOMERS"), UnitStatus::Done);
    ASSERT_TRUE(reopened.getTablePayload("CUSTOMERS").has_value());
}

TEST_F(CheckpointStoreTest, TableNamesWithStatusFiltersCorrectly) {
    CheckpointStore store(dbPath());
    store.markTableDone("A", "{}");
    store.markTableStatus("B", UnitStatus::Pending);
    store.markTableStatus("C", UnitStatus::Error, "boom");

    const auto done = store.tableNamesWithStatus(UnitStatus::Done);
    ASSERT_EQ(done.size(), 1u);
    EXPECT_EQ(done[0], "A");

    const auto errored = store.tableNamesWithStatus(UnitStatus::Error);
    ASSERT_EQ(errored.size(), 1u);
    EXPECT_EQ(errored[0], "C");
}
