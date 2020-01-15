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

    if (config().standard_metrics)
    {
        try
        {

            for (const auto& description : platform::get_mem_events())
            {
                if (description.name != config().metric_leader)
                {
                    used_counters.emplace_back(description);
                }
            }

            if ("instructions" != config().metric_leader)
            {
                used_counters.emplace_back(perf::EventProvider::get_event_by_name("instructions"));
            }

            if ("cpu-cycles" != config().metric_leader)
            {
                used_counters.emplace_back(perf::EventProvider::get_event_by_name("cpu-cycles"));
            }
        }
        catch (const perf::EventProvider::InvalidEvent&)
        {
            Log::error() << "Failed to add an event requested as part of the standard metrics.";
            throw;
        }
    }

    if (used_counters.empty())
    {
        // if no events will be recorded, we make an early exit with a fake leader
        return { CounterDescription(std::string(), static_cast<perf_type_id>(-1), 0, 0),
                 std::move(used_counters) };
    }
    if (config().metric_leader.empty())
    {
        Log::error() << "Failed to determine a suitable metric leader event";
        Log::error() << "Try manually specifying one with --metric-leader.";
        throw perf::EventProvider::InvalidEvent(lo2s::config().metric_leader);
    }

    if (!perf::EventProvider::has_event(lo2s::config().metric_leader))
    {
        lo2s::Log::error() << "event '" << lo2s::config().metric_leader
                           << "' is not available as a metric leader!";
        throw perf::EventProvider::InvalidEvent(lo2s::config().metric_leader);
    }

    return { perf::EventProvider::get_event_by_name(config().metric_leader),
             std::move(used_counters) };
}

const EventCollection& requested_events()
{
    static EventCollection events{ collect_requested_events() };
    return events;
}
} // namespace perf
} // namespace lo2s
