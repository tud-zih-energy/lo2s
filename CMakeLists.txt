cmake_minimum_required(VERSION 3.17)

project(lo2s VERSION 1.7.0)
set(LO2S_COPYRIGHT_YEAR "2023")


include(CMakeDependentOption)
include(CheckIncludeFiles)
include(CheckStructHasMember)
include(CheckFunctionExists)
include(FeatureSummary)

SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

include(cmake/DefaultBuildType.cmake)
include(cmake/UnsetIfUpdated.cmake)
include(cmake/CheckNameExists.cmake)
include(cmake/CheckStructHasBitField.cmake)

# Intialize git submodules if not done already
include(cmake/GitSubmoduleUpdate.cmake)
git_submodule_update()

# Set of variables to enforce static or dynamic linking
set(lo2s_USE_STATIC_LIBS "OFF" CACHE STRING "Link lo2s statically.")
set_property(CACHE lo2s_USE_STATIC_LIBS PROPERTY STRINGS "MOSTLY" "OFF" "ALL")

IfUpdatedUnsetAll(lo2s_USE_STATIC_LIBS
    Dl_USE_STATIC_LIBS
    Binutils_USE_STATIC_LIBS
    OTF2_USE_STATIC_LIBS
    OTF2XX_USE_STATIC_LIBS
    Radare_USE_STATIC_LIBS
    X86Adapt_STATIC
    x86_energy_STATIC
)

if(lo2s_USE_STATIC_LIBS STREQUAL "OFF")
    set(Dl_USE_STATIC_LIBS       OFF CACHE BOOL "")
    set(Binutils_USE_STATIC_LIBS OFF CACHE BOOL "")
    set(OTF2_USE_STATIC_LIBS     OFF CACHE BOOL "")
    set(OTF2XX_USE_STATIC_LIBS   OFF CACHE BOOL "")
    set(Radare_USE_STATIC_LIBS   OFF CACHE BOOL "")
    set(X86Adapt_STATIC          OFF CACHE BOOL "")
    set(x86_energy_STATIC        OFF CACHE BOOL "")
    set(Sensors_USE_STATIC_LIBS  OFF CACHE BOOL "")
    set(NVML_USE_STATIC_LIBS     OFF CACHE BOOL "")
endif()

if(lo2s_USE_STATIC_LIBS STREQUAL "MOSTLY")
    set(Dl_USE_STATIC_LIBS       OFF CACHE BOOL "")
    set(Binutils_USE_STATIC_LIBS ON CACHE BOOL "")
    set(OTF2_USE_STATIC_LIBS     ON CACHE BOOL "")
    set(OTF2XX_USE_STATIC_LIBS   ON CACHE BOOL "")
    set(Radare_USE_STATIC_LIBS   ON CACHE BOOL "")
    set(X86Adapt_STATIC          ON CACHE BOOL "")
    set(x86_energy_STATIC        ON CACHE BOOL "")
    set(Sensors_USE_STATIC_LIBS  ON CACHE BOOL "")
    set(NVML_USE_STATIC_LIBS     ON CACHE BOOL "")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
endif()

if(lo2s_USE_STATIC_LIBS STREQUAL "ALL")
    set(Dl_USE_STATIC_LIBS       ON CACHE BOOL "")
    set(Binutils_USE_STATIC_LIBS ON CACHE BOOL "")
    set(OTF2_USE_STATIC_LIBS     ON CACHE BOOL "")
    set(OTF2XX_USE_STATIC_LIBS   ON CACHE BOOL "")
    set(Radare_USE_STATIC_LIBS   ON CACHE BOOL "")
    set(X86Adapt_STATIC          ON CACHE BOOL "")
    set(x86_energy_STATIC        ON CACHE BOOL "")
    set(Sensors_USE_STATIC_LIBS  ON CACHE BOOL "")
    set(NVML_USE_STATIC_LIBS     ON CACHE BOOL "")

    # Doesn't seem to work with clang, even though it should,
    # but at least it doesn't complain about it either
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc -static")
    set(CMAKE_LINK_SEARCH_START_STATIC 1)
    set(CMAKE_LINK_SEARCH_END_STATIC 1)
