# desired installation behavior
# option  | pod2man NOT found | pod2man found
# --------|-------------------|----------------
# OFF     | no man page       | no man page
# default | no man page       | build man page
# ON      | error             | build man page
#
# This is realised through a "smart" default for the INSTALL_MANPAGE cmake option.

# step 1: investigate if man page can even be built
find_program(POD2MAN_EXECUTABLE pod2man)
mark_as_advanced(POD2MAN_EXECUTABLE)

# *Technically* gzip is not required for man pages,
# but (1) exists on most systems,
# and (2) all other man pages are gzipped.
find_program(GZIP_EXECUTABLE gzip)
mark_as_advanced(GZIP_EXECUTABLE)

if (POD2MAN_EXECUTABLE AND GZIP_EXECUTABLE)
    set(INSTALL_MANPAGE_DEFAULT ON)
else()
    set(INSTALL_MANPAGE_DEFAULT OFF)
endif()

# step 2: set default option accordingly (idea: don't get in the way of user)
option(INSTALL_MANPAGE "Build and install man pages" ${INSTALL_MANPAGE_DEFAULT})
add_feature_info(INSTALL_MANPAGE INSTALL_MANPAGE "Build and install man pages.")

# step 3: if requested (either through default or explicitly), build man page
if(INSTALL_MANPAGE)
    if (NOT POD2MAN_EXECUTABLE)
        message(FATAL_ERROR "pod2man not found: can not install man page")
    endif()
    if (NOT GZIP_EXECUTABLE)
        message(FATAL_ERROR "gzip not found: can not install man page")
    endif()

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lo2s.1
        COMMAND ${POD2MAN_EXECUTABLE}
        ARGS --name="lo2s"
                --center="lo2s User Manual"
                --release="${LO2S_VERSION_STRING}"
                --utf8
                lo2s.1.pod
                ${CMAKE_CURRENT_BINARY_DIR}/lo2s.1
        MAIN_DEPENDENCY lo2s.1.pod
        COMMENT "Generating man pages"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lo2s.1.gz
        COMMAND ${GZIP_EXECUTABLE}
        ARGS -9 --force --keep ${CMAKE_CURRENT_BINARY_DIR}/lo2s.1
        MAIN_DEPENDENCY ${CMAKE_CURRENT_BINARY_DIR}/lo2s.1
        COMMENT "Compressing man pages"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    add_custom_target(man-page ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/lo2s.1.gz)

    include(GNUInstallDirs)
    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/lo2s.1.gz
        DESTINATION "${CMAKE_INSTALL_MANDIR}/man1"
    )
endif()
