
# Find libradare
if (Radare_LIBRARIES AND Radare_INCLUDE_DIRS)
  set (Radare_FIND_QUIETLY TRUE)
endif()

find_path(Radare_INCLUDE_DIRS NAMES r_asm.h PATH_SUFFIXES libr)
find_library(Radare_LIBRARIES NAMES r_asm PATH_SUFFIXES lib64 lib)

include (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Radare DEFAULT_MSG
  Radare_LIBRARIES
  Radare_INCLUDE_DIRS)

mark_as_advanced(Radare_INCLUDE_DIRS Radare_LIBRARIES)
