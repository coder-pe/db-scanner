add_library(dbscanner_engine STATIC
    ThreadPool.cpp
    ShutdownController.cpp
    ScanEngine.cpp
)

target_include_directories(dbscanner_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(dbscanner_engine PUBLIC
    dbscanner_core
    dbscanner_db_iface
    dbscanner_checkpoint
    dbscanner_relations
    Threads::Threads
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)
