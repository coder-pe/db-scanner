add_library(dbscanner_report STATIC
    JsonReportWriter.cpp
    ConsoleReporter.cpp
)

target_include_directories(dbscanner_report PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(dbscanner_report PUBLIC
    dbscanner_core
    nlohmann_json::nlohmann_json
    fmt::fmt
)
