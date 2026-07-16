add_library(dbscanner_checkpoint STATIC
    CheckpointStore.cpp
)

target_include_directories(dbscanner_checkpoint PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(dbscanner_checkpoint PUBLIC
    SQLite::SQLite3
    dbscanner_core
)
