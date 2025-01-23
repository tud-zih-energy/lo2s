set(TESTS lo2s_user_sampling)

foreach(TEST ${TESTS})

    add_test(NAME ${TEST} COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/${TEST}.sh)
    set_tests_properties(${TEST} PROPERTIES SKIP_RETURN_CODE 100)
endforeach()
