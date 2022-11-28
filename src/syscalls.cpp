/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2016,
 *    Technische Universitaet Dresden, Germany
 *
 * lo2s is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * lo2s is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with lo2s.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <lo2s/syscalls.hpp>

#include <string>

#include <fmt/core.h>

#ifdef HAVE_LIBAUDIT
extern "C"
{
#include <libaudit.h>
}
#endif

namespace lo2s
{

std::string syscall_name_for_nr(int64_t syscall_nr)
{
#ifdef HAVE_LIBAUDIT
    int machine;
    if ((machine = audit_detect_machine()) != -1)
    {
        const char* syscall_name;
        if ((syscall_name = audit_syscall_to_name(syscall_nr, machine)))
        {
            return std::string(syscall_name);
        }
    }
#endif
    return fmt::format("syscall {}", syscall_nr);
}

int64_t syscall_nr_for_name(const std::string name)
{
    int syscall_nr;
    try
    {
        syscall_nr = std::stoi(name);
    }
    catch (std::invalid_argument& e)
    {
#ifdef HAVE_LIBAUDIT
        int machine = audit_detect_machine();

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
