// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <iostream>

#include <cstddef>
#include <cstdint>

#include <fmt/base.h>
#include <fmt/format.h>

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
        return ((std::hash<int64_t>()(static_cast<std::size_t>(t.as_int()))));
    }
};
} // namespace std
