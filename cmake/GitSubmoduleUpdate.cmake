# SPDX-FileCopyrightText: 2018 (c) Technische Universität Dresden
#
# SPDX-License-Identifier: GPL-3.0-or-later

# This code helps you initialize and update your git submodules
# As such it should be in your repository, not in a submodule - copy paste it there
# Use with:
#    include(cmake/GitSubmoduleUpdate.cmake)
#    git_submodule_update()

macro(_git_submodule_update_path git_path)
    message(STATUS "Checking git submodules in ${git_path}")
    execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule status --recursive
            WORKING_DIRECTORY "${git_path}"
            OUTPUT_VARIABLE GIT_SUBMODULE_STATE
    )
    string(REPLACE "\n" ";" GIT_SUBMODULE_STATE ${GIT_SUBMODULE_STATE})
    foreach(submodule IN LISTS GIT_SUBMODULE_STATE)
        if(submodule MATCHES "[ ]*[+-][^ ]* ([^ ]*).*")
            message("  Found outdated submodule '${CMAKE_MATCH_1}'")
            set(GIT_SUBMODULE_RUN_UPDATE ON)
        endif()
    endforeach()
    if(GIT_SUBMODULE_RUN_UPDATE)
        message(STATUS "Updating git submodules")
        execute_process(
                COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                WORKING_DIRECTORY "${git_path}"
        )
    endif()
endmacro()

macro(_is_git git_path result_variable)
    execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --is-inside-work-tree
            WORKING_DIRECTORY ${git_path}
            OUTPUT_VARIABLE OUTPUT_DEV_NULL
            ERROR_VARIABLE OUTPUT_DEV_NULL
            RESULT_VARIABLE NOT_IN_GIT
    )
    if (NOT_IN_GIT)
        set(${result_variable} FALSE)
    else()
        set(${result_variable} TRUE)
    endif()
endmacro()

macro(git_submodule_update)
    find_package(Git)
    if (NOT Git_FOUND)
        message(STATUS "No git executable found. Skipping submodule check.")
    else()
        # If a global check in CMAKE_SOURCE_DIR was already performed recursively, we skip all
        # further checks. If we only do a local check in CMAKE_CURRENT_SOURCE_DIR, we don't set
        # this variable and repeat the checks.
        get_property(GIT_SUBMODULE_CHECKED GLOBAL PROPERTY GIT_SUBMODULE_CHECKED)
        if (NOT GIT_SUBMODULE_CHECKED)
            _is_git(${CMAKE_SOURCE_DIR} IN_GIT)
            if (IN_GIT)
                _git_submodule_update_path("${CMAKE_SOURCE_DIR}")
                set_property(GLOBAL PROPERTY GIT_SUBMODULE_CHECKED TRUE)
            else()
                _is_git(${CMAKE_CURRENT_SOURCE_DIR} IN_CURRENT_GIT)
                if (IN_CURRENT_GIT)
                    _git_submodule_update_path("${CMAKE_CURRENT_SOURCE_DIR}")
                else()
                    message(STATUS "No source directory is a git repository. Skipping submodule check.")
                endif()
            endif()
        endif()
    endif()
endmacro()