endif()

# Check if we are running Linux
if(NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    message(FATAL_ERROR "You can only build and run lo2s on a Linux machine. I'm sorry.")
endif()

# configure OTF2xx submodule
set(OTF2XX_CHRONO_DURATION_TYPE nanoseconds CACHE INTERNAL "")
add_subdirectory(lib/otf2xx)
mark_as_advanced(OTF2XX_WITH_MPI OTF2_CONFIG OTF2_PRINT)

# configure fmtlib submodule
add_subdirectory(lib/fmt)

# configure Nitro submodule
add_subdirectory(lib/nitro)

# configure x64_adapt_cxx library
include(lib/x86_adapt/x86_adapt.cmake)

# find external dependencies
find_package(Git)
find_package(Binutils REQUIRED)
find_package(Radare)
set(THREADS_PREFER_PTHREAD_FLAG true)
find_package(Threads REQUIRED)
find_package(Doxygen COMPONENTS dot)
find_package(x86_energy 2.0 CONFIG)
find_package(StdFilesystem REQUIRED)
find_package(Sensors)
find_package(CUDAToolkit)
find_package(PkgConfig)

if(PkgConfig_FOUND)
    pkg_check_modules(Audit audit)
endif()


# configurable options
CMAKE_DEPENDENT_OPTION(USE_RADARE "Enable Radare support." ON "Radare_FOUND" OFF)
option(USE_HW_BREAKPOINT_COMPAT "Time synchronization fallback for old kernels without hardware breakpoint support." OFF)
add_feature_info("USE_HW_BREAKPOINT_COMPAT" USE_HW_BREAKPOINT_COMPAT "Time synchronization fallback for old kernels without hardware breakpoint support.")
option(IWYU "Developer option for include what you use." OFF)
option(UML_LOOK "Generate graphs with an UML look" OFF)
add_feature_info("USE_RADARE" USE_RADARE "Use Radare to add instruction information to samples.")
CMAKE_DEPENDENT_OPTION(USE_SENSORS "Use the libsensors to read system metrics." ON "Sensors_FOUND" OFF)
add_feature_info("USE_SENSORS" USE_SENSORS "Use the libsensors to read system metrics.")
CMAKE_DEPENDENT_OPTION(USE_NVML "Use the NVIDIA Management Library for GPU sampling and metrics." ON "CUDAToolkit_FOUND" OFF)
add_feature_info("USE_NVML" USE_NVML "Use the NVIDIA Management Library for GPU sampling and metrics.")
CMAKE_DEPENDENT_OPTION(USE_LIBAUDIT "Use libaudit for syscall name resolution" ON Audit_FOUND OFF)
add_feature_info("USE_LIBAUDIT" USE_LIBAUDIT "Use libaudit for syscall name resolution.")

# system configuration checks
CHECK_INCLUDE_FILES(linux/hw_breakpoint.h HAVE_HW_BREAKPOINT_H)
CHECK_STRUCT_HAS_MEMBER("struct perf_event_attr" clockid linux/perf_event.h HAVE_PERF_EVENT_ATTR_CLOCKID)

if(NOT HAVE_PERF_EVENT_ATTR_CLOCKID)
    message(FATAL_ERROR "The system is not able to use custom reference clocks for perf events. Make sure that you are running on system that supports use_clockid with perf")
endif()

check_function_exists(clock_gettime CLOCK_GETTIME_FOUND)
if(NOT CLOCK_GETTIME_FOUND)
    set(CMAKE_REQUIRED_LIBRARIES "rt")
    check_function_exists(clock_gettime CLOCK_GETTIME_FOUND_WITH_RT)
    unset(CMAKE_REQUIRED_LIBRARIES)
endif()

CHECK_STRUCT_HAS_BITFIELD("struct perf_event_attr" context_switch linux/perf_event.h HAVE_PERF_RECORD_SWITCH)

if(NOT HAVE_PERF_RECORD_SWITCH)
    message(FATAL_ERROR "lo2s requires support for perf context switch recording. Make sure that you are running on a kernel that support context_switch with perf_event_open")
endif()

# detect version of running kernel
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" LINUX_VERSION ${CMAKE_SYSTEM_VERSION})

# prepare list of source files
set(SOURCE_FILES
    src/types.cpp

    src/metric/plugin/plugin.cpp src/metric/plugin/channel.cpp src/metric/plugin/metrics.cpp

    src/monitor/cpu_set_monitor.cpp
    src/monitor/poll_monitor.cpp
    src/monitor/main_monitor.cpp
    src/monitor/process_monitor.cpp
    src/monitor/system_process_monitor.cpp
    src/monitor/process_monitor_main.cpp
    src/monitor/scope_monitor.cpp
    src/monitor/threaded_monitor.cpp
    src/monitor/tracepoint_monitor.cpp
    src/monitor/bio_monitor.cpp
    src/process_controller.cpp

    src/perf/event_provider.cpp

    src/perf/bio/block_device.cpp
    src/perf/multi_reader.cpp
    src/perf/counter/counter_provider.cpp
    src/perf/counter/group/reader.cpp
    src/perf/counter/userspace/reader.cpp

    src/perf/counter/userspace/writer.cpp
    src/perf/counter/group/writer.cpp

    src/perf/sample/writer.cpp
    src/perf/time/converter.cpp src/perf/time/reader.cpp
    src/perf/tracepoint/format.cpp
    src/perf/tracepoint/writer.cpp
    src/perf/syscall/writer.cpp

    src/time/time.cpp

    src/trace/trace.cpp

    src/config.cpp src/main.cpp src/monitor/process_monitor.cpp
    src/platform.cpp
    src/topology.cpp src/bfd_resolve.cpp src/pipe.cpp
    src/mmap.cpp
    src/util.cpp
    src/perf/util.cpp
    src/syscalls.cpp
    src/summary.cpp
)

# define lo2s target
add_executable(lo2s ${SOURCE_FILES})

# define dependencies of lo2s target
target_link_libraries(lo2s
    PRIVATE
        otf2xx::Writer
        Nitro::log
        Nitro::env
        Nitro::dl
        Nitro::options
        Threads::Threads
        Binutils::Binutils
        fmt::fmt
        std::filesystem
)

# old glibc versions require -lrt for clock_gettime()
if(NOT CLOCK_GETTIME_FOUND)
    if(CLOCK_GETTIME_FOUND_WITH_RT)
        target_link_libraries(lo2s PRIVATE rt)
    else()
        message(SEND_ERROR "Could not find the function clock_gettime(), but it is required.")
    endif()
endif()

# handle x86_adapt dependency
if(X86Adapt_FOUND)
    target_sources(lo2s PRIVATE
        src/metric/x86_adapt/metrics.cpp
        src/metric/x86_adapt/monitor.cpp
        src/metric/x86_adapt/node_monitor.cpp
    )
    target_link_libraries(lo2s PRIVATE x86_adapt::x86_adapt)
endif()
add_feature_info("x86_adapt" X86Adapt_FOUND "Provide additional hardware metrics using x86_adapt")

# handle x86_energy dependency
if(x86_energy_FOUND)
    target_compile_definitions(lo2s PRIVATE HAVE_X86_ENERGY)
    target_sources(lo2s PRIVATE
        src/metric/x86_energy/metrics.cpp
        src/metric/x86_energy/monitor.cpp
    )
    if(x86_energy_STATIC)
        target_link_libraries(lo2s PRIVATE x86_energy::x86_energy-static)
    else()
        target_link_libraries(lo2s PRIVATE x86_energy::x86_energy)
    endif()
endif()
add_feature_info("x86_energy" x86_energy_FOUND "Provide additional power measurements using x86_energy")

# handle radare dependency
if (USE_RADARE)
    if (Radare_FOUND)
        target_compile_definitions(lo2s PUBLIC HAVE_RADARE)
        target_link_libraries(lo2s PRIVATE Radare::Radare)
        target_sources(lo2s PRIVATE
            src/radare.cpp
        )
    else()
        message(SEND_ERROR "Radare not found but requested.")
    endif()
endif()

if (USE_SENSORS)
    if (Sensors_FOUND)
        target_compile_definitions(lo2s PUBLIC HAVE_SENSORS)
        target_link_libraries(lo2s PRIVATE Sensors::sensors)
        target_sources(lo2s PRIVATE
            src/metric/sensors/recorder.cpp
        )
    else()
        message(SEND_ERROR "Sensors not found but requested.")
    endif()
endif()

if (USE_NVML)
    if (CUDAToolkit_FOUND)
        target_compile_definitions(lo2s PUBLIC HAVE_NVML)
        target_link_libraries(lo2s PRIVATE CUDA::nvml)
        target_sources(lo2s PRIVATE
            src/metric/nvml/nvml_recorder.cpp
        )
    else()
        message(SEND_ERROR "NVML not found but requested.")
    endif()
endif()

if (USE_LIBAUDIT)
    if (Audit_FOUND)
        target_compile_definitions(lo2s PUBLIC HAVE_LIBAUDIT)
        target_link_libraries(lo2s PRIVATE ${Audit_LIBRARIES})
    else()
        message(SEND_ERROR "Libaudit not found but requested.")
    endif()
endif()

# generate version string used in lo2s
if(Git_FOUND)
    _is_git(${CMAKE_SOURCE_DIR} IN_GIT)
endif()
if(IN_GIT)
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --always --tags --dirty
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE LO2S_VERSION_STRING
    )
    string(STRIP ${LO2S_VERSION_STRING} LO2S_VERSION_STRING)
