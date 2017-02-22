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

#include <lo2s/perf/tracepoint/format.hpp>

#include <lo2s/log.hpp>

#include <regex>
#include <string>

#include <boost/algorithm/string/predicate.hpp>

namespace lo2s
{
namespace perf
{
namespace tracepoint
{

EventFormat::EventFormat(const std::string& name) : name_(name)
{
    boost::filesystem::path path_event = base_path_ / name;
    boost::filesystem::ifstream ifs_id, ifs_format;

    ifs_id.exceptions(std::ios::failbit | std::ios::badbit);
    ifs_format.exceptions(std::ios::failbit | std::ios::badbit);

    try
    {
        ifs_id.open(path_event / "id");
        ifs_id >> id_;

        ifs_format.open(path_event / "format");
        std::string line;
        ifs_format.exceptions(std::ios::badbit);
        while (getline(ifs_format, line))
        {
            parse_format_line(line);
        }
    }
    catch (...)
    {
        Log::error() << "Couldn't read information from " << path_event;
        throw;
    }
}

void EventFormat::parse_format_line(const std::string& line)
{
    static std::regex field_regex(
        "^\\s+field:([^;]+);\\s+offset:(\\d+);\\s+size:(\\d+);\\s+signed:(\\d+);$");
    static std::regex type_name_regex("^(.*) ([^ \\[\\]]+)(\\[[^\\]]+\\])?$");

    std::smatch field_match;
    if (!std::regex_match(line, field_match, field_regex))
    {
        Log::trace() << "Discarding line from parsing " << name_ << "/format: " << line;
        return;
    }

    std::string param = field_match[1];
    auto offset = stol(field_match[2]);
    auto size = stol(field_match[3]);

    std::smatch type_name_match;
    if (!std::regex_match(param, type_name_match, type_name_regex))
    {
        Log::warn() << "Could not parse type/name of tracepoint event field line for " << name_
                    << ", " << line;
        return;
    }

    std::string name = type_name_match[2];
    EventField field(name, offset, size);

    if (boost::starts_with(name, "common_"))
    {
        common_fields_.emplace_back(field);
    }
    else
    {
        fields_.emplace_back(field);
    }
}

const boost::filesystem::path EventFormat::base_path_ = "/sys/kernel/debug/tracing/events";
}
}
}
