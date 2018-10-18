include(CheckCSourceCompiles)

function(CHECK_STRUCT_HAS_BITFIELD _STRUCT _MEMBER _HEADER _RESULT)
    set(_INCLUDE_FILES)
    foreach(it ${_HEADER})
        string(APPEND _INCLUDE_FILES "#include <${it}>\n")
    endforeach()

    set(_CHECK_STRUCT_MEMBER_SOURCE_CODE "
    ${_INCLUDE_FILES}
    int main()
    {
        ${_STRUCT} test;
        test.${_MEMBER} = 0;
        return 0;
    }
    ")

    CHECK_C_SOURCE_COMPILES("${_CHECK_STRUCT_MEMBER_SOURCE_CODE}" ${_RESULT})
endfunction()
