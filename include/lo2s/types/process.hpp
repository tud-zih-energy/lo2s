// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <iostream>

#include <cstdint>

#include <fmt/base.h>
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
