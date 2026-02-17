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

#include <cstdint>

#include <fmt/format.h>

namespace lo2s
{
class NecDevice
{
public:
    explicit NecDevice(int64_t id) : id_(id)
    {
    }

    int64_t as_int() const
    {
        return id_;
    }

    friend bool operator==(const NecDevice& lhs, const NecDevice& rhs)
    {
        return lhs.id_ == rhs.id_;
    }

    friend bool operator<(const NecDevice& lhs, const NecDevice& rhs)
    {
        return lhs.id_ < rhs.id_;
    }

private:
    int64_t id_;
};
} // namespace lo2s

namespace fmt
{
template <>
struct formatter<lo2s::NecDevice>
{
    constexpr auto parse(format_parse_context& ctx)
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
    auto format(const lo2s::NecDevice& device, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), "VE {}", device.as_int());
    }
};
} // namespace fmt
