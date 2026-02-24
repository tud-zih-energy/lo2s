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

#include <string>
#include <utility>

#include <cstddef>

namespace lo2s::perf::tracepoint
{
class EventField
{
public:
    EventField() = default;

    EventField(std::string name, std::ptrdiff_t offset, std::size_t size)
    : name_(std::move(name)), offset_(offset), size_(size)
    {
    }

    const std::string& name() const
    {
        return name_;
    }

    std::ptrdiff_t offset() const
    {
        return offset_;
    }

    std::size_t size() const
    {
        return size_;
    }

    bool is_integer() const
    {
        // Parsing the type name is hard... really you don't want to do that
        switch (size())
        {
        case 1:
        case 2:
        case 4:
        case 8:
            return true;
        default:
            return false;
        }
    }

    bool valid() const
    {
        return size_ > 0;
    }

private:
    std::string name_;
    std::ptrdiff_t offset_;
    std::size_t size_ = 0;
};
} // namespace lo2s::perf::tracepoint
