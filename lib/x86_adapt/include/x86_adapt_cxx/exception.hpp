// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdexcept>
#include <string>
#include <system_error>

namespace x86_adapt
{

class x86_adapt_error : public std::system_error
{
public:
    x86_adapt_error(int error_code)
    : std::system_error(error_code, std::generic_category(), "x86_adapt")
    {
    }
};

inline void raise(int error)
{
    throw x86_adapt_error(error < 0 ? -error : error);
}

inline int check(int error)
{
    if (error < 0)
    {
        raise(error);
    }

    return error;
}
} // namespace x86_adapt
