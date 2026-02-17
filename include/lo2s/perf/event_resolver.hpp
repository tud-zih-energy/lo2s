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

#include <lo2s/perf/tracepoint/event_attr.hpp>

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace lo2s
{
namespace perf
{

class EventResolver
{
public:
    EventAttr get_event_by_name(const std::string& name);

    bool has_event(const std::string& name);

    std::vector<EventAttr> get_predefined_events();
    std::vector<SysfsEventAttr> get_pmu_events();

    EventAttr get_metric_leader(const std::string& metric_leader);

    std::vector<std::string> get_tracepoint_event_names();

    static EventResolver& instance()
    {
        static EventResolver e;
        return e;
    }

private:
    EventResolver();
    EventResolver(const EventResolver&) = delete;
    EventResolver& operator=(const EventResolver&) = delete;
    EventResolver(const EventResolver&&) = delete;
    EventResolver& operator=(const EventResolver&&) = delete;

    EventAttr cache_event(const std::string& name);
    EventAttr fallback_metric_leader_event();

    std::unordered_map<std::string, std::optional<EventAttr>> event_map_;
};

} // namespace perf
} // namespace lo2s
