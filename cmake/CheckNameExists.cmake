include(CheckCSourceCompiles)

macro(CHECK_NAME_EXISTS _NAME _HEADER _RESULT)
    set(_INCLUDE_FILES)
    foreach(it ${_HEADER})
        string(APPEND _INCLUDE_FILES "#include <${it}>\n")
    endforeach()

    set(_CHECK_NAME_SOURCE_CODE "
    ${_INCLUDE_FILES}
    int main(void)
    {
    #ifdef ${_NAME}
        return 0;
    #else
        (void)(${_NAME});
        return 0;
    #endif
    }
    ")

    CHECK_C_SOURCE_COMPILES("${_CHECK_NAME_SOURCE_CODE}" ${_RESULT})
    unset(_CHECK_NAME_SOURCE_CODE)
    unset(_INCLUDE_FILES)
endmacro()