else()
    set(LO2S_VERSION_STRING ${lo2s_VERSION})
endif()
configure_file(include/lo2s/version.hpp.in include/lo2s/version.hpp @ONLY)


# check for version dependent features and generate a build config

# check if specific perf events are available
CHECK_NAME_EXISTS(PERF_COUNT_SW_ALIGNMENT_FAULTS linux/perf_event.h HAVE_PERF_EVENT_ALIGNMENT_FAULTS)
CHECK_NAME_EXISTS(PERF_COUNT_SW_EMULATION_FAULTS linux/perf_event.h HAVE_PERF_EVENT_EMULATION_FAULTS)
CHECK_NAME_EXISTS(PERF_COUNT_HW_REF_CPU_CYCLES linux/perf_event.h HAVE_PERF_EVENT_REF_CYCLES)
CHECK_NAME_EXISTS(PERF_COUNT_HW_STALLED_CYCLES_FRONTEND linux/perf_event.h HAVE_PERF_EVENT_STALLED_CYCLES_FRONTEND)
CHECK_NAME_EXISTS(PERF_COUNT_HW_STALLED_CYCLES_BACKEND linux/perf_event.h HAVE_PERF_EVENT_STALLED_CYCLES_BACKEND)
CHECK_NAME_EXISTS(PERF_COUNT_HW_CACHE_NODE linux/perf_event.h HAVE_PERF_EVENT_CACHE_NODE)

