# Copyright (c) 2017, Technische Universität Dresden, Germany
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

option(Binutils_USE_STATIC_LIBS "Link binutils libraries statically." ON)

UnsetIfUpdated(Binutils_LIBRARIES Binutils_USE_STATIC_LIBS)

find_path(Binutils_INCLUDE_DIRS bfd.h
        PATHS ENV C_INCLUDE_PATH
        PATH_SUFFIXES include)

if(Binutils_USE_STATIC_LIBS)
    find_library(Bfd_LIBRARIES NAMES libbfd.a
            HINTS ENV LIBRARY_PATH)
    find_library(Z_LIBRARIES NAMES libz.a
            HINTS ENV LIBRARY_PATH)
else()
    find_library(Bfd_LIBRARIES NAMES libbfd.so
            HINTS ENV LIBRARY_PATH ENV LD_LIBRARY_PATH)
    find_library(Z_LIBRARIES NAMES libz.so
            HINTS ENV LIBRARY_PATH ENV LD_LIBRARY_PATH)
endif()

# libiberty is always linked statically
find_library(Libiberty_LIBRARIES NAMES libiberty.a
        HINTS ENV LIBRARY_PATH)

include (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Binutils DEFAULT_MSG
        Bfd_LIBRARIES
        Z_LIBRARIES
        Libiberty_LIBRARIES
        Binutils_INCLUDE_DIRS)

set(Binutils_LIBRARIES "")
list(APPEND Binutils_LIBRARIES ${Bfd_LIBRARIES})
# BFD requires libz...
list(APPEND Binutils_LIBRARIES ${Z_LIBRARIES})
list(APPEND Binutils_LIBRARIES ${Libiberty_LIBRARIES})

mark_as_advanced(Binutils_LIBRARIES Binutils_INCLUDE_DIRS)
