# SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
#
# SPDX-License-Identifier: GPL-3.0-or-later

include(${CMAKE_CURRENT_LIST_DIR}/UnsetIfUpdated.cmake)

option(X86Adapt_STATIC "Link x86_adapt library static." ON)

UnsetIfUpdated(X86_ADAPT_LIBRARIES X86Adapt_STATIC)

if(X86Adapt_STATIC)
    set(LIBX86A_NAME libx86_adapt_static.a)
else()
    set(LIBX86A_NAME libx86_adapt.so)
endif()

if (X86_ADAPT_LIBRARIES AND X86_ADAPT_INCLUDE_DIRS)
    set (X86Adapt_FIND_QUIETLY TRUE)
endif (X86_ADAPT_LIBRARIES AND X86_ADAPT_INCLUDE_DIRS)

find_path(X86_ADAPT_INCLUDE_DIRS x86_adapt.h HINTS ${X86_ADAPT_DIR} PATHS ENV C_INCLUDE_PATH PATH_SUFFIXES include)
find_library(X86_ADAPT_LIBRARIES NAMES ${LIBX86A_NAME} PATHS ENV LIBRARY_PATH LD_LIBRARY_PATH)

include (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(X86Adapt DEFAULT_MSG
        X86_ADAPT_LIBRARIES
        X86_ADAPT_INCLUDE_DIRS)

mark_as_advanced(X86_ADAPT_INCLUDE_DIRS X86_ADAPT_LIBRARIES)

unset(LIBX86A_NAME)
