option(INSTALL_MANPAGE "Build and install man pages" ON)

if(INSTALL_MANPAGE)
    find_program(POD2MAN_EXECUTABLE pod2man)
    mark_as_advanced(POD2MAN_EXECUTABLE)

    if (POD2MAN_EXECUTABLE)
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
        add_custom_target(man-page ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/lo2s.1)
    else()
        message(WARNING
            "pod2man not found. Man pages will not be available")
    endif()
endif()

include(GNUInstallDirs)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/lo2s.1
    DESTINATION "${CMAKE_INSTALL_MANDIR}/man1"
    OPTIONAL
)
