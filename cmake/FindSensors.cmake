# Copyright (c) 2022, Technische Universität Dresden, Germany
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are permitted
# provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of conditions
#    and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
#    and the following disclaimer in the documentation and/or other materials provided with the
#    distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse
#    or promote products derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
# FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
    add_library(libsensors INTERFACE)
    target_include_directories(libsensors SYSTEM INTERFACE ${Sensors_INCLUDE_DIRS})
    target_link_libraries(binutils INTERFACE ${Sensors_LIBRARIES})
    add_library(Sensors::sensors ALIAS libsensors)
endif()

mark_as_advanced(Sensors_LIBRARIES Sensors_INCLUDE_DIRS)
