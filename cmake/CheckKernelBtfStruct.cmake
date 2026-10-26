# SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
#
# SPDX-License-Identifier: GPL-3.0-or-later

# CHECK_KERNEL_BTF_HAS_STRUCT(<struct_name> <result_variable> [<btf_tool>])
#
# Checks whether the currently running kernel exports a struct type with the
# given name in its BTF (read from /sys/kernel/btf/vmlinux). This is useful for
# detecting kernel struct layout changes between versions that are not
# reflected in userspace header files.
#
#   <struct_name>       Name of the struct to look for (e.g. __filename_head)
#   <result_variable>   Variable set to TRUE if the struct was found, FALSE otherwise
#   <btf_tool>          Optional path to bpftool (defaults to "bpftool" from PATH)
function(CHECK_KERNEL_BTF_HAS_STRUCT struct_name result_var)
    set(_btf_tool "bpftool")
    if(ARGC GREATER 2)
        set(_btf_tool ${ARGV2})
    endif()
    execute_process(
        COMMAND bash -c "${_btf_tool} btf dump file /sys/kernel/btf/vmlinux 2>/dev/null | grep -q \"STRUCT '${struct_name}'\""
        RESULT_VARIABLE _btf_grep_result
    )
    if(_btf_grep_result EQUAL 0)
        set(${result_var} TRUE PARENT_SCOPE)
    else()
        set(${result_var} FALSE PARENT_SCOPE)
    endif()
endfunction()
