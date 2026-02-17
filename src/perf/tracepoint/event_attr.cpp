/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2024,
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

#include <lo2s/perf/tracepoint/event_attr.hpp>

#include <regex>

namespace lo2s
{
namespace perf
{
namespace tracepoint
{

TracepointEventAttr::TracepointEventAttr(const std::string& name)
: EventAttr(name, PERF_TYPE_TRACEPOINT, 0)
{
    parse_format();

    // update to correct config (id_ set in parse_format())
    attr_.config = id_;
    update_availability();
}

const std::filesystem::path TracepointEventAttr::base_path_ = "/sys/kernel/tracing/events";

TracepointEventAttr::ParseError::ParseError(const std::string& what, int error_code)
: std::runtime_error{ what + ": " + std::strerror(error_code) }
{
}

void TracepointEventAttr::parse_format()
{
    using namespace std::string_literals;

    // allow perf-like name format which uses ':' as a separator
    std::replace(name_.begin(), name_.end(), ':', '/');

    std::filesystem::path path_event = base_path_ / name_;
    std::ifstream ifs_id, ifs_format;

    auto id_path = path_event / "id";
    auto format_path = path_event / "format";

    ifs_id.open(id_path);
    ifs_id >> id_;

    if (ifs_id.fail())
    {
        throw ParseError{ "Failed to read tracepoint ID file "s + id_path.string(), errno };
    }

    ifs_format.open(format_path);

    if (ifs_format.fail())
    {
        throw ParseError{ "Failed to open tracepoint format file "s + format_path.string(), errno };
    }

    std::string line;
    while (std::getline(ifs_format, line))
    {
        parse_format_line(line);
    }

    if (ifs_format.bad())
    {
        throw ParseError{ "Unexpected error while reading tracepoint format description" };
    }
}

void TracepointEventAttr::parse_format_line(const std::string& line)
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
    tracepoint::EventField field(name, offset, size);

    if (!nitro::lang::starts_with(name, "common_"))
    {
        fields_.emplace_back(field);
    }
}

} // namespace tracepoint
} // namespace perf
} // namespace lo2s
