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

#include <stdexcept>
#include <vector>

#include <cstddef>

#include <lo2s/filesystem.hpp>

namespace lo2s
{
namespace perf
{
namespace tracepoint
{
class EventField
{
public:
    EventField()
    {
    }

    EventField(const std::string& name, std::ptrdiff_t offset, std::size_t size)
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

class EventFormat
{
public:
    class ParseError : public std::runtime_error
    {
    public:
        ParseError(const std::string& what) : std::runtime_error(what)
        {
        }

        ParseError(const std::string& what, int error_code);
    };

public:
    EventFormat(const std::string& name);

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

    const auto& field(const std::string& name) const
    {
        for (const auto& field : fields())
        {
            if (field.name() == name)
            {
                return field;
            }
        }
        throw std::out_of_range("field not found");
    }

    static std::vector<std::string> get_tracepoint_event_names();

private:
    void parse_id();
    void parse_format_line(const std::string& line);

    std::string name_;
    int id_;
    std::vector<EventField> common_fields_;
    std::vector<EventField> fields_;

    const static std::filesystem::path base_path_;
};
} // namespace tracepoint
} // namespace perf
} // namespace lo2s
