cmake_minimum_required(VERSION 3.2)
project(lo2s)

include(CMakeDependentOption)

SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

include(cmake/DefaultBuildType.cmake)
include(cmake/UnsetIfUpdated.cmake)

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

set(lo2s_USE_STATIC_LIBS "MOSTLY" CACHE STRING "Link lo2s statically.")
set_property(CACHE lo2s_USE_STATIC_LIBS PROPERTY STRINGS "MOSTLY" "OFF" "ALL")

UnsetIfUpdated(Dl_USE_STATIC_LIBS lo2s_USE_STATIC_LIBS)
UnsetIfUpdated(Binutils_USE_STATIC_LIBS lo2s_USE_STATIC_LIBS)
UnsetIfUpdated(Boost_USE_STATIC_LIBS lo2s_USE_STATIC_LIBS)
UnsetIfUpdated(Boost_USE_STATIC_RUNTIME lo2s_USE_STATIC_LIBS)
UnsetIfUpdated(OTF2_USE_STATIC_LIBS lo2s_USE_STATIC_LIBS)
UnsetIfUpdated(Radare_USE_STATIC_LIBS lo2s_USE_STATIC_LIBS)
UnsetIfUpdated(X86Adapt_STATIC lo2s_USE_STATIC_LIBS)

if(lo2s_USE_STATIC_LIBS STREQUAL "OFF")
    set(Dl_USE_STATIC_LIBS       OFF CACHE BOOL "")
    set(Binutils_USE_STATIC_LIBS OFF CACHE BOOL "")
    set(Boost_USE_STATIC_LIBS    OFF CACHE BOOL "")
    set(Boost_USE_STATIC_RUNTIME OFF CACHE BOOL "")
    set(OTF2_USE_STATIC_LIBS     OFF CACHE BOOL "")
    set(Radare_USE_STATIC_LIBS   OFF CACHE BOOL "")
    set(X86Adapt_STATIC          OFF CACHE BOOL "")
endif()

if(lo2s_USE_STATIC_LIBS STREQUAL "MOSTLY")
    set(Dl_USE_STATIC_LIBS       OFF CACHE BOOL "")
    set(Binutils_USE_STATIC_LIBS ON CACHE BOOL "")
    set(Boost_USE_STATIC_LIBS    ON CACHE BOOL "")
    set(Boost_USE_STATIC_RUNTIME ON CACHE BOOL "")
    set(OTF2_USE_STATIC_LIBS     ON CACHE BOOL "")
    set(Radare_USE_STATIC_LIBS   ON CACHE BOOL "")
    set(X86Adapt_STATIC          ON CACHE BOOL "")
endif()

if(lo2s_USE_STATIC_LIBS STREQUAL "ALL")
    set(Dl_USE_STATIC_LIBS       ON CACHE BOOL "")
    set(Binutils_USE_STATIC_LIBS ON CACHE BOOL "")
    set(Boost_USE_STATIC_LIBS    ON CACHE BOOL "")
    set(Boost_USE_STATIC_RUNTIME ON CACHE BOOL "")
    set(OTF2_USE_STATIC_LIBS     ON CACHE BOOL "")
    set(Radare_USE_STATIC_LIBS   ON CACHE BOOL "")
    set(X86Adapt_STATIC          ON CACHE BOOL "")

    # Doesn't seem to work with clang, even though it should,
    # but at least it doesn't complain about it either
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc -static")
    set(CMAKE_LINK_SEARCH_START_STATIC 1)
    set(CMAKE_LINK_SEARCH_END_STATIC 1)
endif()

include(lib/otf2xx/OTF2XX.cmake)

include(lib/nitro/Nitro.cmake)

include(lib/x86_adapt/x86_adapt.cmake)

if (X86Adapt_FOUND)
    set(X86_ADAPT_SOURCE_FILES src/metric/x86_adapt/metrics.cpp src/metric/x86_adapt/monitor.cpp)
else()
    set(X86_ADAPT_SOURCE_FILES "")
