add_test(NAME PerfEventParanoid2 COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fixtures/paranoid.sh 2)
add_test(NAME PerfEventParanoid1 COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fixtures/paranoid.sh 1)
add_test(NAME PerfEventParanoid0 COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fixtures/paranoid.sh 0)
add_test(NAME PerfEventParanoidMinus1 COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fixtures/paranoid.sh -1)

add_test(NAME SysKernelDebug COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fixtures/test_sys_kernel_debug.sh)


set_tests_properties(PerfEventParanoid2 PROPERTIES FIXTURES_SETUP PARANOID2)
set_tests_properties(PerfEventParanoid1 PROPERTIES FIXTURES_SETUP PARANOID1)
set_tests_properties(PerfEventParanoid0 PROPERTIES FIXTURES_SETUP PARANOID0)
set_tests_properties(PerfEventParanoidMinus1 PROPERTIES FIXTURES_SETUP PARANOIDMINUS1)
set_tests_properties(SysKernelDebug PROPERTIES FIXTURES_SETUP DEBUGFS)

# Process Mode tests, those require perf_event_paranoid=2
add_test(NAME process_sampling COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/process_sampling/process_sampling.sh)
set_tests_properties(process_sampling PROPERTIES FIXTURES_REQUIRED PARANOID2)

add_test(NAME process_counters COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/process_counters/process_counters.sh)
set_tests_properties(process_counters PROPERTIES FIXTURES_REQUIRED PARANOID2)

# System Mode tests, those require perf_event_paranoid=0
add_test(NAME system_sampling COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/system_sampling/system_sampling.sh)
set_tests_properties(system_sampling PROPERTIES FIXTURES_REQUIRED PARANOID0)

add_test(NAME system_counters COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/system_counters/system_counters.sh)
set_tests_properties(system_counters PROPERTIES FIXTURES_REQUIRED PARANOID0)

# Tracepoint based tests, those require perf_event_paranoid=-1 and
# /sys/kernel/debug/tracing availability
add_test(NAME tracepoint_recording COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/tracepoint_recording/tracepoint_recording.sh)
set_tests_properties(tracepoint_recording PROPERTIES FIXTURES_REQUIRED "PARANOIDMINUS1;DEBUGFS")

add_test(NAME block_io COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/block_io/block_io.sh)
set_tests_properties(block_io PROPERTIES FIXTURES_REQUIRED "PARANOIDMINUS1;DEBUGFS")

if(USE_LIBAUDIT)
    add_test(NAME syscall_recording COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/syscall_recording/syscall_recording.sh)
    set_tests_properties(syscall_recording PROPERTIES FIXTURES_REQUIRED "PARANOIDMINUS1;DEBUGFS")
endif()

# Tests that do not require perf
if(USE_SENSORS)
    add_test(NAME sensors_recording COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/sensors_recording/sensors_recording.sh)
endif()

if(USE_LIBPFM)
    add_test(NAME pfm_counters COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/pfm_counters/pfm_counters.sh)
    set_tests_properties(pfm_counters PROPERTIES FIXTURES_REQUIRED PARANOID0)
endif()
