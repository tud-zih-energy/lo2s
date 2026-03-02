// SPDX-FileCopyrightText: 2026 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <iostream>

#include <cstdint>

#include <fmt/base.h>
#include <fmt/format.h>

namespace lo2s
{
class ExecutionScope;

class Cpu
{
public:
    explicit Cpu(int64_t cpuid) : cpu_(cpuid)
    {
    }

    int64_t as_int() const;
    ExecutionScope as_scope() const;

    static Cpu invalid()
    {
        return Cpu(-1);
    }

    friend bool operator==(const Cpu& lhs, const Cpu& rhs)
    {
        return lhs.cpu_ == rhs.cpu_;
    }

    friend bool operator<(const Cpu& lhs, const Cpu& rhs)
    {
        return lhs.cpu_ < rhs.cpu_;
    }

    friend bool operator>(const Cpu& lhs, const Cpu& rhs)
    {
        return lhs.cpu_ > rhs.cpu_;
    }

    friend std::ostream& operator<<(std::ostream& stream, const Cpu& cpu)
    {
        return stream << fmt::format("{}", cpu);
    }

private:
    int64_t cpu_;
};

} // namespace lo2s

namespace fmt
{
template <>
struct formatter<lo2s::Cpu>
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
    auto format(const lo2s::Cpu& cpu, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), "cpu {}", cpu.as_int());
    }
};
} // namespace fmt
