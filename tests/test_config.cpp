#include <gtest/gtest.h>

#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "config/CliArgs.hpp"
#include "config/Config.hpp"

using namespace dbscanner::config;

TEST(CliArgs, ParsesRequiredFlags) {
    const char* argv[] = {"dbscanner", "--connect", "host:1521/xe", "--user", "app", "--threads", "8"};
    CliArgs args = parseCliArgs(7, const_cast<char**>(argv));

    ASSERT_TRUE(args.connectString.has_value());
    EXPECT_EQ(*args.connectString, "host:1521/xe");
    ASSERT_TRUE(args.username.has_value());
    EXPECT_EQ(*args.username, "app");
    ASSERT_TRUE(args.threads.has_value());
    EXPECT_EQ(*args.threads, 8);
}

TEST(CliArgs, ParsesExcludeAsCsv) {
    const char* argv[] = {"dbscanner", "--exclude", "TMP_*,*_BACKUP"};
    CliArgs args = parseCliArgs(3, const_cast<char**>(argv));

    ASSERT_EQ(args.excludeTablePatterns.size(), 2u);
    EXPECT_EQ(args.excludeTablePatterns[0], "TMP_*");
    EXPECT_EQ(args.excludeTablePatterns[1], "*_BACKUP");
}

TEST(CliArgs, ThrowsOnUnknownFlag) {
    const char* argv[] = {"dbscanner", "--nope"};
    EXPECT_THROW(parseCliArgs(2, const_cast<char**>(argv)), CliParseError);
}

TEST(CliArgs, ThrowsOnMissingValue) {
    const char* argv[] = {"dbscanner", "--connect"};
    EXPECT_THROW(parseCliArgs(2, const_cast<char**>(argv)), CliParseError);
}

TEST(CliArgs, ThrowsOnNonIntegerThreads) {
    const char* argv[] = {"dbscanner", "--threads", "abc"};
    EXPECT_THROW(parseCliArgs(3, const_cast<char**>(argv)), CliParseError);
}

TEST(Config, RequiresConnectStringAndUsername) {
    CliArgs args;
    EXPECT_THROW(Config::fromCliArgs(args), ConfigError);

    args.connectString = "host:1521/xe";
    EXPECT_THROW(Config::fromCliArgs(args), ConfigError);

    args.username = "app";
    EXPECT_NO_THROW(Config::fromCliArgs(args));
}

TEST(Config, DefaultsSchemaOwnerToUsername) {
    CliArgs args;
    args.connectString = "host:1521/xe";
    args.username = "app_user";

    Config cfg = Config::fromCliArgs(args);
    // Oracle folds unquoted identifiers to uppercase in ALL_TABLES.OWNER, so
    // the default (and any explicit --schema) must be uppercased to match --
    // otherwise data-dictionary queries silently return zero rows.
    EXPECT_EQ(cfg.schemaOwner, "APP_USER");
}

TEST(Config, UppercasesExplicitSchemaOwner) {
    CliArgs args;
    args.connectString = "host:1521/xe";
    args.username = "app_user";
    args.schemaOwner = "some_other_schema";

    Config cfg = Config::fromCliArgs(args);
    EXPECT_EQ(cfg.schemaOwner, "SOME_OTHER_SCHEMA");
}

TEST(Config, CliFlagsOverrideConfigFile) {
    const auto path = std::filesystem::temp_directory_path() / "dbscanner_test_config.json";
    {
        std::ofstream out(path);
        out << R"({"connectString": "file:1521/xe", "username": "file_user", "threads": 2})";
    }

    CliArgs args;
    args.configPath = path.string();
    args.username = "cli_user";  // should win over file's "file_user"

    Config cfg = Config::fromCliArgs(args);
    EXPECT_EQ(cfg.connectString, "file:1521/xe");  // from file, not overridden
    EXPECT_EQ(cfg.username, "cli_user");            // overridden by CLI
    EXPECT_EQ(cfg.threads, 2);

    std::filesystem::remove(path);
}

TEST(Config, RejectsNonPositiveThreads) {
    CliArgs args;
    args.connectString = "host:1521/xe";
    args.username = "app";
    args.threads = 0;

    EXPECT_THROW(Config::fromCliArgs(args), ConfigError);
}

TEST(ResolvePassword, ReadsFromEnvironmentVariable) {
    setenv("DBSCANNER_ORACLE_PWD", "s3cr3t", 1);
    EXPECT_EQ(resolvePassword(), "s3cr3t");
    unsetenv("DBSCANNER_ORACLE_PWD");
}
