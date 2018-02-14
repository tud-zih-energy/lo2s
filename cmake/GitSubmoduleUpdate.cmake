# Copyright (c) 2018, Technische Universität Dresden, Germany
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are permitted
# provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of conditions
#    and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions
#    and the following disclaimer in the documentation and/or other materials provided with the
#    distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse
#    or promote products derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
# FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

macro(git_submodule_update)
    get_property(GIT_SUBMODULE_CHECKED GLOBAL PROPERTY GIT_SUBMODULE_CHECKED)
    if (NOT GIT_SUBMODULE_CHECKED)
        message(STATUS "Checking git submodules")
        execute_process(COMMAND git submodule status --recursive WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE GIT_SUBMODULE_STATE)
        string(REPLACE "\n" ";" GIT_SUBMODULE_STATE ${GIT_SUBMODULE_STATE})
        foreach(submodule IN LISTS GIT_SUBMODULE_STATE)
            if(submodule MATCHES "[ ]*[+-][^ ]* ([^ ]*).*")
                message("  Found outdated submodule '${CMAKE_MATCH_1}'")
                set(GIT_SUBMODULE_RUN_UPDATE ON)
            endif()
        endforeach()
        if(GIT_SUBMODULE_RUN_UPDATE)
            message(STATUS "Updating git submodules")
            execute_process(COMMAND git submodule update --init --recursive WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        endif()
        set_property(GLOBAL PROPERTY GIT_SUBMODULE_CHECKED TRUE)
    endif()
endmacro()
