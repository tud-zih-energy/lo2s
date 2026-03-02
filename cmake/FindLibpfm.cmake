# SPDX-FileCopyrightText: 2022 (c) Technische Universität Dresden
#
# SPDX-License-Identifier: GPL-3.0-or-later

include(${CMAKE_CURRENT_LIST_DIR}/UnsetIfUpdated.cmake)

option(Libpfm_USE_STATIC_LIBS "Link libpfm statically" OFF)

UnsetIfUpdated(Libpfm_LIBRARIES, Libpfm_USE_STATIC_LIBS)

find_path(Libpfm_INCLUDE_DIRS perfmon/pfmlib.h PATHS ENV C_INCLUDE_PATH ENV CPATH PATH_SUFFIXES include)

if(Libpfm_USE_STATIC_LIBS)
  find_library(Libpfm_LIBRARIES NAME libpfm.a HINTS ENV LIBRARY_PATH LD_LIBRARY_PATH)
  else()
  find_library(Libpfm_LIBRARIES NAME libpfm.so HINTS ENV LIBRARY_PATH LD_LIBRARY_PATH)
endif()

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(Libpfm DEFAULT_MSG Libpfm_LIBRARIES Libpfm_INCLUDE_DIRS)

if(Libpfm_FOUND)
  add_library(libpfm INTERFACE)
  target_include_directories(libpfm SYSTEM INTERFACE ${Libpfm_INCLUDE_DIRS})
  target_link_libraries(libpfm INTERFACE ${Libpfm_LIBRARIES})
  add_library(Libpfm::libpfm ALIAS libpfm)
 endif()

 mark_as_advanced(Libpfm_LIBRARIES Libpfm_INCLUDE_DIRS)
