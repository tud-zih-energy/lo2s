# SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
option(LibBpf_USE_STATIC_LIBS "Link LibBpf statically." ON)

find_path(LIBBPF_INCLUDE_DIRS
  NAMES
    bpf/bpf.h
    bpf/btf.h
    bpf/libbpf.h
  PATHS
    /usr/include
    /usr/local/include
    /opt/local/include
    /sw/include
    ENV CPATH)


if(LibBpf_USE_STATIC_LIBS)
find_library(LIBBPF_LIBRARIES
  NAMES
    libbpf.a
  PATHS
    /usr/lib
    /usr/local/lib
    /opt/local/lib
    /sw/lib
    ENV LIBRARY_PATH
    ENV LD_LIBRARY_PATH)
find_library(LIBELF_LIBRARIES
  NAMES
    libelf.a
  PATHS
    /usr/lib
    /usr/local/lib
    /opt/local/lib
    /sw/lib
    ENV LIBRARY_PATH
    ENV LD_LIBRARY_PATH)
find_library(LIBZ_LIBRARIES
  NAMES
    libz.a
  PATHS
    /usr/lib
    /usr/local/lib
    /opt/local/lib
    /sw/lib
    ENV LIBRARY_PATH
    ENV LD_LIBRARY_PATH)
else()
find_library(LIBBPF_LIBRARIES
  NAMES
    libbpf.so
  PATHS
    /usr/lib
    /usr/local/lib
    /opt/local/lib
    /sw/lib
    ENV LIBRARY_PATH
    ENV LD_LIBRARY_PATH)
find_library(LIBELF_LIBRARIES
  NAMES
  libelf.so
  PATHS
    /usr/lib
    /usr/local/lib
    /opt/local/lib
    /sw/lib
    ENV LIBRARY_PATH
    ENV LD_LIBRARY_PATH)
find_library(LIBZ_LIBRARIES
  NAMES
    libz.so
  PATHS
    /usr/lib
    /usr/local/lib
    /opt/local/lib
    /sw/lib
    ENV LIBRARY_PATH
    ENV LD_LIBRARY_PATH)
endif()

include (FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibBpf DEFAULT_MSG
  LIBBPF_LIBRARIES
  LIBELF_LIBRARIES
  LIBZ_LIBRARIES
  LIBBPF_INCLUDE_DIRS)

list(APPEND LIBBPF_LIBRARIES ${LIBELF_LIBRARIES})
list(APPEND LIBBPF_LIBRARIES ${LIBZ_LIBRARIES})

if(LibBpf_FOUND)
    add_library(libbpf INTERFACE)
    target_link_libraries(libbpf INTERFACE ${LIBBPF_LIBRARIES})
    target_include_directories(libbpf INTERFACE ${LIBBPF_INCLUDE_DIRS})
    add_library(LibBpf::LibBpf ALIAS libbpf)
endif()
mark_as_advanced(LIBBPF_INCLUDE_DIRS LIBBPF_LIBRARIES)