#check if certain clock sources are available

CHECK_NAME_EXISTS(CLOCK_MONOTONIC_RAW linux/time.h HAVE_CLOCK_MONOTONIC_RAW)
CHECK_NAME_EXISTS(CLOCK_MONOTONIC_COARSE linux/time.h HAVE_CLOCK_MONOTONIC_COARSE)
CHECK_NAME_EXISTS(CLOCK_REALTIME_COARSE linux/time.h HAVE_CLOCK_REALTIME_COARSE)
CHECK_NAME_EXISTS(CLOCK_BOOTTIME linux/time.h HAVE_CLOCK_BOOTTIME)
CHECK_NAME_EXISTS(CLOCK_REALTIME_ALARM linux/time.h HAVE_CLOCK_REALTIME_ALARM)
CHECK_NAME_EXISTS(CLOCK_BOOTTIME_ALARM linux/time.h HAVE_CLOCK_BOOTTIME_ALARM)

#check if PERF_RECORD_LOST_SAMPLES is available
CHECK_NAME_EXISTS(PERF_RECORD_LOST_SAMPLES linux/perf_event.h HAVE_PERF_RECORD_LOST_SAMPLES)

# special quirk for old kernel versions
if (USE_HW_BREAKPOINT_COMPAT)
    if (HAVE_HW_BREAKPOINT_H)
        message(WARNING "Using inaccurate compatibility time synchronization while hardware breakpoints are available. Consider deactivating USE_HW_BREAKPOINT_COMPAT")
    endif()
