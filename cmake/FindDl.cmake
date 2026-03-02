# SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
#
# SPDX-License-Identifier: GPL-3.0-or-later

include(${CMAKE_CURRENT_LIST_DIR}/UnsetIfUpdated.cmake)

# Linking libdl isn't a great default because it produces warnings
option(Dl_USE_STATIC_LIBS "Link libdl statically." OFF)

UnsetIfUpdated(Dl_LIBRARIES Dl_USE_STATIC_LIBS)

find_path(Dl_INCLUDE_DIRS dlfcn.h
        PATHS ENV C_INCLUDE_PATH ENV CPATH
        PATH_SUFFIXES include)

if(Dl_USE_STATIC_LIBS)
    find_library(Dl_LIBRARIES NAMES libdl.a
            HINTS ENV LIBRARY_PATH)
else()
    find_library(Dl_LIBRARIES NAMES libdl.so
            HINTS ENV LIBRARY_PATH LD_LIBRARY_PATH)
endif()

include (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Dl DEFAULT_MSG
        Dl_LIBRARIES
        Dl_INCLUDE_DIRS)

mark_as_advanced(Dl_LIBRARIES Dl_INCLUDE_DIRS)
