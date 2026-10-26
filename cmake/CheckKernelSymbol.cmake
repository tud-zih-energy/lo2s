# SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
#
# SPDX-License-Identifier: GPL-3.0-or-later

# CHECK_KERNEL_SYMBOL_EXISTS(<symbol_name> <result_variable>)
#
# Checks whether the given symbol is exported by the currently running kernel
# by searching /proc/kallsyms. This is useful for detecting function renames
# between kernel versions that are not reflected in userspace header files.
#
#   <symbol_name>       Name of the kernel symbol to look for
#   <result_variable>   Variable set to TRUE if the symbol was found, FALSE otherwise
function(CHECK_KERNEL_SYMBOL_EXISTS symbol_name result_var)
    execute_process(
        COMMAND grep -cw "${symbol_name}" /proc/kallsyms
        RESULT_VARIABLE _grep_result
        OUTPUT_VARIABLE _grep_output
        ERROR_QUIET
    )
    if(_grep_result EQUAL 0)
        set(${result_var} TRUE PARENT_SCOPE)
    else()
        set(${result_var} FALSE PARENT_SCOPE)
    endif()
endfunction()
