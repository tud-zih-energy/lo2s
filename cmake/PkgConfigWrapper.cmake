# pkg_check_modules with some grievances addressed
# - Checks for the actual existence of all libraries in the specified static or
#   shared variant
# - Creates a ${libname}::${libname} library that is static, if requested
# - checks for the existence of a header files
#
# libname: Name for the library target produces, i.e. ${libname}::${libname}
#
# pkgconfig_lib_name: Name of the package pkg-config should search for
#
# main_library: Name of the "main" library. AFAIK you can not have a library target
# that is just 15 different libraries, you need to specify one that is the "main" one
# in add_library() and add the rest with target_link_libraries.
#
# In reality, I think it does not make any difference what is in add_library() and
# target_link_libraries().
# Example: You are trying to addd libdw, that thing requires -ldw -lelf -lz.
# Then main_library is "dw".
macro(FindPkgConfigWrapper libname pkgconfig_lib_name main_library)
    include(cmake/UnsetIfUpdated.cmake)

    UnsetIfUpdated(${libname}_LIBRARIES ${libname}_USE_STATIC_LIBS)

    if(TARGET ${libname}::${libname})
        return()
    endif()

    find_package(PkgConfig)
    pkg_check_modules("${libname}_PKG_CONFIG"  ${pkgconfig_lib_name})

    # pkg-config has the fatal flaw that it just prints out whatever would
    # be correct to link your application, regardless of whether those libraries
    # are _actually_ installed

    # So for every library you have to link for libdw check using find_library if its
    # _actually_ there. Initialize ${LIBRARY_NAME}_LIBRARY with the full path to the library
    # (dw_LIBRARY, zstd_LIBRARY, ...) and put all the *_LIBRARY's into ${libname}_LIBRARIES
    if(${libname}_PKG_CONFIG_FOUND)
        if(${libname}_USE_STATIC_LIBS)
            foreach(LIBRARY IN LISTS ${libname}_PKG_CONFIG_STATIC_LIBRARIES)
                find_library(${LIBRARY}_LIBRARY NAMES "lib${LIBRARY}.a"
                    HINTS ${${libname}_PKG_CONFIG_STATIC_LIBRARY_DIRS} ENV LIBRARY_PATH)
                list(APPEND ${libname}_LIBRARIES "${LIBRARY}_LIBRARY")
            endforeach()
        else()
            foreach(LIBRARY IN LISTS ${libname}_PKG_CONFIG_LIBRARIES)
                find_library(${LIBRARY}_LIBRARY NAMES "lib${LIBRARY}.so"
                    HINTS ${${libname}_PKG_CONFIG_LIBRARY_DIRS} ENV LIBRARY_PATH)
                list(APPEND ${libname}_LIBRARIES "${LIBRARY}_LIBRARY")
            endforeach()
        endif()
    endif()

    include (FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(${libname} DEFAULT_MSG
        ${libname}_PKG_CONFIG_INCLUDE_DIRS ${${libname}_LIBRARIES})

    if(${libname}_FOUND)
        add_library(${libname}::${libname} UNKNOWN IMPORTED)
        foreach(LIBRARY IN LISTS ${libname}_LIBRARIES)
            # CMake becomes very unhappy if you just target_link_libraries everything
            # so check for libdw and use that for set_property so that there is a
            # single specialest library
            if(${LIBRARY} STREQUAL ${main_library}_LIBRARY)
                set_property(TARGET ${libname}::${libname} PROPERTY IMPORTED_LOCATION ${${main_library}_LIBRARY})
            else()
                target_link_libraries(${libname}::${libname} INTERFACE ${${LIBRARY}})
            endif()
        endforeach()
        target_include_directories(${libname}::${libname} INTERFACE ${${libname}_PKG_CONFIG_INCLUDE_DIRS})
    endif()
    mark_as_advanced(${libname}_LIBRARY ${libname}_PKG_CONFIG_INCLUDE_DIRS)
endmacro()
