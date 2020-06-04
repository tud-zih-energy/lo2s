# Copyright (c) 2019, ZIH, Technische Universitaet Dresden, Federal Republic of Germany
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright notice,
#       this list of conditions and the following disclaimer in the documentation
#       and/or other materials provided with the distribution.
#     * Neither the name of metricq nor the names of its contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

include(FindPackageHandleStandardArgs)
include(CheckCXXSymbolExists)
include(CheckCXXSourceCompiles)

if (StdFilesystem_LIBRARY)
    set(StdFilesystem_FIND_QUIETLY TRUE)
endif()

# detect the stdlib we are using
CHECK_CXX_SYMBOL_EXISTS(_LIBCPP_VERSION ciso646 HAS_LIBCXX)
CHECK_CXX_SYMBOL_EXISTS(__GLIBCXX__ ciso646 HAS_LIBSTDCXX)

if(HAS_LIBSTDCXX)
    set(StdFSLibName "stdc++fs")
elseif(HAS_LIBCXX)
    set(StdFSLibName "c++fs")
endif()

# detect if the headers are present
set(CMAKE_REQUIRED_FLAGS "-std=c++17")
CHECK_CXX_SYMBOL_EXISTS(std::filesystem::status_known filesystem HAS_STD_FILESYSTEM)
CHECK_CXX_SYMBOL_EXISTS(std::experimental::filesystem::status_known experimental/filesystem HAS_EXPERIMENTAL_STD_FILESYSTEM)
unset(CMAKE_REQUIRED_LIBRARIES)

# source code for further checks
if(HAS_EXPERIMENTAL_STD_FILESYSTEM)
    set(_CHECK_FILESYSTEM_CODE_PREFIX "
#include <experimental/filesystem>
namespace std
{
using namespace experimental;
}")
else()
    set(_CHECK_FILESYSTEM_CODE_PREFIX "
#include <filesystem>
")
endif()
set(_CHECK_FILESYSTEM_CODE "
${_CHECK_FILESYSTEM_CODE_PREFIX}
int main(void)
{
    return std::filesystem::exists(std::filesystem::path(\"\"));
}
")

# check if we need to link an additional library
check_cxx_source_compiles("${_CHECK_FILESYSTEM_CODE}" _HAS_INTEGRATED_STD_FILESYSTEM)

# check if the compiler can find the filesystem library of its stdlib by itself
if(NOT _HAS_INTEGRATED_STD_FILESYSTEM)
    set(CMAKE_REQUIRED_LIBRARIES ${StdFSLibName})
    check_cxx_source_compiles("${_CHECK_FILESYSTEM_CODE}" _HAS_BUNDLED_FILESYSTEM_LIBRARY)
    unset(CMAKE_REQUIRED_LIBRARIES)
endif()
unset(CMAKE_REQUIRED_FLAGS)

# if we only have it in experimental, we still have it
if(HAS_EXPERIMENTAL_STD_FILESYSTEM)
    set(HAS_STD_FILESYSTEM TRUE)
endif()

if(NOT _HAS_INTEGRATED_STD_FILESYSTEM AND NOT _HAS_BUNDLED_FILESYSTEM_LIBRARY)
    if(HAS_LIBSTDCXX)
        find_library(StdFilesystem_LIBRARY NAMES ${StdFSLibName} HINTS ENV LD_LIBRARY_PATH ENV LIBRARY_PATH ENV DYLD_LIBRARY_PATH)
    elseif(HAS_LIBCXX)
        find_library(StdFilesystem_LIBRARY NAMES ${StdFSLibName} HINTS ENV LD_LIBRARY_PATH ENV LIBRARY_PATH ENV DYLD_LIBRARY_PATH)
    else()
        message(WARNING "Couldn't detect your C++ standard library, but also couldn't link without an additional library. This is bad.")
    endif()

    find_package_handle_standard_args(StdFilesystem
        "Coudln't determine a proper setup for std::filesystem. Please use a fully C++17 compliant compiler and standard library."
        StdFilesystem_LIBRARY
        HAS_STD_FILESYSTEM
    )
else()
    if(_HAS_INTEGRATED_STD_FILESYSTEM)
        set(OUTPUT_MESSAGE "Compiler integrated")
    else()
        set(OUTPUT_MESSAGE "Using -l${StdFSLibName}")
    endif()
    find_package_handle_standard_args(StdFilesystem
        "Coudln't determine a proper setup for std::filesystem. Please use a fully C++17 compliant compiler and standard library."
        OUTPUT_MESSAGE
        HAS_STD_FILESYSTEM
    )
endif()

# if we have found it, let's define the std::filesystem target
if(StdFilesystem_FOUND)
    add_library(std::filesystem INTERFACE IMPORTED)
    target_compile_features(std::filesystem INTERFACE cxx_std_17)

    if(NOT _HAS_INTEGRATED_STD_FILESYSTEM AND NOT _HAS_BUNDLED_FILESYSTEM_LIBRARY)
        target_link_libraries(std::filesystem INTERFACE ${StdFilesystem_LIBRARY})
    elseif(_HAS_BUNDLED_FILESYSTEM_LIBRARY)
        target_link_libraries(std::filesystem INTERFACE ${StdFSLibName})
        set(StdFilesystem_LIBRARY ${StdFSLibName})
    endif()

    if(HAS_EXPERIMENTAL_STD_FILESYSTEM)
        set_target_properties(std::filesystem PROPERTIES INTERFACE_COMPILE_DEFINITIONS HAS_EXPERIMENTAL_STD_FILESYSTEM)
    else()
        set_target_properties(std::filesystem PROPERTIES INTERFACE_COMPILE_DEFINITIONS HAS_STD_FILESYSTEM)
    endif()

    mark_as_advanced(StdFilesystem_LIBRARY)
endif()
