find_package(GTest REQUIRED)

add_executable(dbscanner_tests
    test_table_metadata_json.cpp
    test_relationship_inference.cpp
    test_dependency_graph.cpp
    test_checkpoint_store.cpp
    test_config.cpp
    test_retry_policy.cpp
    test_scan_engine.cpp
)

target_link_libraries(dbscanner_tests PRIVATE
    dbscanner_core
    dbscanner_config
    dbscanner_checkpoint
    dbscanner_relations
    dbscanner_db_iface
    dbscanner_engine
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(dbscanner_tests)
