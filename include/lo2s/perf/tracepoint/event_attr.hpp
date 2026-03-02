// SPDX-FileCopyrightText: 2024 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <lo2s/perf/event_attr.hpp>
#include <lo2s/perf/tracepoint/format.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace lo2s::perf::tracepoint
{

/**
 * Contains an event that is addressable via name
 */
class TracepointEventAttr : public EventAttr
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

    TracepointEventAttr(const std::string& name);

    void parse_format();

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

    int id() const
    {
        return id_;
    }

private:
    void parse_format_line(const std::string& line);

    int id_;
    std::vector<tracepoint::EventField> fields_;
};

} // namespace lo2s::perf::tracepoint
