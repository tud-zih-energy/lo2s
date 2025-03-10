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

# Linking libelf isn't a great default because it produces warnings
option(LibElf_USE_STATIC_LIBS "Link libelf statically." OFF)

UnsetIfUpdated(LibElf_LIBRARIES LibElf_USE_STATIC_LIBS)

find_path(LibElf_INCLUDE_DIRS libelf.h
    PATHS ENV C_INCLUDE_PATH ENV CPATH
    PATH_SUFFIXES include)

find_path(LibDw_INCLUDE_DIRS elfutils/libdw.h
    PATHS ENV C_INCLUDE_PATH ENV CPATH
    PATH_SUFFIXES include)

if(LibElf_USE_STATIC_LIBS)
    find_library(LibElf_LIBRARY NAMES libelf.a
            HINTS ENV LIBRARY_PATH)
    find_library(LibDw_LIBRARY NAMES libdw.a
            HINTS ENV LIBRARY_PATH)
    set(LibElf_LIBRARIES ${LibElf_LIBRARY} ${LibDw_LIBRARY})
else()
    find_library(LibElf_LIBRARY NAMES libelf.so
            HINTS ENV LIBRARY_PATH LD_LIBRARY_PATH)
    find_library(LibDw_LIBRARY NAMES libdw.so
            HINTS ENV LIBRARY_PATH LD_LIBRARY_PATH)
    set(LibElf_LIBRARIES ${LibElf_LIBRARY} ${LibDw_LIBRARY})
endif()

include (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibElf DEFAULT_MSG
        LibElf_LIBRARIES
        LibElf_INCLUDE_DIRS)

if(LibElf_FOUND)
    add_library(LibElf::LibElf UNKNOWN IMPORTED)
    set_property(TARGET LibElf::LibElf PROPERTY IMPORTED_LOCATION ${LibElf_LIBRARY})
    target_include_directories(LibElf::LibElf INTERFACE ${LibElf_INCLUDE_DIRS})
    add_library(LibElf::LibDw UNKNOWN IMPORTED)
    set_property(TARGET LibElf::LibDw PROPERTY IMPORTED_LOCATION ${LibDw_LIBRARY})
    target_include_directories(LibElf::LibDw INTERFACE ${LibDw_INCLUDE_DIRS})
endif()

mark_as_advanced(LibElf_LIBRARIES LibElf_INCLUDE_DIRS)
