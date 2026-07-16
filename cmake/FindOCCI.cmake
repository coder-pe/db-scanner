#[=======================================================================[
FindOCCI
--------

Locates the Oracle OCCI (C++ Call Interface) headers and libraries that
ship with the Oracle Instant Client "Basic" + "SDK" packages.

Looks under, in order:
  - ${ORACLE_HOME} / $ENV{ORACLE_HOME}
  - ${ORACLE_INSTANT_CLIENT_DIR} / $ENV{ORACLE_INSTANT_CLIENT_DIR}

Expects to find:
  - occi.h, oci.h under <root>/sdk/include or <root>/include
  - libocci.so and libclntsh.so directly under <root> (as shipped by the
    Basic package) or under <root>/sdk/lib

Defines:
  OCCI_FOUND
  OCCI_INCLUDE_DIRS
  OCCI_LIBRARIES
  imported target Oracle::OCCI
#]=======================================================================]

set(_dbscanner_oracle_roots
    "${ORACLE_HOME}"
    "$ENV{ORACLE_HOME}"
    "${ORACLE_INSTANT_CLIENT_DIR}"
    "$ENV{ORACLE_INSTANT_CLIENT_DIR}"
)

find_path(OCCI_INCLUDE_DIR
    NAMES occi.h
    PATHS ${_dbscanner_oracle_roots}
    PATH_SUFFIXES sdk/include include
    NO_DEFAULT_PATH
)

find_library(OCCI_LIBRARY
    NAMES occi
    PATHS ${_dbscanner_oracle_roots}
    PATH_SUFFIXES sdk/lib lib .
    NO_DEFAULT_PATH
)

find_library(OCCI_CLNTSH_LIBRARY
    NAMES clntsh
    PATHS ${_dbscanner_oracle_roots}
    PATH_SUFFIXES sdk/lib lib .
    NO_DEFAULT_PATH
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OCCI
    REQUIRED_VARS OCCI_LIBRARY OCCI_CLNTSH_LIBRARY OCCI_INCLUDE_DIR
    FAIL_MESSAGE "Could not find Oracle OCCI headers/libraries (occi.h, libocci.so, libclntsh.so). Set ORACLE_HOME to an Instant Client install that includes the SDK package."
)

if(OCCI_FOUND)
    set(OCCI_INCLUDE_DIRS "${OCCI_INCLUDE_DIR}")
    set(OCCI_LIBRARIES "${OCCI_LIBRARY}" "${OCCI_CLNTSH_LIBRARY}")

    if(NOT TARGET Oracle::OCCI)
        add_library(Oracle::OCCI UNKNOWN IMPORTED)
        set_target_properties(Oracle::OCCI PROPERTIES
            IMPORTED_LOCATION "${OCCI_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OCCI_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES "${OCCI_CLNTSH_LIBRARY}"
        )
    endif()
endif()

mark_as_advanced(OCCI_INCLUDE_DIR OCCI_LIBRARY OCCI_CLNTSH_LIBRARY)
