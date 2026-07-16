add_library(dbscanner_config STATIC
    CliArgs.cpp
    Config.cpp
)

target_include_directories(dbscanner_config PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(dbscanner_config PUBLIC
    nlohmann_json::nlohmann_json
)
