option(INSTALL_MANPAGE "Build and install man pages" ON)

set(_MANPAGE_GEN "${CMAKE_CURRENT_BINARY_DIR}/lo2s.1")

if(INSTALL_MANPAGE)
    find_program(POD2MAN_EXECUTABLE pod2man)
    mark_as_advanced(POD2MAN_EXECUTABLE)

    if (POD2MAN_EXECUTABLE)
        add_custom_target(man-page ALL
            COMMAND ${POD2MAN_EXECUTABLE}
                        --name="lo2s"
                        --center="lo2s User Manual"
                        --release="${LO2S_VERSION_STRING}"
                        --utf8
                        "lo2s.1.pod"
                        "${_MANPAGE_GEN}"
            DEPENDS "lo2s.1.pod"
            )
    else()
        message(WARNING
            "pod2man not found. Man pages will not be built or installed")
    endif()
endif()

include(GNUInstallDirs)
install(
    FILES "${_MANPAGE_GEN}"
    DESTINATION "${CMAKE_INSTALL_MANDIR}/man1"
    OPTIONAL
    )

unset(_MANPAGE_GEN)
