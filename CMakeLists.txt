cmake_minimum_required(VERSION 3.5)
project(lo2s)

include(CMakeDependentOption)
include(CheckIncludeFiles)
include(CheckStructHasMember)

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

# Set of variables to enforce static or dynamic linking
set(lo2s_USE_STATIC_LIBS "MOSTLY" CACHE STRING "Link lo2s statically.")
set_property(CACHE lo2s_USE_STATIC_LIBS PROPERTY STRINGS "MOSTLY" "OFF" "ALL")

IfUpdatedUnsetAll(lo2s_USE_STATIC_LIBS
    Dl_USE_STATIC_LIBS
    Binutils_USE_STATIC_LIBS
    Boost_USE_STATIC_LIBS
    Boost_USE_STATIC_RUNTIME
    OTF2_USE_STATIC_LIBS
    OTF2XX_USE_STATIC_LIBS
    Radare_USE_STATIC_LIBS
    X86Adapt_STATIC
)

if(lo2s_USE_STATIC_LIBS STREQUAL "OFF")
    set(Dl_USE_STATIC_LIBS       OFF CACHE BOOL "")
    set(Binutils_USE_STATIC_LIBS OFF CACHE BOOL "")
    set(Boost_USE_STATIC_LIBS    OFF CACHE BOOL "")
    set(Boost_USE_STATIC_RUNTIME OFF CACHE BOOL "")
    set(OTF2_USE_STATIC_LIBS     OFF CACHE BOOL "")
    set(OTF2XX_USE_STATIC_LIBS   OFF CACHE BOOL "")
    set(Radare_USE_STATIC_LIBS   OFF CACHE BOOL "")
    set(X86Adapt_STATIC          OFF CACHE BOOL "")
endif()

if(lo2s_USE_STATIC_LIBS STREQUAL "MOSTLY")
    set(Dl_USE_STATIC_LIBS       OFF CACHE BOOL "")
    set(Binutils_USE_STATIC_LIBS ON CACHE BOOL "")
    set(Boost_USE_STATIC_LIBS    ON CACHE BOOL "")
    set(Boost_USE_STATIC_RUNTIME ON CACHE BOOL "")
    set(OTF2_USE_STATIC_LIBS     ON CACHE BOOL "")
    set(OTF2XX_USE_STATIC_LIBS   ON CACHE BOOL "")
    set(Radare_USE_STATIC_LIBS   ON CACHE BOOL "")
    set(X86Adapt_STATIC          ON CACHE BOOL "")
endif()

if(lo2s_USE_STATIC_LIBS STREQUAL "ALL")
    set(Dl_USE_STATIC_LIBS       ON CACHE BOOL "")
    set(Binutils_USE_STATIC_LIBS ON CACHE BOOL "")
    set(Boost_USE_STATIC_LIBS    ON CACHE BOOL "")
    set(Boost_USE_STATIC_RUNTIME ON CACHE BOOL "")
    set(OTF2_USE_STATIC_LIBS     ON CACHE BOOL "")
    set(OTF2XX_USE_STATIC_LIBS   ON CACHE BOOL "")
    set(Radare_USE_STATIC_LIBS   ON CACHE BOOL "")
    set(X86Adapt_STATIC          ON CACHE BOOL "")

    # Doesn't seem to work with clang, even though it should,
    # but at least it doesn't complain about it either
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc -static")
    set(CMAKE_LINK_SEARCH_START_STATIC 1)
    set(CMAKE_LINK_SEARCH_END_STATIC 1)
endif()


# configure OTF2xx submodule
set(OTF2XX_CHRONO_DURATION_TYPE nanoseconds)
add_subdirectory(lib/otf2xx)
mark_as_advanced(OTF2XX_CHRONO_DURATION_TYPE OTF2XX_WITH_MPI OTF2_CONFIG OTF2_PRINT)

# configure Nitro submodule
add_subdirectory(lib/nitro)

# configure x64_adapt_cxx library
include(lib/x86_adapt/x86_adapt.cmake)

# find external dependencies
find_package(Boost REQUIRED COMPONENTS system program_options filesystem)
find_package(Binutils REQUIRED)
find_package(Radare)
find_package(Threads REQUIRED)

