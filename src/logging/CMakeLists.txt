add_library(dbscanner_logging STATIC
    Logger.cpp
)

target_include_directories(dbscanner_logging PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(dbscanner_logging PUBLIC
    spdlog::spdlog
)
