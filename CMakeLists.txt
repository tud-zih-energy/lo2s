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

find_package(Boost REQUIRED COMPONENTS system program_options filesystem)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

find_package(Radare REQUIRED)
include_directories(SYSTEM ${Radare_INCLUDE_DIRS})

#find_package(BFD REQUIRED)
#include_directories(SYSTEM ${BFD_INCLUDE_DIRS})
set(BFD_LIBRARIES "-lbfd -lz -ldl -liberty")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -pedantic -Wextra -pthread")

include_directories(include)

set(SOURCE_FILES
        src/main.cpp src/monitor.cpp src/thread_map.cpp src/thread_monitor.cpp
        src/platform.cpp src/perf_sample_otf2.cpp src/otf2_trace.cpp src/time.cpp
        src/topology.cpp src/radare.cpp
        src/bfd_resolve.cpp src/pipe.cpp src/address.hpp src/mmap.cpp src/perf_counter.cpp
        src/otf2_counters.hpp src/otf2_counters.cpp src/metric_plugin.cpp src/metric_channel.cpp
        src/metrics.cpp src/perf_sample_raw/perf_sample_raw_otf2.cpp
    )

add_executable(lo2s ${SOURCE_FILES})
target_link_libraries(lo2s ${OTF2_LIBRARIES} ${Boost_LIBRARIES} ${Radare_LIBRARIES}
        ${BFD_LIBRARIES})


# Convince clion to accept headers as part of the project by adding them to a dummy target
# Should not interfer with normal opratation.
FILE(GLOB_RECURSE clion_dummy_headers *.hpp *.h)
FILE(GLOB_RECURSE clion_dummy_source main.cpp)
add_executable(clion_dummy_executable EXCLUDE_FROM_ALL ${clion_dummy_source} ${clion_dummy_headers})

install(TARGETS lo2s RUNTIME DESTINATION bin)
