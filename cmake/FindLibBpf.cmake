# SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
option(LibBpf_USE_STATIC_LIBS "Link LibBpf statically." ON)

find_package(PkgConfig)
find_package(LibDw)

if(NOT LibDw_FOUND)
    return()
endif()

if(NOT PKG_CONFIG_FOUND)
    return()
endif()

pkg_check_modules(LIBBPF_PKG_CONFIG libbpf)

if(NOT LIBBPF_PKG_CONFIG_FOUND)
    return()
endif()

find_path(LIBBPF_INCLUDE_DIRS
  NAMES
    bpf/bpf.h
    bpf/btf.h
    bpf/libbpf.h
    PATHS
    ${LIBBPF_PKG_CONFIG_INCLUDDE_DIRS}
    ENV CPATH)


if(LibBpf_USE_STATIC_LIBS)
find_library(LIBBPF_LIBRARIES
  NAMES
    libbpf.a
    HINTS
    ${LIBBPF_PKG_CONFIG_STATIC_LIBRARY_DIRS}
    ENV LIBRARY_PATH
    )
else()
find_library(LIBBPF_LIBRARIES
  NAMES
    libbpf.so
    HINTS
    ${LIBBPF_PKG_CONFIG_LIBRARY_DIRS}
    ENV LIBRARY_PATH
    ENV LD_LIBRARY_PATH)
endif()

include (FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibBpf DEFAULT_MSG
  LIBBPF_LIBRARIES
  LIBBPF_INCLUDE_DIRS)


if(LibBpf_FOUND)
    add_library(LibBpf::LibBpf UNKNOWN IMPORTED)
    set_property(TARGET LibBpf::LibBpf PROPERTY IMPORTED_LOCATION ${LIBBPF_LIBRARIES})
    target_link_libraries(LibBpf::LibBpf INTERFACE LibDw::LibDw)
    target_include_directories(LibBpf::LibBpf INTERFACE ${LIBBPF_INCLUDE_DIRS})
endif()
mark_as_advanced(LIBBPF_INCLUDE_DIRS LIBBPF_LIBRARIES)
