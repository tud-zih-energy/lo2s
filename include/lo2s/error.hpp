// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <system_error>

#include <cerrno>

namespace lo2s
{
inline std::system_error make_system_error()
{
    return { errno, std::system_category() };
}

[[noreturn]] inline void throw_errno()
{
    throw make_system_error();
}

inline void check_errno(long retval)
{
    if (retval == -1)
    {
        throw_errno();
    }
}
} // namespace lo2s