# configurable options
CMAKE_DEPENDENT_OPTION(USE_RADARE "Enable Radare support." ON "Radare_FOUND" OFF)
option(HW_BREAKPOINT_COMPAT "Time synchronization fallback for old kernels without hardware breakpoint support." OFF)
option(USE_PERF_CLOCKID "Enables specifying a custom reference clock for recorded events" ON)
option(IWYU "Developer option for include what you use." OFF)

# system configuration checks
CHECK_INCLUDE_FILES(linux/hw_breakpoint.h HAVE_HW_BREAKPOINT_H)
CHECK_STRUCT_HAS_MEMBER("struct perf_event_attr" clockid linux/perf_event.h HAVE_PERF_EVENT_ATTR_CLOCKID)

# prepare list of source files
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

    src/perf/event_provider.cpp
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
)
if (X86Adapt_FOUND)
    list(APPEND SOURCE_FILES src/metric/x86_adapt/metrics.cpp src/metric/x86_adapt/monitor.cpp)
endif()
if (USE_RADARE AND Radare_FOUND)
    list(APPEND SOURCE_FILES src/radare.cpp)
endif()

# define lo2s target
add_executable(lo2s ${SOURCE_FILES})

# define dependencies of lo2s target
target_link_libraries(lo2s
    PRIVATE
        otf2xx::Writer
        Nitro::Log
        Nitro::Env
        Nitro::Dl
        Threads::Threads
        Boost::system
        Boost::program_options
        Boost::filesystem
        Binutils::Binutils
)

# handle x86_adapt dependency
if(X86Adapt_FOUND)
    target_link_libraries(lo2s PRIVATE x86_adapt::x86_adapt)
endif()

# handle radare dependency
if (USE_RADARE)
    if (Radare_FOUND)
        target_compile_definitions(lo2s PUBLIC HAVE_RADARE)
        target_link_libraries(lo2s PRIVATE Radare::Radare)
    else()
        message(SEND_ERROR "Radare not found but requested.")
    endif()
endif()

# define include directory of target lo2s
target_include_directories(lo2s PRIVATE include)

# special quirk for old kernel versions
if (HW_BREAKPOINT_COMPAT)
    target_compile_definitions(lo2s PRIVATE HW_BREAKPOINT_COMPAT)
    if (HAVE_HW_BREAKPOINT_H)
        message(WARNING "Using inaccurate compatibility time synchronization while hardware breakpoints are available. Consider deactivating HW_BREAKPOINT_COMPAT")
    endif()
else()
    if (NOT HAVE_HW_BREAKPOINT_H)
        message(SEND_ERROR "Missing linux/hw_breakpoint.h, install that or consider activating HW_BREAKPOINT_COMPAT for old kernels with compatibility time synchronization.")
    endif()
endif()

# extra feature for newer kernel version
if (USE_PERF_CLOCKID)
    if (HAVE_PERF_EVENT_ATTR_CLOCKID)
        target_compile_definitions(lo2s PRIVATE USE_PERF_CLOCKID)
    else()
        message(SEND_ERROR "The system is not able to use custom reference clocks for perf events. Consider deactivating USE_PERF_CLOCKID.")
    endif()
else()
    if (HAVE_PERF_EVENT_ATTR_CLOCKID)
        message(WARNING "The system claims to support custom reference clocks for perf events. Consider activating USE_PERF_CLOCKID to enable the options -k,--clockid.")
    endif()
endif()

# define list of C++ features needed for compilation
target_compile_features(lo2s
    PRIVATE
        cxx_auto_type
        cxx_constexpr
        cxx_decltype
        cxx_decltype_auto
        cxx_defaulted_functions
        cxx_defaulted_move_initializers
        cxx_delegating_constructors
        cxx_deleted_functions
        cxx_explicit_conversions
        cxx_generalized_initializers
        cxx_generic_lambdas
        cxx_lambdas
        cxx_lambda_init_captures
        cxx_noexcept
        cxx_nullptr
        cxx_override
        cxx_range_for
        cxx_return_type_deduction
        cxx_rvalue_references
        cxx_trailing_return_types
        cxx_user_literals
        cxx_variadic_templates
        cxx_template_template_parameters
)

# define feature test macro
target_compile_definitions(lo2s PRIVATE _GNU_SOURCE)

# build Debug with all warnings
target_compile_options(lo2s PRIVATE $<$<CONFIG:Debug>:-Wall -pedantic -Wextra>)

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
