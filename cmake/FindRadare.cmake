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

find_package(PkgConfig)

include(${CMAKE_CURRENT_LIST_DIR}/UnsetIfUpdated.cmake)

option(Radare_USE_STATIC_LIBS "Link Radare statically" OFF)

UnsetIfUpdated(Radare_LIBRARIES Radare_USE_STATIC_LIBS)

if(PkgConfig_FOUND)
    # Breaking API changes in 5.8.0
    pkg_check_modules(Radare IMPORTED_TARGET r_asm>=5.8.0 r_anal>=5.8.0)

    if(Radare_FOUND)
        # pkg-config does not know if Radare_STATIC_LIBRARIES really only contains
        # static libraries, so use try_compile.
        if(Radare_USE_STATIC_LIBS)
            try_compile(BUILD_RADARE
                SOURCES "${CMAKE_CURRENT_LIST_DIR}/test_static_radare.c"
                LINK_LIBRARIES ${Radare_STATIC_LIBRARIES}
                COMPILE_DEFINITIONS "-static")
            if(NOT BUILD_RADARE)
                set(Radare_FOUND OFF CACHE INTERNAL "")
            endif()
        endif()
        if(Radare_USE_STATIC_LIBS)
            add_library(Radare::Radare IMPORTED STATIC)
            set_property(TARGET Radare::Radare PROPERTY IMPORTED_LOCATION ${Radare_STATIC_LIBRARIES})
        else()
            add_library(Radare::Radare IMPORTED SHARED)
            set_property(TARGET Radare::Radare PROPERTY IMPORTED_LOCATION ${Radare_LIBRARIES})
        endif()
        target_include_directories(Radare::Radare INTERFACE ${Radare_INCLUDE_DIRS})
    endif()
endif()
