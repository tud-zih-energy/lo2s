
# Find libradare
if (Radare_LIBRARIES AND Radare_INCLUDE_DIRS)
  set (Radare_FIND_QUIETLY TRUE)
endif()

option(Radare_USE_STATIC_LIBS "Link radare2 libraries statically." ON)
UnsetIfUpdated(Radare_LIBRARIES Radare_USE_STATIC_LIBS)

if(Radare_USE_STATIC_LIBS)
    set(Radare_LIBRARIES "")
    find_library(Radare_LIBRARIES_asm NAMES libr_asm.a HINTS ENV LIBRARY_PATH ENV LD_LIBRARY_PATH)
    find_library(Radare_LIBRARIES_syscall NAMES libr_syscall.a HINTS ENV LIBRARY_PATH ENV LD_LIBRARY_PATH)
    find_library(Radare_LIBRARIES_util NAMES libr_util.a HINTS ENV LIBRARY_PATH ENV LD_LIBRARY_PATH)
    find_library(Radare_LIBRARIES_parse NAMES libr_parse.a HINTS ENV LIBRARY_PATH ENV LD_LIBRARY_PATH)
    find_library(Radare_LIBRARIES_flags NAMES libr_flags.a HINTS ENV LIBRARY_PATH ENV LD_LIBRARY_PATH)
    find_library(Radare_LIBRARIES_reg NAMES libr_reg.a HINTS ENV LIBRARY_PATH ENV LD_LIBRARY_PATH)
    list(APPEND Radare_LIBRARIES ${Radare_LIBRARIES_asm} ${Radare_LIBRARIES_syscall} ${Radare_LIBRARIES_parse} ${Radare_LIBRARIES_flags} ${Radare_LIBRARIES_reg} ${Radare_LIBRARIES_util})
else()
    find_library(Radare_LIBRARIES NAMES libr_asm.so HINTS ENV LIBRARY_PATH ENV LD_LIBRARY_PATH)
endif()

find_path(Radare_INCLUDE_DIRS NAMES r_asm.h HINTS ENV C_INCLUDE_PATH ENV CPATH PATH_SUFFIXES libr)

include (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Radare DEFAULT_MSG
  Radare_LIBRARIES
  Radare_INCLUDE_DIRS)

# define CMake target object
add_library(radare INTERFACE)
target_link_libraries(radare INTERFACE ${Radare_LIBRARIES})
target_include_directories(radare SYSTEM INTERFACE ${Radare_INCLUDE_DIRS})
add_library(Radare::Radare ALIAS radare)

mark_as_advanced(Radare_INCLUDE_DIRS Radare_LIBRARIES)
