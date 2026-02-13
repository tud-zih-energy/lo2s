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

class ExecutionScope;

namespace lo2s
{
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
