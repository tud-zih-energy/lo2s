/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2026,
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

#pragma once

#include <fmt/base.h>
#pragma once

#include <iostream>

#include <cstdint>

#include <fmt/format.h>

extern "C"
{
#include <unistd.h>
}

namespace lo2s
{

class ExecutionScope;
class Thread;

class Process
{
public:
    explicit Process(int64_t pid) : pid_(pid)
    {
    }

    explicit Process() : pid_(-1)
    {
    }

    static Process no_parent()
    {
        return Process(0);
    }

    friend bool operator==(const Process& lhs, const Process& rhs)
    {
        return lhs.pid_ == rhs.pid_;
    }

    friend bool operator!=(const Process& lhs, const Process& rhs)
    {
        return !(lhs == rhs);
    }

    friend bool operator<(const Process& lhs, const Process& rhs)
    {
        return lhs.pid_ < rhs.pid_;
    }

    friend bool operator!(const Process& process)
    {
        return process.pid_ == -1;
    }

    static Process invalid()
    {
        return Process(-1);
    }

    static Process idle()
    {
        return Process(0);
    }

    static Process me()
    {
        return Process(getpid());
    }

    int64_t as_int() const
    {
        return pid_;
    }

    Thread as_thread() const;
    ExecutionScope as_scope() const;

    friend std::ostream& operator<<(std::ostream& stream, const Process& process)
    {
        return stream << fmt::format("{}", process);
    }

private:
    int64_t pid_;
};
} // namespace lo2s

namespace fmt
{
template <>
struct formatter<lo2s::Process>
{
    static constexpr auto parse(format_parse_context& ctx)
    {
        const auto* it = ctx.begin();
        const auto* end = ctx.end();
        if (it != end && *it != '}')
        {
            throw format_error("invalid format");
        }

        return it;
    }

    template <typename FormatContext>
    auto format(const lo2s::Process& process, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), "process {}", process.as_int());
    }
};
} // namespace fmt
