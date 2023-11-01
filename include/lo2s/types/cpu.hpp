/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2017,
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

#include <iostream>

namespace lo2s
{

class ExecutionScope;
class Cpu
{
public:
    explicit Cpu(int cpuid) : cpu_(cpuid)
    {
    }
    int as_int() const;
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
    int cpu_;
};

class Core
{
public:
    Core(int core_id, int package_id) : core_id_(core_id), package_id_(package_id)
    {
    }

    static Core invalid()
    {
        return Core(-1, -1);
    }

    friend bool operator==(const Core& lhs, const Core& rhs)
    {
        return (lhs.core_id_ == rhs.core_id_) && (lhs.package_id_ == rhs.package_id_);
    }

    friend bool operator<(const Core& lhs, const Core& rhs)
    {
        if (lhs.package_id_ == rhs.package_id_)
        {
            return lhs.core_id_ < rhs.core_id_;
        }
        return lhs.package_id_ < rhs.package_id_;
    }

    int core_as_int() const
    {
        return core_id_;
    }

    int package_as_int() const
    {
        return package_id_;
    }

private:
    int core_id_;
    int package_id_;
};

class Package
{
public:
    explicit Package(int id) : id_(id)
    {
    }

    static Package invalid()
    {
        return Package(-1);
    }

    friend bool operator==(const Package& lhs, const Package& rhs)
    {
        return lhs.id_ == rhs.id_;
    }
    friend bool operator<(const Package& lhs, const Package& rhs)
    {
        return lhs.id_ < rhs.id_;
    }

    int as_int() const
    {
        return id_;
    }

private:
    int id_;
};

} // namespace lo2s
namespace fmt
{
template <>
struct formatter<lo2s::Cpu>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        auto it = ctx.begin(), end = ctx.end();
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
