// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/types/process.hpp>

#include <lo2s/types/thread.hpp>

#include <iostream>

#include <fmt/format.h>

namespace lo2s
{


Thread Process::as_thread() const
{
    return Thread(pid_);
}

std::ostream& operator<<(std::ostream& stream, const Process& process)
{
    return stream << fmt::format("{}", process);
}
} // namespace lo2s
