cmake_minimum_required(VERSION 3.11)

project(lo2s VERSION 1.8.0)
set(LO2S_COPYRIGHT_YEAR "2025")


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
    Radare_USE_STATIC_LIBS
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
    set(Radare_USE_STATIC_LIBS   OFF CACHE BOOL "")
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
    set(Radare_USE_STATIC_LIBS   ON CACHE BOOL "")
    set(Audit_USE_STATIC_LIBS    ON CACHE BOOL "")
    set(LibBpf_USE_STATIC_LIBS   ON CACHE BOOL "")
    set(Debuginfod_USE_STATIC_LIBS     ON CACHE BOOL "")
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

add_subdirectory(lib/indicators)

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
    find_package(rocprofiler-sdk)
    find_package(OpenMP)

    include(CheckIncludeFileCXX)
    if(OpenMP_CXX_FOUND)
        check_include_file_cxx("omp-tools.h" OMPT_FOUND CMAKE_REQUIRED_LIBRARIES OpenMP::OpenMP_CXX)
    endif()
endif()

option(Radare_USE_STATIC_LIBS "Link Radare statically" OFF)

if(Radare_USE_STATIC_LIBS)
    set(R_asm_USE_STATIC_LIBS ON)
    set(R_anal_USE_STATIC_LIBS ON)
endif()
FindPkgConfigWrapper(R_asm "r_asm>=5.8.0" r_asm)
FindPkgConfigWrapper(R_anal "r_anal>=5.8.0" r_anal)

if(R_anal_FOUND AND R_asm_FOUND)
    set(Radare_FOUND TRUE)
endif()



# configurable options
option(IWYU "Developer option for include what you use." OFF)
option(UML_LOOK "Generate graphs with an UML look" OFF)
option(USE_HW_BREAKPOINT_COMPAT "Time synchronization fallback for old kernels without hardware breakpoint support." OFF)
add_feature_info("USE_HW_BREAKPOINT_COMPAT" USE_HW_BREAKPOINT_COMPAT "Time synchronization fallback for old kernels without hardware breakpoint support.")
CMAKE_DEPENDENT_OPTION(USE_RADARE "Enable Radare support." ON Radare_FOUND OFF)
add_feature_info("USE_RADARE" USE_RADARE "Use Radare to add instruction information to samples.")
CMAKE_DEPENDENT_OPTION(USE_SENSORS "Use the libsensors to read system metrics." ON "Sensors_FOUND" OFF)
add_feature_info("USE_SENSORS" USE_SENSORS "Use the libsensors to read system metrics.")
CMAKE_DEPENDENT_OPTION(USE_LIBPFM "Use libpfm to retrieve metric information." ON "Libpfm_FOUND" OFF)
add_feature_info("USE_LIBPFM" USE_LIBPFM "Use libpfm to retrieve metric information.")
CMAKE_DEPENDENT_OPTION(USE_LIBAUDIT "Use libaudit for syscall name resolution" ON Audit_FOUND OFF)
add_feature_info("USE_LIBAUDIT" USE_LIBAUDIT "Use libaudit for syscall name resolution.")
CMAKE_DEPENDENT_OPTION(USE_VEOSINFO "Use libveosinfo to sample NEC SX-Aurora Tsubasa cards." ON "Veosinfo_FOUND" OFF)
add_feature_info("USE_VEOSINFO" USE_VEOSINFO "Use libveosinfo to sample NEC SX-Aurora  Tsubasa cards.")
CMAKE_DEPENDENT_OPTION(USE_CUPTI "Use CUPTI to record CUDA activity." ON "CUDAToolkit_FOUND" OFF)
add_feature_info("USE_CUPTI" USE_CUPTI "Use CUPTI to record CUDA activity.")
CMAKE_DEPENDENT_OPTION(USE_BPF "Use BPF to record POSIX I/O activity" ON BpfObject_FOUND OFF)
add_feature_info("USE_BPF" USE_BPF "Use BPF to record POSIX I/O activity")
CMAKE_DEPENDENT_OPTION(USE_DEBUGINFOD "Use Debuginfod to download debug information on-demand." ON "Debuginfod_FOUND" OFF)
add_feature_info("USE_DEBUGINFOD" USE_DEBUGINFOD "Use Debuginfod to download debug information on-demand.")
CMAKE_DEPENDENT_OPTION(USE_OPENMP "Use OMPT to record OpenMP activity" ON "OMPT_FOUND" OFF)
add_feature_info("USE_OPENMP" USE_OPENMP "Use OMPT to record OpenMP activity")
CMAKE_DEPENDENT_OPTION(USE_HIP "Use rocprofiler-sdk to record AMD HIP activity" ON "rocprofiler-sdk_FOUND" OFF)
add_feature_info("USE_HIP" USE_HIP "Use rocprofiler-sdk to record AMD HIP activity")

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

# detect version of running kernel
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" LINUX_VERSION ${CMAKE_SYSTEM_VERSION})

