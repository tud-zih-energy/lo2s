// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/syscalls.hpp>

#include <stdexcept>
#include <string>

#include <fmt/core.h>
#include <fmt/format.h>

#ifdef HAVE_LIBAUDIT
extern "C"
{
#include <libaudit.h>
}
#endif

namespace lo2s
{

std::string syscall_name_for_nr(int syscall_nr)
{
#ifdef HAVE_LIBAUDIT
    const int machine = audit_detect_machine();
    if (machine != -1)
    {
        const char* syscall_name = audit_syscall_to_name(syscall_nr, machine);
        if (syscall_name != nullptr)
        {
            return syscall_name;
        }
    }
#endif
    return fmt::format("syscall {}", syscall_nr);
}

int syscall_nr_for_name(const std::string& name)
{
    int syscall_nr = -1;
    try
    {
        syscall_nr = std::stoi(name);
    }
    catch (std::invalid_argument& e)
    {
#ifdef HAVE_LIBAUDIT
        const int machine = audit_detect_machine();

        if (machine == -1)
        {
            return -1;
        }

        syscall_nr = audit_name_to_syscall(name.c_str(), machine);
#else
        syscall_nr = -1;
#endif
    }
    return syscall_nr;
}

} // namespace lo2s
