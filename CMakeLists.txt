cmake_minimum_required(VERSION 3.20)
project(db-scanner
    VERSION 0.1.0
    DESCRIPTION "Oracle schema/dependency/consistency scanner"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(DBSCANNER_BUILD_TESTS "Build unit tests" ON)
option(DBSCANNER_ENABLE_ORACLE "Build the Oracle/OCCI-dependent app and db targets" ON)

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)

set(DBSCANNER_ORACLE_AVAILABLE FALSE)
if(DBSCANNER_ENABLE_ORACLE)
    find_package(OCCI)
    if(OCCI_FOUND)
        set(DBSCANNER_ORACLE_AVAILABLE TRUE)
    else()
        message(WARNING
            "Oracle Instant Client SDK (occi.h/oci.h) not found under ORACLE_HOME "
            "($ENV{ORACLE_HOME}). The 'dbscanner' app and Oracle-backed db library will "
            "be skipped. Download instantclient-sdk-linux.x64-<version>.zip from "
            "Oracle, extract it into the same directory as your Instant Client Basic "
            "install (so it contains sdk/include/occi.h), and re-run cmake. "
            "Core libraries and their unit tests still build without it.")
    endif()
endif()

add_subdirectory(src)

if(DBSCANNER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