# prepare list of source files
set(SOURCE_FILES
    src/types.cpp

    src/helpers/fd.cpp

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
    src/process_controller.cpp

    src/perf/event_resolver.cpp
    src/perf/event_attr.cpp
    src/perf/event_guard.cpp
    src/monitor/socket_monitor.cpp
    src/monitor/gpu_monitor.cpp
    src/monitor/openmp_monitor.cpp

    src/perf/bio/block_device.cpp
    src/perf/event_composer.cpp
    src/perf/counter/group/reader.cpp
    src/perf/counter/userspace/reader.cpp

    src/perf/counter/userspace/writer.cpp
    src/perf/counter/group/writer.cpp

    src/perf/sample/writer.cpp
    src/perf/time/converter.cpp src/perf/time/reader.cpp
    src/perf/tracepoint/writer.cpp
    src/perf/tracepoint/event_attr.cpp
    src/perf/syscall/writer.cpp

    src/time/time.cpp

    src/local_cctx_tree.cpp
    src/trace/trace.cpp

    src/config.cpp src/main.cpp src/monitor/process_monitor.cpp
    src/platform.cpp
    src/topology.cpp src/dwarf_resolve.cpp
    src/function_resolver.cpp
    src/instruction_resolver.cpp
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
        fmt::fmt-header-only
        std::filesystem
        LibDw::LibDw
        indicators::indicators
        PMUEvents::pmu-events
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
        target_link_libraries(lo2s PRIVATE R_asm::R_asm R_anal::R_anal)
        target_sources(lo2s PRIVATE
            src/radare.cpp
        )
    else()
        message(SEND_ERROR "Radare2 not found but requested.")
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
    if(Veosinfo_FOUND)
        target_compile_definitions(lo2s PUBLIC HAVE_VEOSINFO)
        target_link_libraries(lo2s PRIVATE Veosinfo::veosinfo)
        target_sources(lo2s PRIVATE src/monitor/nec_thread_monitor.cpp
                                    src/monitor/nec_monitor_main.cpp)
    else()
        message(SEND_ERROR "Veosinfo not found but requested.")
    endif()
endif()

if (USE_DEBUGINFOD)
    if (Debuginfod_FOUND)
        target_compile_definitions(lo2s PUBLIC HAVE_DEBUGINFOD)
        target_link_libraries(lo2s PRIVATE Debuginfod::Debuginfod)
    else()
        message(SEND_ERROR "Debuginfod not found but requested")
    endif()
endif()

if (USE_LIBPFM)
    if (Libpfm_FOUND)
        target_compile_definitions(lo2s PUBLIC HAVE_LIBPFM)
        target_link_libraries(lo2s PRIVATE Libpfm::libpfm)
    else()
        message(SEND_ERROR "Libpfm not found but requested.")
    endif()
endif()

if (USE_LIBAUDIT)
    if (Audit_FOUND)
        target_compile_definitions(lo2s PUBLIC HAVE_LIBAUDIT)
        target_link_libraries(lo2s PRIVATE Audit::Audit)
    else()
        message(SEND_ERROR "Libaudit not found but requested.")
    endif()
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
    if(CUDAToolkit_FOUND)
        target_sources(lo2s_injection PRIVATE src/cuda/lib.cpp src/types.cpp src/time/time.cpp)
        if (CUDA_USE_STATIC_LIBS)
            target_link_libraries(lo2s_injection PRIVATE CUDA::cupti_static)
        else()
            target_link_libraries(lo2s_injection PRIVATE CUDA::cupti)
        endif()

        if(SHM_OPEN_FOUND_WITH_RT)
            target_link_libraries(lo2s_injection PRIVATE rt)
        endif()

        target_compile_definitions(lo2s PUBLIC HAVE_CUDA)
    else()
        message(SEND_ERROR "Cupti not found but requested.")
    endif()
endif()

if(USE_OPENMP)
    if(OMPT_FOUND)
        target_sources(lo2s_injection PRIVATE src/ompt/lib.cpp)
        target_link_libraries(lo2s_injection PRIVATE OpenMP::OpenMP_CXX)
        target_compile_definitions(lo2s PUBLIC HAVE_OPENMP)
    else()
        message(SEND_ERROR "OpenMP not supported but requested.")
    endif()
endif()

if(USE_HIP)
    if(rocprofiler-sdk_FOUND)
        target_sources(lo2s_injection PRIVATE src/hip/lib.cpp)
        target_link_libraries(lo2s_injection PRIVATE rocprofiler-sdk::rocprofiler-sdk)
        target_compile_definitions(lo2s PUBLIC HAVE_HIP)
    else()
        message(SEND_ERROR "HIP activity recording not supported but requested.")
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
CHECK_NAME_EXISTS(PERF_COUNT_SW_DUMMY linux/perf_event.h HAVE_PERF_EVENT_DUMMY)
CHECK_NAME_EXISTS(PERF_COUNT_SW_BPF_OUTPUT linux/perf_event.h HAVE_PERF_EVENT_BPF_OUTPUT)
CHECK_NAME_EXISTS(PERF_COUNT_SW_CGROUP_SWITCHES linux/perf_event.h HAVE_PERF_EVENT_CGROUP_SWITCHES)

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


if (USE_BPF)
    if (BpfObject_FOUND)
        target_compile_definitions(lo2s PUBLIC HAVE_BPF)
        bpf_object(posix_io src/perf/posix_io/posix_io.bpf.c)
        add_dependencies(lo2s posix_io_skel)
        target_link_libraries(lo2s PRIVATE posix_io_skel)
    else()
        message(SEND_ERROR "BPF not found but requested.")
    endif()
endif()

add_subdirectory(man)

message(STATUS "Linux kernel version: ${LINUX_VERSION}")

target_compile_features(lo2s PRIVATE cxx_std_17)

# define feature test macro
target_compile_definitions(lo2s PRIVATE _GNU_SOURCE)

# build Debug with -Werror. Disable overlength string detection, though, as this
# gives us false positives from the compiled BPF code, which is included in lo2s
# as one big string.
target_compile_options(lo2s PRIVATE $<$<CONFIG:Debug>:-Werror> -Wno-overlength-strings -Wall -pedantic -Wextra)

enable_testing()
include(cmake/Tests.cmake)

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
