
find_path(Veosinfo_INCLUDE_DIRS veosinfo/veosinfo.h PATHS ENV C_INCLUDE_PATH ENV CPATH PATH_SUFFIXES include)

find_library(Veosinfo_LIBRARIES veosinfo HINT ENV LIBRARY_PATH ENV LD_LIBRARY_PATH)


include (FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(Veosinfo DEFAULT_MSG Veosinfo_LIBRARIES Veosinfo_INCLUDE_DIRS)

if(Veosinfo_FOUND)
    add_library(libveosinfo INTERFACE)
    target_link_libraries(libveosinfo INTERFACE ${Veosinfo_LIBRARIES})
    target_include_directories(libveosinfo SYSTEM INTERFACE ${Veosinfo_INCLUDE_DIRS})
    add_library(Veosinfo::veosinfo ALIAS libveosinfo)
endif()

mark_as_advanced(Veosinfo_LIBRARIES Veosinfo_INCLUDE_DIRS)
