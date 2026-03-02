# SPDX-FileCopyrightText: 2019 (c) Technische Universität Dresden
#
# SPDX-License-Identifier: GPL-3.0-or-later

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
unset(CMAKE_REQUIRED_LIBRARIES)

set(_CHECK_FILESYSTEM_CODE "
#include <filesystem>

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

if(NOT _HAS_INTEGRATED_STD_FILESYSTEM AND NOT _HAS_BUNDLED_FILESYSTEM_LIBRARY)
    if(HAS_LIBSTDCXX)
        find_library(StdFilesystem_LIBRARY NAMES ${StdFSLibName} HINTS ENV LD_LIBRARY_PATH ENV LIBRARY_PATH ENV DYLD_LIBRARY_PATH)
    elseif(HAS_LIBCXX)
        find_library(StdFilesystem_LIBRARY NAMES ${StdFSLibName} HINTS ENV LD_LIBRARY_PATH ENV LIBRARY_PATH ENV DYLD_LIBRARY_PATH)
    else()
        message(WARNING "Couldn't detect your C++ standard library, but also couldn't link without an additional library. This is bad.")
    endif()

    find_package_handle_standard_args(StdFilesystem
        "Couldn't determine a proper setup for std::filesystem. Please use a fully C++17 compliant compiler and standard library."
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
        "Couldn't determine a proper setup for std::filesystem. Please use a fully C++17 compliant compiler and standard library."
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

    mark_as_advanced(StdFilesystem_LIBRARY)
endif()
