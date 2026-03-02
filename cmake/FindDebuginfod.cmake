# SPDX-FileCopyrightText: 2022 (c) Technische Universität Dresden
#
# SPDX-License-Identifier: GPL-3.0-or-later

include(${CMAKE_CURRENT_LIST_DIR}/UnsetIfUpdated.cmake)

# Linking libelf isn't a great default because it produces warnings
option(Debuginfod_USE_STATIC_LIBS "Link debuginfod statically." OFF)

UnsetIfUpdated(Debuginfod_LIBRARY Debuginfod_USE_STATIC_LIBS)

find_path(Debuginfod_INCLUDE_DIRS libelf.h
    PATHS ENV C_INCLUDE_PATH ENV CPATH
    PATH_SUFFIXES include)

if(Debuginfod_USE_STATIC_LIBS)
    find_library(Debuginfod_LIBRARY NAMES libdebuginfod.a
            HINTS ENV LIBRARY_PATH)
else()
    find_library(Debuginfod_LIBRARY NAMES libdebuginfod.so
            HINTS ENV LIBRARY_PATH LD_LIBRARY_PATH)
endif()

include (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Debuginfod DEFAULT_MSG
        Debuginfod_LIBRARY
        Debuginfod_INCLUDE_DIRS)

if(Debuginfod_FOUND)
    add_library(Debuginfod::Debuginfod UNKNOWN IMPORTED)
    set_property(TARGET Debuginfod::Debuginfod PROPERTY IMPORTED_LOCATION ${Debuginfod_LIBRARY})
    target_include_directories(Debuginfod::Debuginfod INTERFACE ${Debuginfod_INCLUDE_DIRS})
endif()

mark_as_advanced(Debuginfod_LIBRARY Debuginfod_INCLUDE_DIRS)
