IF(OTF2_CONFIG_PATH)
    FIND_PROGRAM(OTF2_CONFIG NAMES otf2-config
        PATHS
        /opt/otf2/bin
        HINTS
        ${OTF2_CONFIG_PATH}
    )
ELSE(OTF2_CONFIG_PATH)
    FIND_PROGRAM(OTF2_CONFIG NAMES otf2-config
        PATHS
        /opt/otf2/bin
    )
ENDIF(OTF2_CONFIG_PATH)

IF(NOT OTF2_CONFIG OR NOT EXISTS ${OTF2_CONFIG})
    MESSAGE(STATUS "no otf2-config found")
    set(OTF2_FOUND false)
ELSE()
    message(STATUS "OTF2 library found. (using ${OTF2_CONFIG})")

    execute_process(COMMAND ${OTF2_CONFIG} "--interface-version" OUTPUT_VARIABLE OTF2_VERSION)
    STRING(REPLACE ":" "." OTF2_VERSION ${OTF2_VERSION})
    STRING(STRIP ${OTF2_VERSION} OTF2_VERSION)

    if (OTF2_FIND_VERSION)
        if (OTF2_FIND_VERSION_EXACT)
            if (OTF2_FIND_VERSION VERSION_EQUAL OTF2_VERSION)
                set(OTF2_FOUND true)
            else()
                message(SEND_ERROR "Requested excatly interface version ${OTF2_FIND_VERSION}, but found ${OTF2_VERSION}.")
                set(OTF2_FOUND false)
            endif()
        else()
            if(OTF2_VERSION VERSION_GREATER OTF2_FIND_VERSION OR OTF2_VERSION VERSION_EQUAL OTF2_FIND_VERSION)
                set(OTF2_FOUND true)
            else()
                message(SEND_ERROR "Requested minimum interface version ${OTF2_FIND_VERSION}, but found ${OTF2_VERSION}.")
                set(OTF2_FOUND false)
            endif()
        endif()
    else()
        set(OTF2_FOUND true)
    endif()

    execute_process(COMMAND ${OTF2_CONFIG} "--cppflags" OUTPUT_VARIABLE OTF2_INCLUDE_DIRS)
    STRING(REPLACE "-I" "" OTF2_INCLUDE_DIRS ${OTF2_INCLUDE_DIRS})

    execute_process(COMMAND ${OTF2_CONFIG} "--ldflags" OUTPUT_VARIABLE _LINK_LD_ARGS)
    STRING( REPLACE " " ";" _LINK_LD_ARGS ${_LINK_LD_ARGS} )
    FOREACH( _ARG ${_LINK_LD_ARGS} )
        IF(${_ARG} MATCHES "^-L")
            STRING(REGEX REPLACE "^-L" "" _ARG ${_ARG})
            STRING(STRIP "${_ARG}" _ARG)
            SET(OTF2_LINK_DIRS ${OTF2_LINK_DIRS} ${_ARG})
        ENDIF(${_ARG} MATCHES "^-L")
    ENDFOREACH(_ARG)

    execute_process(COMMAND ${OTF2_CONFIG} "--libs" OUTPUT_VARIABLE _LINK_LD_ARGS)
    STRING( REPLACE " " ";" _LINK_LD_ARGS ${_LINK_LD_ARGS} )
    FOREACH( _ARG ${_LINK_LD_ARGS} )
        IF(${_ARG} MATCHES "^-l")
            STRING(REGEX REPLACE "^-l" "" _ARG "${_ARG}")
            STRING(STRIP "${_ARG}" _ARG)
            FIND_LIBRARY(_OTF2_LIB_FROM_ARG NAMES ${_ARG}
                HINTS ${OTF2_LINK_DIRS}
            )
            IF(_OTF2_LIB_FROM_ARG)
                SET(OTF2_LIBRARIES ${OTF2_LIBRARIES} ${_OTF2_LIB_FROM_ARG})
            ENDIF(_OTF2_LIB_FROM_ARG)
            UNSET(_OTF2_LIB_FROM_ARG CACHE)
        ENDIF(${_ARG} MATCHES "^-l")
    ENDFOREACH(_ARG)

    find_program(OTF2_PRINT "otf2-print" PATHS "${OTF2_LINK_DIRS}/.." PATH_SUFFIXES "bin")

ENDIF()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OTF2 DEFAULT_MSG
                                  OTF2_LIBRARIES OTF2_INCLUDE_DIRS OTF2_PRINT)

if(OTF2_FOUND)
    message(STATUS "OTF2 interface version: ${OTF2_VERSION}")
else()
    unset(OTF2_PRINT)
    unset(OTF2_INCLUDE_DIRS)
    unset(OTF2_LINK_DIRS)
    unset(OTF2_LIBRARIES)
endif()
