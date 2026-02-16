
macro(AddLo2sTest test_name)
    add_test(NAME ${test_name} COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/${test_name}/${test_name}.sh")
    # For the CI, not every test is runnable.
    # This includes everything that requires hardware PMU events.
    # This undocumented-by-design switch allows the CI to skip tests
    # where it is detected that they can not possibly run.
    if(LO2S_TESTS_MAY_SKIP)
        set_tests_properties(${test_name} PROPERTIES SKIP_RETURN_CODE 127)
    endif()
endmacro()

AddLo2sTest(process_sampling)
AddLo2sTest(process_counters)
AddLo2sTest(predefined_counters)
AddLo2sTest(userspace_counters)

AddLo2sTest(system_sampling)
AddLo2sTest(system_counters)

AddLo2sTest(block_io)
AddLo2sTest(tracepoint_recording)

if(USE_LIBAUDIT)
    AddLo2sTest(syscall_recording)
endif()


if(USE_SENSORS)
    AddLo2sTest(sensors_recording)
endif()

if(USE_LIBPFM)
    AddLo2sTest(pfm_counters)
endif()


if(USE_BPF)
    AddLo2sTest(posix_io)
endif()
