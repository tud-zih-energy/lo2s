
# Find libradare
if (Radare_LIBRARIES AND Radare_INCLUDE_DIRS)
  set (Radare_FIND_QUIETLY TRUE)
endif()

option(Radare_USE_STATIC_LIBS "Link radare2 libraries statically." ON)
UnsetIfUpdated(Radare_LIBRARIES Radare_USE_STATIC_LIBS)

if(Radare_USE_STATIC_LIBS)
    find_library(Radare_LIBRARIES NAMES libr_asm.a HINTS ENV LIBRARY_PATH ENV LD_LIBRARY_PATH)
else()
    find_library(Radare_LIBRARIES NAMES libr_asm.so HINTS ENV LIBRARY_PATH ENV LD_LIBRARY_PATH)
endif()

find_path(Radare_INCLUDE_DIRS NAMES r_asm.h HINTS ENV C_INCLUDE_PATH PATH_SUFFIXES libr)

include (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Radare DEFAULT_MSG
  Radare_LIBRARIES
  Radare_INCLUDE_DIRS)

mark_as_advanced(Radare_INCLUDE_DIRS Radare_LIBRARIES)
