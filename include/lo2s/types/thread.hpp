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

#include <fmt/format.h>

#include <cstdint>
#include <iostream>

namespace lo2s
{

class ExecutionScope;
class Process;

class Thread
{
public:
    explicit Thread(int64_t tid) : tid_(tid)
    {
    }

    explicit Thread() : tid_(-1)
    {
    }

    friend bool operator==(const Thread& lhs, const Thread& rhs)
    {
        return lhs.tid_ == rhs.tid_;
    }

    friend bool operator!=(const Thread& lhs, const Thread& rhs)
    {
        return lhs.tid_ != rhs.tid_;
    }

    friend bool operator<(const Thread& lhs, const Thread& rhs)
    {
        return lhs.tid_ < rhs.tid_;
    }

    friend bool operator>(const Thread& lhs, const Thread& rhs)
    {
        return lhs.tid_ > rhs.tid_;
    }

    friend bool operator!(const Thread& thread)
    {
        return thread.tid_ == -1;
    }

    Process as_process() const;
    ExecutionScope as_scope() const;

    static Thread invalid()
    {
        return Thread(-1);
    }

    friend std::ostream& operator<<(std::ostream& stream, const Thread& thread)
    {
        return stream << fmt::format("{}", thread);
    }

    int64_t as_int() const
    {
        return tid_;
    }

private:
    int64_t tid_;
};
} // namespace lo2s

namespace fmt
{
template <>
struct formatter<lo2s::Thread>
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
    auto format(const lo2s::Thread& thread, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), "thread {}", thread.as_int());
    }
};
} // namespace fmt

namespace std
{
template <>
struct hash<lo2s::Thread>
{
    std::size_t operator()(const lo2s::Thread& t) const
    {
        return ((std::hash<int64_t>()(t.as_int())));
    }
};
} // namespace std
