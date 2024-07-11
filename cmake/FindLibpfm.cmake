
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
