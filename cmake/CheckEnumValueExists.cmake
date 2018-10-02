include(CheckCSourceCompiles)

macro(CHECK_ENUM_VALUE_EXISTS _VALUE _HEADER _RESULT)
    set(_INCLUDE_FILES)
    foreach(it ${_HEADER})
        string(APPEND _INCLUDE_FILES "#include <${it}>\n")
    endforeach()

    set(_CHECK_ENUM_VALUE_SOURCE_CODE "
    ${_INCLUDE_FILES}
    int main()
    {
        int test = ${_VALUE};
        return 0;
    }
    ")

    CHECK_C_SOURCE_COMPILES("${_CHECK_ENUM_VALUE_SOURCE_CODE}" ${_RESULT})
endmacro()
