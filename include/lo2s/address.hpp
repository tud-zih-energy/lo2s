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
class address
{
public:
    explicit address(const std::string s)
    {
        std::stringstream ss;
        ss << std::hex << s;
        ss >> v_;
    }
    address(uint64_t v) : v_(v)
    {
    }

    uint64_t value() const
    {
        return v_;
    }

    bool operator==(address rhs) const
    {
        return v_ == rhs.v_;
    }

    bool operator!=(address rhs) const
    {
        return v_ != rhs.v_;
    }

    bool operator<(address rhs) const
    {
        return v_ < rhs.v_;
    }
    bool operator<=(address rhs) const
    {
        return v_ <= rhs.v_;
    }
    bool operator>(address rhs) const
    {
        return v_ > rhs.v_;
    }
    bool operator>=(address rhs) const
    {
        return v_ >= rhs.v_;
    }

    address operator+(address rhs) const
    {
        return address(v_ + rhs.v_);
    }

    address operator-(address rhs) const
    {
        return address(v_ - rhs.v_);
    }

    address truncate_bits(int bits) const
    {
        uint64_t mask = -1;
        mask = (mask >> bits) << bits;
        return value() & mask;
    }

private:
    uint64_t v_;
};

inline std::ostream& operator<<(std::ostream& os, address a)
{
    static_assert(sizeof(a.value()) == sizeof(void*),
                  "internal address should be the same bit-width as void*");
    return os << reinterpret_cast<void*>(a.value());
}

struct range
{
    class error : public std::runtime_error
    {
    public:
        error(const std::string& what) : std::runtime_error(what)
        {
        }
    };
    range(address point) : range(point, point + 1)
    {
    }
    range(address min_, address max_) : start(min_), end(max_)
    {
        if (min_ >= max_)
        {
            throw error("malformed range");
        }
    }
    address start;
    address end;

    bool operator<(const range& other) const
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
            throw error("overlapping ranges");
        }

        return end <= other.start;
    }
};
}
