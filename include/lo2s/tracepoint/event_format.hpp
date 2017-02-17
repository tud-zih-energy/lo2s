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

#include <vector>

#include <cstddef>

#include <boost/filesystem.hpp>

namespace lo2s
{

class event_field
{
public:
    event_field(const std::string& name, std::ptrdiff_t offset, std::size_t size)
    : name_(name), offset_(offset), size_(size)
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

private:
    std::string name_;
    std::ptrdiff_t offset_;
    std::size_t size_;
};

class event_format
{
public:
    event_format(const std::string& name);

    int id() const
    {
        return id_;
    }

    const auto& common_fields() const
    {
        return common_fields_;
    }

    const auto& fields() const
    {
        return fields_;
    }

private:
    void parse_format_line(const std::string& line);

    std::string name_;
    int id_;
    std::vector<event_field> common_fields_;
    std::vector<event_field> fields_;

    const static boost::filesystem::path base_path_;
};
}
