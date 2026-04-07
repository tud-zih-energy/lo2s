# SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
#
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.11)

project(lo2s VERSION 1.8.0)
set(LO2S_COPYRIGHT_YEAR "2025")

# Check if we are running Linux
if(NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    message(FATAL_ERROR "You can only build and run lo2s on a Linux machine. I'm sorry.")
endif()

include(CMakeDependentOption)
include(CheckIncludeFiles)
include(CheckStructHasMember)
include(CheckFunctionExists)
include(FeatureSummary)
include(GNUInstallDirs)

SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

include(cmake/DefaultBuildType.cmake)
include(cmake/UnsetIfUpdated.cmake)
include(cmake/CheckNameExists.cmake)
include(cmake/CheckStructHasBitField.cmake)
include(cmake/PkgConfigWrapper.cmake)

# Intialize git submodules if not done already
include(cmake/GitSubmoduleUpdate.cmake)
git_submodule_update()

# Set of variables to enforce static or dynamic linking
set(lo2s_USE_STATIC_LIBS "OFF" CACHE STRING "Link lo2s statically.")
set_property(CACHE lo2s_USE_STATIC_LIBS PROPERTY STRINGS "MOSTLY" "OFF" "ALL")

IfUpdatedUnsetAll(lo2s_USE_STATIC_LIBS
    Dl_USE_STATIC_LIBS
    LibDw_USE_STATIC_LIBS
    OTF2_USE_STATIC_LIBS
    OTF2XX_USE_STATIC_LIBS
    Libpfm_USE_STATIC_LIBS
    X86Adapt_STATIC
    x86_energy_STATIC
    CUDA_USE_STATIC_LIBS
    Sensors_USE_STATIC_LIBS
    Veosinfo_USE_STATIC_LIBS
    Audit_USE_STATIC_LIBS
    Debuginfod_USE_STATIC_LIBS
    LibBpf_USE_STATIC_LIBS)

if(lo2s_USE_STATIC_LIBS STREQUAL "OFF")
    set(Dl_USE_STATIC_LIBS       OFF CACHE BOOL "")
    set(LibDw_USE_STATIC_LIBS    OFF CACHE BOOL "")
    set(OTF2_USE_STATIC_LIBS     OFF CACHE BOOL "")
    set(OTF2XX_USE_STATIC_LIBS   OFF CACHE BOOL "")
    set(X86Adapt_STATIC          OFF CACHE BOOL "")
    set(x86_energy_STATIC        OFF CACHE BOOL "")
    set(Sensors_USE_STATIC_LIBS  OFF CACHE BOOL "")
    set(Libpfm_USE_STATIC_LIBS   OFF CACHE BOOL "")
    set(CUDA_USE_STATIC_LIBS     OFF CACHE BOOL "")
    set(Veosinfo_USE_STATIC_LIBS OFF CACHE BOOL "")
    set(Audit_USE_STATIC_LIBS    OFF CACHE BOOL "")
    set(LibBpf_USE_STATIC_LIBS   OFF CACHE BOOL "")
else()
    if(lo2s_USE_STATIC_LIBS STREQUAL "MOSTLY")
        set(Dl_USE_STATIC_LIBS       OFF CACHE BOOL "")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
    elseif(lo2s_USE_STATIC_LIBS STREQUAL "ALL")
        set(Dl_USE_STATIC_LIBS       ON CACHE BOOL "")
        # Doesn't seem to work with clang, even though it should,
        # but at least it doesn't complain about it either
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc -static")
        set(CMAKE_LINK_SEARCH_START_STATIC 1)
        set(CMAKE_LINK_SEARCH_END_STATIC 1)
    endif()
    set(Dl_USE_STATIC_LIBS       OFF CACHE BOOL "")
    set(LibDw_USE_STATIC_LIBS    ON CACHE BOOL "")
    set(OTF2_USE_STATIC_LIBS     ON CACHE BOOL "")
    set(OTF2XX_USE_STATIC_LIBS   ON CACHE BOOL "")
    set(X86Adapt_STATIC          ON CACHE BOOL "")
    set(x86_energy_STATIC        ON CACHE BOOL "")
    set(Sensors_USE_STATIC_LIBS  ON CACHE BOOL "")
    set(Libpfm_USE_STATIC_LIBS   ON CACHE BOOL "")
    set(CUDA_USE_STATIC_LIBS     ON CACHE BOOL "")
    set(Veosinfo_USE_STATIC_LIBS ON CACHE BOOL "")
    set(Audit_USE_STATIC_LIBS    ON CACHE BOOL "")
    set(LibBpf_USE_STATIC_LIBS   ON CACHE BOOL "")
    set(Debuginfod_USE_STATIC_LIBS     ON CACHE BOOL "")
endif()

# configure OTF2xx submodule
set(OTF2XX_CHRONO_DURATION_TYPE nanoseconds CACHE INTERNAL "")
add_subdirectory(lib/otf2xx)
mark_as_advanced(OTF2XX_WITH_MPI OTF2_CONFIG OTF2_PRINT)

add_subdirectory(lib/json)

add_subdirectory(lib/fmt)

# configure Nitro submodule
add_subdirectory(lib/nitro)

#configure pmu-events submodule
add_subdirectory(lib/pmu-events)

# configure x64_adapt_cxx library
include(lib/x86_adapt/x86_adapt.cmake)

# find external dependencies
find_package(Git)
set(THREADS_PREFER_PTHREAD_FLAG true)
find_package(Threads REQUIRED)
find_package(Doxygen COMPONENTS dot)
find_package(x86_energy 2.0 CONFIG)
find_package(StdFilesystem REQUIRED)
find_package(Sensors)
find_package(Veosinfo)
find_package(Libpfm)
find_package(Audit)
find_package(BpfObject)
find_package(LibDw REQUIRED)
find_package(Debuginfod)

# These libraries are used in the lo2s injection shared library
# so don't search for them in a pure static build, because that would
# be pretty pointless
if(NOT ${lo2s_USE_STATIC_LIBS} STREQUAL "ALL")
    find_package(CUDAToolkit)
    find_package(rocprofiler-sdk NO_MODULE)
    find_package(OpenMP)

    include(CheckIncludeFileCXX)
    if(OpenMP_CXX_FOUND)
        check_include_file_cxx("omp-tools.h" OMPT_FOUND CMAKE_REQUIRED_LIBRARIES OpenMP::OpenMP_CXX)
    endif()
endif()

CHECK_INCLUDE_FILES(linux/hw_breakpoint.h HAVE_HW_BREAKPOINT_H)

# system configuration checks
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

check_function_exists(shm_open SHM_OPEN_FOUND)
if(NOT SHM_OPEN_FOUND)
    set(CMAKE_REQUIRED_LIBRARIES "rt")
    check_function_exists(shm_open SHM_OPEN_FOUND_WITH_RT)
    unset(CMAKE_REQUIRED_LIBRARIES)
endif()

CHECK_STRUCT_HAS_BITFIELD("struct perf_event_attr" context_switch linux/perf_event.h HAVE_PERF_RECORD_SWITCH)

if(NOT HAVE_PERF_RECORD_SWITCH)
    message(FATAL_ERROR "lo2s requires support for perf context switch recording. Make sure that you are running on a kernel that support context_switch with perf_event_open")
endif()
macro(lo2s_dependent_option _USE_OPTION _FOUND_VAL _DESC)
    CMAKE_DEPENDENT_OPTION(${_USE_OPTION} "${_DESC}" ON ${_FOUND_VAL} OFF)
    add_feature_info("${_USE_OPTION}" ${_USE_OPTION} ${_DESC})
endmacro()

lo2s_dependent_option(USE_HW_BREAKPOINT_COMPAT "NOT ${HAVE_HW_BREAKPOINT_H}" "Time synchronization fallback for architectures without hardware breakpoint support.")
lo2s_dependent_option(USE_SENSORS "Sensors_FOUND" "Use the libsensors to read system metrics.")
lo2s_dependent_option(USE_LIBPFM "Libpfm_FOUND" "Use libpfm to retrieve metric information.")
lo2s_dependent_option(USE_LIBAUDIT Audit_FOUND "Use libaudit for syscall name resolution")
lo2s_dependent_option(USE_VEOSINFO "Veosinfo_FOUND" "Use libveosinfo to sample NEC SX-Aurora Tsubasa cards.")
lo2s_dependent_option(USE_CUPTI "CUDAToolkit_FOUND" "Use CUPTI to record CUDA activity.")
lo2s_dependent_option(USE_BPF BpfObject_FOUND "Use BPF to record POSIX I/O activity")
lo2s_dependent_option(USE_DEBUGINFOD "Debuginfod_FOUND" "Use Debuginfod to download debug information on-demand.")
lo2s_dependent_option(USE_OPENMP "OMPT_FOUND" "Use OMPT to record OpenMP activity")
lo2s_dependent_option(USE_HIP "${rocprofiler-sdk_FOUND}" "Use rocprofiler-sdk to record AMD HIP activity")
lo2s_dependent_option(USE_X86_ADAPT "${X86Adapt_FOUND}" "Use x86_adapt to provide extra hardware metrics")
lo2s_dependent_option(USE_X86_ENERGY "${x86_energy_FOUND}" "Use x86_energy  to provide extra power metrics")

option(UML_LOOK "Generate graphs with an UML look" OFF)


# prepare list of source files
set(SOURCE_FILES
    src/types/cpu.cpp
    src/types/thread.cpp
    src/types/process.cpp
    src/execution_scope.cpp

    src/metric/plugin/plugin.cpp
    src/metric/plugin/channel.cpp
    src/metric/plugin/metrics.cpp

    src/monitor/cpu_set_monitor.cpp
    src/monitor/poll_monitor.cpp
    src/monitor/main_monitor.cpp
    src/monitor/process_monitor.cpp
    src/monitor/system_process_monitor.cpp
    src/monitor/process_monitor_main.cpp
    src/monitor/scope_monitor.cpp
    src/monitor/threaded_monitor.cpp
    src/monitor/tracepoint_monitor.cpp
    src/monitor/socket_monitor.cpp
    src/monitor/gpu_monitor.cpp
    src/monitor/openmp_monitor.cpp

    src/process_controller.cpp

    src/resolvers.cpp
    src/resolvers/kallsyms.cpp
    src/resolvers/perf_map.cpp

    src/rb/shm_ringbuf.cpp
    src/rb/reader.cpp
    src/rb/writer.cpp

    src/perf/event_resolver.cpp
    src/perf/event_attr.cpp
    src/perf/bio/block_device.cpp
    src/perf/bio/writer.cpp
    src/perf/event_composer.cpp
    src/perf/counter/metric_writer.cpp
    src/perf/counter/group/reader.cpp
    src/perf/counter/group/writer.cpp
    src/perf/counter/userspace/reader.cpp
    src/perf/counter/userspace/writer.cpp
    src/perf/sample/writer.cpp
    src/perf/time/converter.cpp src/perf/time/reader.cpp
    src/perf/tracepoint/writer.cpp
    src/perf/tracepoint/event_attr.cpp
    src/perf/syscall/writer.cpp

    src/resolvers.cpp
    src/resolvers/kallsyms.cpp

    src/time/time.cpp

    src/local_cctx_tree.cpp
    src/trace/trace.cpp

    src/config.cpp
    src/config/monitor_type.cpp
    src/config/otf2_config.cpp
    src/config/general_config.cpp
    src/config/dwarf_config.cpp
    src/config/rb_config.cpp
    src/config/accelerator_config.cpp
    src/config/x86_adapt_config.cpp
    src/config/x86_energy_config.cpp
    src/config/put_config.cpp
    src/config/as_user_config.cpp
    src/config/perf_config.cpp
    src/config/perf/tracepoint_config.cpp
    src/config/perf/syscall_config.cpp
    src/config/perf/group_config.cpp
    src/config/perf/userspace_config.cpp
    src/config/perf/sampling_config.cpp
    src/config/perf/block_io_config.cpp
    src/config/perf/posix_io_config.cpp
    src/config/sensors_config.cpp

    src/main.cpp src/monitor/process_monitor.cpp
    src/topology.cpp src/dwarf_resolve.cpp
    src/function_resolver.cpp
    src/util.cpp
    src/perf/util.cpp
    src/syscalls.cpp
    src/summary.cpp
)

# define lo2s target
add_executable(lo2s ${SOURCE_FILES})

target_link_libraries(lo2s
    PRIVATE
        otf2xx::Writer
        Nitro::log
        Nitro::env
        Nitro::dl
        Nitro::options
        Threads::Threads
        fmt::fmt-header-only
        std::filesystem
        LibDw::LibDw
        PMUEvents::pmu-events
        nlohmann_json::nlohmann_json
    )

target_include_directories(lo2s PRIVATE
    include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

# old glibc versions require -lrt for clock_gettime()
if(NOT CLOCK_GETTIME_FOUND)
    if(CLOCK_GETTIME_FOUND_WITH_RT)
        target_link_libraries(lo2s PRIVATE rt)
    else()
        message(SEND_ERROR "Could not find the function clock_gettime(), but it is required.")
    endif()
endif()

if(NOT SHM_OPEN_FOUND)
    if(SHM_OPEN_FOUND_WITH_RT)
        target_link_libraries(lo2s PRIVATE rt)
    else()
        message(SEND_ERROR "Could not find the function shm_open(), but it is required.")
    endif()
endif()

# handle x86_adapt dependency
if(USE_X86_ADAPT)
    target_sources(lo2s PRIVATE
        src/metric/x86_adapt/metrics.cpp
        src/metric/x86_adapt/monitor.cpp
        src/metric/x86_adapt/node_monitor.cpp
        src/config/x86_adapt_config.cpp
        src/metric/x86_adapt/knobs.cpp
    )
    target_link_libraries(lo2s PRIVATE x86_adapt::x86_adapt)
endif()

# handle x86_energy dependency
if(USE_X86_ENERGY)
    target_compile_definitions(lo2s PRIVATE HAVE_X86_ENERGY)
    target_sources(lo2s PRIVATE
        src/metric/x86_energy/metrics.cpp
        src/metric/x86_energy/monitor.cpp
        src/config/x86_energy_config.cpp
    )
    if(x86_energy_STATIC)
        target_link_libraries(lo2s PRIVATE x86_energy::x86_energy-static)
    else()
        target_link_libraries(lo2s PRIVATE x86_energy::x86_energy)
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

if (USE_VEOSINFO)
        target_compile_definitions(lo2s PUBLIC HAVE_VEOSINFO)
        target_link_libraries(lo2s PRIVATE Veosinfo::veosinfo)
        target_sources(lo2s PRIVATE src/monitor/nec_thread_monitor.cpp
                                    src/monitor/nec_monitor_main.cpp)
endif()

if (USE_DEBUGINFOD)
        target_compile_definitions(lo2s PUBLIC HAVE_DEBUGINFOD)
        target_link_libraries(lo2s PRIVATE Debuginfod::Debuginfod)
endif()

if (USE_LIBPFM)
    target_compile_definitions(lo2s PUBLIC HAVE_LIBPFM)
    target_link_libraries(lo2s PRIVATE Libpfm::libpfm)
endif()

if (USE_LIBAUDIT)
        target_compile_definitions(lo2s PUBLIC HAVE_LIBAUDIT)
        target_link_libraries(lo2s PRIVATE Audit::Audit)
endif()

set(LO2S_INJECTIONLIB_PATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

if(USE_CUPTI OR USE_OPENMP OR USE_HIP)
    add_library(lo2s_injection SHARED)

    target_include_directories(lo2s_injection PRIVATE include
            ${CMAKE_CURRENT_BINARY_DIR}/include)
    target_link_libraries(lo2s_injection PRIVATE fmt::fmt-header-only
            Nitro::log
            Nitro::env
            Nitro::dl
            Nitro::options
            otf2xx::Writer)

    if(SHM_OPEN_FOUND_WITH_RT)
        target_link_libraries(lo2s_injection PRIVATE rt)
    endif()

    install(TARGETS lo2s_injection LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

if(USE_CUPTI)
        target_sources(lo2s_injection PRIVATE src/cuda/lib.cpp src/types/process.cpp src/time/time.cpp)
        if (CUDA_USE_STATIC_LIBS)
            target_link_libraries(lo2s_injection PRIVATE CUDA::cupti_static)
        else()
            target_link_libraries(lo2s_injection PRIVATE CUDA::cupti)
        endif()

        if(SHM_OPEN_FOUND_WITH_RT)
            target_link_libraries(lo2s_injection PRIVATE rt)
        endif()

        target_compile_definitions(lo2s PUBLIC HAVE_CUDA)
endif()

if(USE_OPENMP)
        target_sources(lo2s_injection PRIVATE src/ompt/lib.cpp)
        target_link_libraries(lo2s_injection PRIVATE OpenMP::OpenMP_CXX)
        target_compile_definitions(lo2s PUBLIC HAVE_OPENMP)
endif()

if(USE_HIP)
        target_sources(lo2s_injection PRIVATE src/hip/lib.cpp)
        target_link_libraries(lo2s_injection PRIVATE rocprofiler-sdk::rocprofiler-sdk)
        target_compile_definitions(lo2s PUBLIC HAVE_HIP)
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

CHECK_NAME_EXISTS(PERF_COUNT_SW_ALIGNMENT_FAULTS linux/perf_event.h HAVE_PERF_EVENT_ALIGNMENT_FAULTS)
CHECK_NAME_EXISTS(PERF_COUNT_SW_EMULATION_FAULTS linux/perf_event.h HAVE_PERF_EVENT_EMULATION_FAULTS)
CHECK_NAME_EXISTS(PERF_COUNT_HW_REF_CPU_CYCLES linux/perf_event.h HAVE_PERF_EVENT_REF_CYCLES)
CHECK_NAME_EXISTS(PERF_COUNT_HW_STALLED_CYCLES_FRONTEND linux/perf_event.h HAVE_PERF_EVENT_STALLED_CYCLES_FRONTEND)
CHECK_NAME_EXISTS(PERF_COUNT_HW_STALLED_CYCLES_BACKEND linux/perf_event.h HAVE_PERF_EVENT_STALLED_CYCLES_BACKEND)
CHECK_NAME_EXISTS(PERF_COUNT_HW_CACHE_NODE linux/perf_event.h HAVE_PERF_EVENT_CACHE_NODE)
CHECK_NAME_EXISTS(PERF_COUNT_SW_DUMMY linux/perf_event.h HAVE_PERF_EVENT_DUMMY)
CHECK_NAME_EXISTS(PERF_COUNT_SW_BPF_OUTPUT linux/perf_event.h HAVE_PERF_EVENT_BPF_OUTPUT)
CHECK_NAME_EXISTS(PERF_COUNT_SW_CGROUP_SWITCHES linux/perf_event.h HAVE_PERF_EVENT_CGROUP_SWITCHES)

CHECK_NAME_EXISTS(CLOCK_MONOTONIC_RAW linux/time.h HAVE_CLOCK_MONOTONIC_RAW)
CHECK_NAME_EXISTS(CLOCK_MONOTONIC_COARSE linux/time.h HAVE_CLOCK_MONOTONIC_COARSE)
CHECK_NAME_EXISTS(CLOCK_REALTIME_COARSE linux/time.h HAVE_CLOCK_REALTIME_COARSE)
CHECK_NAME_EXISTS(CLOCK_BOOTTIME linux/time.h HAVE_CLOCK_BOOTTIME)
CHECK_NAME_EXISTS(CLOCK_REALTIME_ALARM linux/time.h HAVE_CLOCK_REALTIME_ALARM)
CHECK_NAME_EXISTS(CLOCK_BOOTTIME_ALARM linux/time.h HAVE_CLOCK_BOOTTIME_ALARM)

CHECK_NAME_EXISTS(PERF_RECORD_LOST_SAMPLES linux/perf_event.h HAVE_PERF_RECORD_LOST_SAMPLES)

if (USE_HW_BREAKPOINT_COMPAT)
    if (HAVE_HW_BREAKPOINT_H)
        message(WARNING "Using inaccurate compatibility time synchronization while hardware breakpoints are available. Consider deactivating USE_HW_BREAKPOINT_COMPAT")
    endif()
endif()

configure_file(include/lo2s/build_config.hpp.in include/lo2s/build_config.hpp)

if (USE_BPF)
    target_compile_definitions(lo2s PUBLIC HAVE_BPF)
    target_sources(lo2s PRIVATE src/monitor/posix_monitor.cpp)
    bpf_object(posix_io src/perf/posix_io/posix_io.bpf.c)
    add_dependencies(lo2s posix_io_skel)
    target_link_libraries(lo2s PUBLIC posix_io_skel)
endif()

add_subdirectory(man)

target_compile_features(lo2s PRIVATE cxx_std_17)

# build Debug with -Werror. Disable overlength string detection, though, as this
# gives us false positives from the compiled BPF code, which is included in lo2s
# as one big string.
target_compile_options(lo2s PRIVATE $<$<CONFIG:Debug>:-Werror> -Wno-overlength-strings -Wall -pedantic -Wextra)

enable_testing()
include(cmake/Tests.cmake)

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

install(TARGETS lo2s RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

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
