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

#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/event_collection.hpp>
#include <lo2s/perf/event_provider.hpp>
#include <lo2s/platform.hpp>

namespace lo2s
{
namespace perf
{
std::vector<lo2s::perf::CounterDescription> collect_requested_events()
{
    const auto& mem_events = platform::get_mem_events();
    const auto& user_events = lo2s::config().perf_events;

    std::vector<perf::CounterDescription> used_counters;

    used_counters.reserve(mem_events.size() + user_events.size());
    for (const auto& ev : user_events)
    {
        try
        {
            const auto event_desc = perf::EventProvider::get_event_by_name(ev);
            used_counters.emplace_back(event_desc);
        }
        catch (const perf::EventProvider::InvalidEvent& e)
        {
            Log::warn() << "'" << ev
                        << "' does not name a known event, ignoring! (reason: " << e.what() << ")";
        }
    }

    if (user_events.size() == 0)
    {
        for (const auto& description : mem_events)
        {
            used_counters.emplace_back(description);
        }

        used_counters.emplace_back(perf::EventProvider::get_event_by_name("instructions"));
        used_counters.emplace_back(perf::EventProvider::get_event_by_name("cpu-cycles"));
    }

    return used_counters;
}
}
}