endif()

find_package(Boost REQUIRED COMPONENTS system program_options filesystem)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

find_package(Binutils REQUIRED)
include_directories(SYSTEM ${Binutils_INCLUDE_DIRS})

# TODO Shouldn't this be part of libc and handlded more elegantly?
find_package(Dl REQUIRED)
include_directories(SYSTEM ${Dl_INCLUDE_DIRS})

find_package(Radare)

CMAKE_DEPENDENT_OPTION(USE_RADARE "Enable Radare support." ON "Radare_FOUND" OFF)

if (USE_RADARE)
    if (Radare_FOUND)
        include_directories(SYSTEM ${Radare_INCLUDE_DIRS})
        set(RADARE_SOURCE_FILES src/radare.cpp)
        add_definitions(-DHAVE_RADARE)
    else()
        message(SEND_ERROR "Radare not found but requested.")
        set(RADARE_SOURCE_FILES "")
    endif()
else()
    set(RADARE_SOURCE_FILES "")
endif()

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

INCLUDE(CheckStructHasMember)
CHECK_STRUCT_HAS_MEMBER("struct perf_event_attr" clockid linux/perf_event.h
    HAVE_PERF_EVENT_ATTR_CLOCKID)

option(USE_PERF_CLOCKID "Enables specifying a custom reference clock for recorded events" ON)

if (USE_PERF_CLOCKID)
    if (HAVE_PERF_EVENT_ATTR_CLOCKID)
        add_definitions(-DUSE_PERF_CLOCKID)
    else()
        message(SEND_ERROR "The system is not able to use custom reference clocks for perf events. Consider deactivating USE_PERF_CLOCKID.")
    endif()
else()
    if (HAVE_PERF_EVENT_ATTR_CLOCKID)
        message(WARNING "The system claims to support custom reference clocks for perf events. Consider activating USE_PERF_CLOCKID to enable the options -k,--clockid.")
    endif()
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GNU_SOURCE")

include_directories(include)

set(SOURCE_FILES
        src/metric/plugin/plugin.cpp src/metric/plugin/channel.cpp src/metric/plugin/metrics.cpp
        src/metric/perf_counter.cpp

        src/monitor/cpu_set_monitor.cpp
        src/monitor/cpu_switch_monitor.cpp
        src/monitor/fd_monitor.cpp
        src/monitor/interval_monitor.cpp
        src/monitor/main_monitor.cpp
        src/monitor/process_monitor.cpp
        src/monitor/process_monitor_main.cpp
        src/monitor/thread_monitor.cpp
        src/monitor/threaded_monitor.cpp

        src/perf/event_collection.cpp
        src/perf/event_provider.cpp
        src/perf/counter/writer.cpp
        src/perf/sample/writer.cpp
        src/perf/time/converter.cpp src/perf/time/reader.cpp
        src/perf/tracepoint/format.cpp src/perf/tracepoint/metric_monitor.cpp
        src/perf/tracepoint/writer.cpp src/perf/tracepoint/switch_writer.cpp
        src/perf/tracepoint/exit_reader.cpp

        src/time/time.cpp

        src/trace/counters.cpp src/trace/trace.cpp

        src/config.cpp src/main.cpp src/monitor/process_monitor.cpp src/thread_map.cpp
        src/platform.cpp
        src/topology.cpp src/bfd_resolve.cpp src/pipe.cpp
        src/mmap.cpp
        src/util.cpp

        ${X86_ADAPT_SOURCE_FILES}
        ${RADARE_SOURCE_FILES}
)


add_executable(lo2s ${SOURCE_FILES})
target_link_libraries(lo2s ${OTF2_LIBRARIES} ${Boost_LIBRARIES}
        ${Binutils_LIBRARIES} ${Dl_LIBRARIES} ${X86_ADAPT_LIBRARIES})

if(USE_RADARE)
    target_link_libraries(lo2s ${Radare_LIBRARIES})
endif()

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
