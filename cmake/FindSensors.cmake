# SPDX-FileCopyrightText: 2022 (c) Technische Universität Dresden
#
# SPDX-License-Identifier: GPL-3.0-or-later

include(${CMAKE_CURRENT_LIST_DIR}/UnsetIfUpdated.cmake)

# Linking libsensors isn't a great default because it produces warnings
option(Sensors_USE_STATIC_LIBS "Link libsensors statically." OFF)

UnsetIfUpdated(Sensors_LIBRARIES Sensors_USE_STATIC_LIBS)

find_path(Sensors_INCLUDE_DIRS sensors/sensors.h
        PATHS ENV C_INCLUDE_PATH ENV CPATH
        PATH_SUFFIXES include)

if(Sensors_USE_STATIC_LIBS)
    find_library(Sensors_LIBRARIES NAMES libsensors.a
            HINTS ENV LIBRARY_PATH)
else()
    find_library(Sensors_LIBRARIES NAMES libsensors.so
            HINTS ENV LIBRARY_PATH LD_LIBRARY_PATH)
endif()

include (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Sensors DEFAULT_MSG
        Sensors_LIBRARIES
        Sensors_INCLUDE_DIRS)

if(Sensors_FOUND)
    add_library(Sensors::sensors UNKNOWN IMPORTED)
    set_property(TARGET Sensors::sensors PROPERTY IMPORTED_LOCATION ${Sensors_LIBRARIES})
    target_include_directories(Sensors::sensors INTERFACE ${Sensors_INCLUDE_DIRS})
endif()

mark_as_advanced(Sensors_LIBRARIES Sensors_INCLUDE_DIRS)
