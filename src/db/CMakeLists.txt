# Header-only interfaces (ISchemaSource, IConsistencyChecker, RetryPolicy).
# Deliberately has zero Oracle/OCCI dependency so ScanEngine and its tests can
# build without the Instant Client SDK.
add_library(dbscanner_db_iface INTERFACE)

target_include_directories(dbscanner_db_iface INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(dbscanner_db_iface INTERFACE
    dbscanner_core
)

if(DBSCANNER_ORACLE_AVAILABLE)
    add_library(dbscanner_db_oracle STATIC
        OracleConnectionPool.cpp
        OracleSchemaReader.cpp
        OracleConsistencyChecker.cpp
    )

    target_include_directories(dbscanner_db_oracle PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )

    target_link_libraries(dbscanner_db_oracle PUBLIC
        dbscanner_db_iface
        Oracle::OCCI
    )
endif()
