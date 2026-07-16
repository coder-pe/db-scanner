add_library(dbscanner_relations STATIC
    RelationshipInference.cpp
    DependencyGraph.cpp
)

target_include_directories(dbscanner_relations PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(dbscanner_relations PUBLIC
    dbscanner_core
)
