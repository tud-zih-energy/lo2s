set(TESTS lo2s_user_sampling
          lo2s_user_counters
          lo2s_tracepoint_test)

foreach(TEST ${TESTS})

    add_test(NAME ${TEST} COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/${TEST}/${TEST}.sh)
    set_tests_properties(${TEST} PROPERTIES SKIP_RETURN_CODE 100)
endforeach()

if(USE_SENSORS)
    add_test(NAME lo2s_sensors_test COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/lo2s_sensors_test/lo2s_sensors_test.sh)
    set_tests_properties(lo2s_sensors_test PROPERTIES SKIP_RETURN_CODE 100)
endif()

if(USE_LIBAUDIT)
    add_test(NAME lo2s_syscall_test COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/lo2s_syscall_test/lo2s_syscall_test.sh)
    set_tests_properties(lo2s_syscall_test PROPERTIES SKIP_RETURN_CODE 100)
endif()
