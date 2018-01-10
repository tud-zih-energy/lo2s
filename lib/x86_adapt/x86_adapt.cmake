# This is optional, if found, it will give you
# X86Adapt_FOUND
# -DHAVE_X86_ADAPT
# X86_ADAPT_LIBRARIES (or empty if not found)
# May the UnderScore be with you, or not - only MA_Rio knows for sure

SET(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake;${CMAKE_MODULE_PATH}")

find_package(X86Adapt)
if (X86Adapt_FOUND)
    message(STATUS "X86Adapt found.")

    add_library(x86_adapt_cxx INTERFACE)
    target_link_libraries(x86_adapt_cxx
        INTERFACE
            ${X86_ADAPT_LIBRARIES}
    )

    target_include_directories(x86_adapt_cxx SYSTEM
        INTERFACE
            ${X86_ADAPT_INCLUDE_DIRS}
    )

    target_include_directories(x86_adapt_cxx
        INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}/include
    )

    target_compile_definitions(x86_adapt_cxx
        INTERFACE
            HAVE_X86_ADAPT
    )

    add_library(x86_adapt::x86_adapt ALIAS x86_adapt_cxx)
endif()
