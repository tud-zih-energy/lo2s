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

# Linking libdw statically isn't a great default because it produces warnings
option(LibDw_USE_STATIC_LIBS "Link libelf statically." OFF)

UnsetIfUpdated(LibDw_LIBRARIES LibDw_USE_STATIC_LIBS)

find_package(PkgConfig)
pkg_check_modules(LIBDW_PKG_CONFIG REQUIRED libdw)

# pkg-config has the fatal flaw that it just prints out whatever would
# be correct to link your application, regardless of whether those libraries
# are _actually_ installed

# So for every library you have to link for libdw check using find_library if its
# _actually_ there. Initialize ${LIBRARY_NAME}_LIBRARY with the full path to the library
# (dw_LIBRARY, zstd_LIBRARY, ...) and put all the *_LIBRARY's into LIBDW_LIBRARIES
if(LIBDW_PKG_CONFIG_FOUND)
    if(LibDw_USE_STATIC_LIBS)
        foreach(LIBRARY IN LISTS LIBDW_PKG_CONFIG_STATIC_LIBRARIES)
            find_library(${LIBRARY}_LIBRARY NAMES "lib${LIBRARY}.a"
                HINTS ENV LIBRARY_PATH)
            list(APPEND LIBDW_LIBRARIES "${LIBRARY}_LIBRARY")
        endforeach()
    else()
        foreach(LIBRARY IN LISTS LIBDW_PKG_CONFIG_LIBRARIES)
            find_library(${LIBRARY}_LIBRARY NAMES "lib${LIBRARY}.so"
                HINTS ENV LIBRARY_PATH)
            list(APPEND LIBDW_LIBRARIES "${LIBRARY}_LIBRARY")
        endforeach()
    endif()
endif()


find_path(LibDw_INCLUDE_DIRS elfutils/libdw.h
    PATHS ENV C_INCLUDE_PATH ENV CPATH
    PATH_SUFFIXES include)


include (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibDw DEFAULT_MSG
        LibDw_INCLUDE_DIRS ${LIBDW_LIBRARIES})

if(LibDw_FOUND)
    add_library(LibDw::LibDw UNKNOWN IMPORTED)
    foreach(LIBRARY IN LISTS LIBDW_LIBRARIES)
        # CMake becomes very unhappy if you just target_link_libraries everything
        # so check for libdw and use that for set_property so that there is a
        # single specialest library
        if(${LIBRARY} STREQUAL dw_LIBRARY)
            set_property(TARGET LibDw::LibDw PROPERTY IMPORTED_LOCATION ${dw_LIBRARY})
        else()
            target_link_libraries(LibDw::LibDw INTERFACE ${${LIBRARY}})
        endif()
    endforeach()
    target_include_directories(LibDw::LibDw INTERFACE ${LibDw_INCLUDE_DIRS})
endif()

mark_as_advanced(LibDw_LIBRARY LibDw_INCLUDE_DIRS)
