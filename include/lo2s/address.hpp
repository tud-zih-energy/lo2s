/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2016,
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

#include <sstream>

#include <cstdint>
#include <fmt/format.h>
#include <lo2s/log.hpp>

namespace lo2s
{

/**
 * This type denotes a memory address in the observed program
 */
class Address
{
public:
    explicit Address(const std::string s)
    {
        std::stringstream ss;
        ss << std::hex << s;
        ss >> v_;
    }

    Address(uint64_t v) : v_(v)
    {
    }

    uint64_t value() const
    {
        return v_;
    }

    bool operator==(Address rhs) const
    {
        return v_ == rhs.v_;
    }

    bool operator!=(Address rhs) const
    {
        return v_ != rhs.v_;
    }

    bool operator<(Address rhs) const
    {
        return v_ < rhs.v_;
    }

    bool operator<=(Address rhs) const
    {
        return v_ <= rhs.v_;
    }

    bool operator>(Address rhs) const
    {
        return v_ > rhs.v_;
    }

    bool operator>=(Address rhs) const
    {
        return v_ >= rhs.v_;
    }

    Address operator+(Address rhs) const
    {
        return Address(v_ + rhs.v_);
    }

    Address operator-(Address rhs) const
    {
        return Address(v_ - rhs.v_);
    }

private:
    uint64_t v_;
};

inline std::ostream& operator<<(std::ostream& os, Address a)
{
    static_assert(sizeof(a.value()) == sizeof(void*),
                  "internal address should be the same bit-width as void*");
    return os << reinterpret_cast<void*>(a.value());
}

// Range start is inclusive, end is exclusive: [start, end)
struct Range
{
    class Error : public std::runtime_error
    {
    public:
        Error(const std::string& what) : std::runtime_error(what)
        {
        }
    };

    // A point range is defined as [point, point + 1)
    // If the point is UINT64_MAX, the highest address, we can obviously not do + 1,
    // so do [point, point) instead. This works reasonably well with the operators we have defined,
    // although it is mathematically kinda unsound.
    Range(Address point) : Range(point, point == UINT64_MAX ? UINT64_MAX : point + 1)
    {
    }

    Range(Address min_, Address max_) : start(min_), end(max_)
    {
        if (min_ > max_)
        {
            throw Error(fmt::format("malformed range: {} > {}", min_, max_));
        }
    }

    Address start;
    Address end;

    friend bool operator==(const Range& lhs, const Range& rhs)
    {
        return lhs.start == rhs.start && lhs.end == rhs.end;
    }

    friend bool operator<(const Range& lhs, const Range& rhs)
    {
        return lhs.end <= rhs.start;
    }

    // Checks whether this Range is completely inside the other Range.
    // Ranges that are equal are considered to be inside each other.
    bool in(const Range& other) const
    {
        return start >= other.start && end <= other.end;
    }
};

struct Mapping
{
    Mapping() : range(0, 0), pgoff(0)
    {
    }

    Mapping(Address s, Address e, Address o) : range(s, e), pgoff(o)
    {
    }

    Mapping(Address addr) : range(addr), pgoff(0)
    {
    }

    Range range;
    Address pgoff;

    bool operator==(const Mapping& other) const
    {
        return range == other.range;
    }

    bool operator<(const Mapping& other) const
    {
        return range < other.range;
    }

    static Mapping max()
    {
        return Mapping(0, UINT64_MAX, 0);
    }

    bool in(const Mapping& m) const
    {
        return range.in(m.range);
    }
};
} // namespace lo2s

namespace fmt
{
template <>
struct formatter<lo2s::Address>
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
    auto format(const lo2s::Address& addr, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), "{:x}", addr.value());
    }
};

template <>
struct formatter<lo2s::Range>
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
    auto format(const lo2s::Range& range, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), "{}-{}", range.start, range.end);
    }
};
} // namespace fmt
