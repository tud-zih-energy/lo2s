include(cmake/UnsetIfUpdated.cmake)

NitroUnsetIfUpdated(MIN_LOG_LEVEL CMAKE_BUILD_TYPE)

if (NOT MIN_LOG_LEVEL)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(MIN_LOG_LEVEL "trace" CACHE STRING
            "The minimum required severity level of log messages to be compiled into the binary.")
    else()
        set(MIN_LOG_LEVEL "debug" CACHE STRING
            "The minimum required severity level of log messages to be compiled into the binary.")
    endif()

    set_property(CACHE MIN_LOG_LEVEL PROPERTY "STRINGS" "fatal" "error" "warn" "info" "debug"
        "trace")
endif()

add_definitions(-DNITRO_LOG_MIN_SEVERITY=${MIN_LOG_LEVEL})
