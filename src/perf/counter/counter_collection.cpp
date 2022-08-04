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
#include <lo2s/perf/counter/counter_collection.hpp>
#include <lo2s/perf/event_provider.hpp>
#include <lo2s/platform.hpp>

namespace lo2s
{
namespace perf
{
namespace counter
{

std::vector<EventDescription> collect_requested_userspace_counters()
{
    const auto& user_events = lo2s::config().perf_userspace_events;

    std::vector<perf::EventDescription> used_counters;

    used_counters.reserve(user_events.size());
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
    return used_counters;
}
std::vector<CounterCollection> collect_requested_group_counters()
{
    std::vector<CounterCollection> collections;
    
    for(const auto& event_group : lo2s::config().perf_group_events)
    {
        std::vector<perf::EventDescription> used_counters;
        
        used_counters.reserve(event_group.events.size());
        for (const auto& ev : event_group.events)
        {
            // skip event if it has already been declared as group leader
            if (ev == event_group.metric_leader)
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

        EventDescription leader_event;

        if(event_group.metric_leader == "")
        {
            leader_event = EventProvider::get_default_metric_leader_event;
        }
        else
        {
            leader_event = EventProvider::get_event_by_name(event_group.metric_leader);
        }

        collections.emplace_back(CounterCollection{ leader_event,
             std::move(used_counters) });

    }
    return collections;
}

const std::vector<EventDescription>& requested_userspace_counters()
{
    static std::vector<EventDescription> counters{ collect_requested_userspace_counters() };
    return counters;
}

const std::vector<CounterCollection>& requested_group_counters()
{
    static std::vector<CounterCollection> counters{ collect_requested_group_counters() };
    return counters;
}

} // namespace counter
} // namespace perf
} // namespace lo2s
