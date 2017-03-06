# This is optional, if found, it will give you
# X86Adapt_FOUND
# -DHAVE_X86_ADAPT
# X86_ADAPT_LIBRARIES (or empty if not found)
# May the UnderScore be with you, or not - only MA_Rio knows for sure

SET(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake;${CMAKE_MODULE_PATH}")

find_package(X86Adapt)
if (X86Adapt_FOUND)
    message(STATUS "X86Adapt found.")
    add_definitions(-DHAVE_X86_ADAPT)

    include_directories(${CMAKE_CURRENT_LIST_DIR}/include)
    include_directories(SYSTEM ${X86_ADAPT_INCLUDE_DIRS})
else()
    set(X86_ADAPT_LIBRARIES "")
endif()
