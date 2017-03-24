cmake_minimum_required(VERSION 3.5)
project(lo2s)

SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

include(cmake/DefaultBuildType.cmake)

# Intialize git submodules if not done already
file(GLOB SUBMODULE_FILES "lib/otf2xx/*")
list(LENGTH SUBMODULE_FILES COUNT_OTF2XX)
file(GLOB SUBMODULE_FILES "lib/nitro/*")
list(LENGTH SUBMODULE_FILES COUNT_NITRO)
if(${COUNT_OTF2XX} EQUAL 0 OR ${COUNT_NITRO} EQUAL 0)
    message(STATUS "Initializing git submodule")
    execute_process(COMMAND "git" "submodule" "init" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    execute_process(COMMAND "git" "submodule" "update" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

include(lib/otf2xx/OTF2XX.cmake)

include(lib/nitro/Nitro.cmake)

include(lib/x86_adapt/x86_adapt.cmake)

if (X86Adapt_FOUND)
    set(X86_ADAPT_SOURCE_FILES src/metric/x86_adapt/metrics.cpp src/metric/x86_adapt/recorder.cpp)
else()
    set(X86_ADAPT_SOURCE_FILES "")
endif()

find_package(Boost REQUIRED COMPONENTS system program_options filesystem)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

find_package(Radare REQUIRED)
include_directories(SYSTEM ${Radare_INCLUDE_DIRS})

# TODO this seems broken enough
#find_package(BFD REQUIRED)
#include_directories(SYSTEM ${BFD_INCLUDE_DIRS})
set(BFD_LIBRARIES "-lbfd -lz -ldl -liberty")


INCLUDE(CheckIncludeFiles)
CHECK_INCLUDE_FILES(linux/hw_breakpoint.h HAVE_HW_BREAKPOINT_H)

option(HW_BREAKPOINT_COMPAT "Time synchronization fallback for old kernels without hardware breakpoint support." OFF)

if (HW_BREAKPOINT_COMPAT)
    add_definitions(-DHW_BREAKPOINT_COMPAT)
    if (HAVE_HW_BREAKPOINT_H)
        message(WARNING "Using inaccurate compatibility time synchronization while hardware breakpoints are available. Consider deactivating HW_BREAKPOINT_COMPAT")
    endif()
else()
    if (NOT HAVE_HW_BREAKPOINT_H)
        message(SEND_ERROR "Missing linux/hw_breakpoint.h, install that or consider activating HW_BREAKPOINT_COMPAT for old kernels with compatibility time synchronization.")
    endif()
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -pedantic -Wextra -pthread")

include_directories(include)

set(SOURCE_FILES
        src/metric/plugin/plugin.cpp src/metric/plugin/channel.cpp src/metric/plugin/metrics.cpp
        ${X86_ADAPT_SOURCE_FILES}
        src/metric/perf_counter.cpp

        src/monitor/thread_monitor.cpp src/monitor/active_monitor.cpp

        src/perf/sample/writer.cpp
        src/perf/time/converter.cpp src/perf/time/reader.cpp
        src/perf/tracepoint/format.cpp src/perf/tracepoint/recorder.cpp
        src/perf/tracepoint/writer.cpp

        src/trace/counters.cpp src/trace/trace.cpp

        src/main.cpp src/monitor.cpp src/thread_map.cpp
        src/platform.cpp
        src/topology.cpp src/radare.cpp src/bfd_resolve.cpp src/pipe.cpp
        src/mmap.cpp
    )


add_executable(lo2s ${SOURCE_FILES})
target_link_libraries(lo2s ${OTF2_LIBRARIES} ${Boost_LIBRARIES} ${Radare_LIBRARIES}
        ${BFD_LIBRARIES} ${X86_ADAPT_LIBRARIES})

option(IWYU "Developer option for include what you use." OFF)

if (IWYU)
    find_program(iwyu_path NAMES include-what-you-use iwyu)
    if(NOT iwyu_path)
        message(FATAL_ERROR "Could not find the program include-what-you-use")
    endif()
    set_property(TARGET lo2s PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path})
endif()


# Convince clion to accept headers as part of the project by adding them to a dummy target
# Should not interfer with normal opratation.
FILE(GLOB_RECURSE clion_dummy_headers *.hpp *.h)
FILE(GLOB_RECURSE clion_dummy_source main.cpp)
add_executable(clion_dummy_executable EXCLUDE_FROM_ALL ${clion_dummy_source} ${clion_dummy_headers})

install(TARGETS lo2s RUNTIME DESTINATION bin)
