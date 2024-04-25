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

    Address truncate_bits(int bits) const
    {
        uint64_t mask = -1;
        mask = (mask >> bits) << bits;
        return value() & mask;
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

struct Range
{
    class Error : public std::runtime_error
    {
    public:
        Error(const std::string& what) : std::runtime_error(what)
        {
        }
    };

    Range(Address point) : Range(point, point + 1)
    {
    }

    Range(Address min_, Address max_) : start(min_), end(max_)
    {
        if (min_ >= max_)
        {
            throw Error("malformed range");
        }
    }

    Address start;
    Address end;

    bool operator<(const Range& other) const
    {
        // Must not overlap!
        // [-----]
        //     [------]
        //
        //     [------]
        // [------]
        // But supersets are ok (equal)
        if ((start < other.start && other.start < end && end < other.end) ||
            (other.start < start && start < other.end && other.end < end))
        {
            throw Error("overlapping ranges");
        }

        return end <= other.start;
    }
};
} // namespace lo2s