else()
    if (NOT HAVE_HW_BREAKPOINT_H)
        message(SEND_ERROR "Missing linux/hw_breakpoint.h, install that or consider activating USE_HW_BREAKPOINT_COMPAT for old kernels with compatibility time synchronization.")
    endif()
endif()


configure_file(include/lo2s/build_config.hpp.in include/lo2s/build_config.hpp)

# define include directory of target lo2s
target_include_directories(lo2s PRIVATE
    include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

add_subdirectory(man)

message(STATUS "Linux kernel version: ${LINUX_VERSION}")

target_compile_features(lo2s PRIVATE cxx_std_17)

# define feature test macro
target_compile_definitions(lo2s PRIVATE _GNU_SOURCE)

# build Debug with -Werror
target_compile_options(lo2s PRIVATE $<$<CONFIG:Debug>:-Werror> -Wall -pedantic -Wextra)

if (IWYU)
    find_program(iwyu_path NAMES include-what-you-use iwyu)
    if(NOT iwyu_path)
        message(FATAL_ERROR "Could not find the program include-what-you-use")
    endif()
    set_property(TARGET lo2s PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path})
endif()

#option for generating graphs of the code if doxygen and graphviz are present
if(DOXYGEN_FOUND)
    set(DOXYGEN_EXTRACT_ALL YES)
    set(DOXYGEN_EXTRACT_PRIVATE YES)
    set(DOXYGEN_EXTRACT_LOCAL_CLASSES YES)
    set(DOXYGEN_CLASS_DIAGRAMS YES)
    set(DOXYGEN_HIDE_UNDOC_RELATIONS YES)
    set(DOXYGEN_CLASS_GRAPH YES)
    set(DOXYGEN_COLLABORATION_GRAPH YES)
    if(UML_LOOK)
        set(DOXYGEN_UML_LOOK YES)
        set(DOXYGEN_UML_LIMIT_NUM_FIELDS 100)
    endif()
    set(DOXYGEN_INCLUDE_GRAPH YES)
    set(DOXYGEN_INCLUDED_BY_GRAPH YES)
    set(DOXYGEN_GRAPHICAL_HIERARCHY YES)
    set(DOXYGEN_DOT_GRAPH_MAX_NODES 100)
    set(DOXYGEN_DOT_GRAPH_DEPTH 1000)
    doxygen_add_docs(doc src include)
endif()

# Convince clion to accept headers as part of the project by adding them to a dummy target
# Should not interfer with normal opratation.
FILE(GLOB_RECURSE clion_dummy_headers *.hpp *.h)
FILE(GLOB_RECURSE clion_dummy_source main.cpp)
add_executable(clion_dummy_executable EXCLUDE_FROM_ALL ${clion_dummy_source} ${clion_dummy_headers})

install(TARGETS lo2s RUNTIME DESTINATION bin)

find_program(GIT_ARCHIVE_ALL git-archive-all PATHS ENV PATH)
if(GIT_ARCHIVE_ALL)
    set(ARCHIVE_NAME ${CMAKE_PROJECT_NAME}-${LO2S_VERSION_STRING})
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.bz2
        COMMAND cmake -E remove -f ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.bz2
        COMMAND git-archive-all --prefix ${ARCHIVE_NAME}/ ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.bz2
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    add_custom_target(dist DEPENDS ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.bz2)
else()
    message(STATUS "Couldn't find git-archive-all. The target `make dist` will not be available. Use 'pip install git-archive-all' to install.")
endif()

feature_summary(WHAT ENABLED_FEATURES DISABLED_FEATURES)
