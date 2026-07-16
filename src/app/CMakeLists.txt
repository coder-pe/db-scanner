add_executable(dbscanner
    main.cpp
)

target_link_libraries(dbscanner PRIVATE
    dbscanner_core
    dbscanner_config
    dbscanner_logging
    dbscanner_checkpoint
    dbscanner_relations
    dbscanner_report
    dbscanner_db_iface
    dbscanner_db_oracle
    dbscanner_engine
)

install(TARGETS dbscanner RUNTIME DESTINATION bin)
