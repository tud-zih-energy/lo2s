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
EventCollection collect_requested_events()
{
    const auto& user_events = lo2s::config().perf_events;

    perf::CounterDescription leader(perf::EventProvider::get_event_by_name(config().metric_leader));
    std::vector<perf::CounterDescription> used_counters;

    used_counters.reserve(user_events.size());
    for (const auto& ev : user_events)
    {
        // skip event if it has already been declared as group leader
        if (ev == config().metric_leader)
        {
            Log::info() << "'" << ev
                        << "' has been requested as both the metric leader event and a regular "
                           "metric event. Will treat it as the leader.";
            continue;
        }
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

    return { leader, used_counters };
}

const EventCollection& requested_events()
{
    static EventCollection events{ collect_requested_events() };
    return events;
}
} // namespace perf
} // namespace lo2s
