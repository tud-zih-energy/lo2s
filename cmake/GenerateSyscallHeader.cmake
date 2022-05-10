# Copyright (c) 2022, ZIH, Technische Universitaet Dresden, Federal Republic of Germany
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright notice,
#       this list of conditions and the following disclaimer in the documentation
#       and/or other materials provided with the distribution.
#     * Neither the name of metricq nor the names of its contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

function(GEN_SYSCALLS_HEADER _RESULT)
    include(CheckCXXSourceCompiles)
    execute_process(COMMAND sh ${CMAKE_SOURCE_DIR}/cmake/gen_syscalls.sh
                OUTPUT_VARIABLE SYSCALL_LIST)

    if (NOT SYSCALL_LIST STREQUAL "")
        configure_file(src/syscalls.cpp.in src/syscalls.cpp @ONLY)
        set(CMAKE_REQUIRED_INCLUDES ${CMAKE_CURRENT_BINARY_DIR}/include)
        set(_CHECK_SYSCALL_CODE "
        #include <map>
        #include <string>

        int main(void)
        {
            static std::map<uint64_t, std::string> sycalls = { ${SYSCALL_LIST} };
            return 0;
        }
        ")
        check_cxx_source_compiles("${_CHECK_SYSCALL_CODE}" ${_RESULT})
    endif()
endfunction()
