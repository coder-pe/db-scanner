add_library(dbscanner_core STATIC
    TableMetadata.cpp
    Relationship.cpp
    ScanResult.cpp
    TimeUtil.cpp
)

target_include_directories(dbscanner_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(dbscanner_core PUBLIC
    nlohmann_json::nlohmann_json
)
