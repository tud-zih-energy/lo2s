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
#include <lo2s/perf/counter/counter_provider.hpp>
#include <lo2s/perf/event_provider.hpp>
#include <lo2s/platform.hpp>

#include <cassert>

namespace lo2s
{
namespace perf
{
namespace counter
{
void CounterProvider::initialize_tracepoints(const std::vector<std::string>& tracepoints)
{
    assert(tracepoint_events_.empty());

    for (const auto& ev_name : tracepoints)
    {
        try
        {
            tracepoint_events_.emplace_back(
                EventProvider::instance().create_tracepoint_event(ev_name, false));
        }
        catch (const perf::EventProvider::InvalidEvent& e)
        {
            Log::warn() << "'" << ev_name
                        << "' does not name a known event, ignoring! (reason: " << e.what() << ")";
        }
    }
}

void CounterProvider::initialize_userspace_counters(const std::vector<std::string>& counters)
{
    assert(userspace_events_.empty());

    for (const auto& ev : counters)
    {
        try
        {
            userspace_events_.emplace_back(perf::EventProvider::get_event_by_name(ev));
            userspace_events_.back().sample_period(0);
        }
        catch (const perf::EventProvider::InvalidEvent& e)
        {
            Log::warn() << "'" << ev
                        << "' does not name a known event, ignoring! (reason: " << e.what() << ")";
        }
    }
}

void CounterProvider::initialize_group_counters(const std::string& leader,
                                                const std::vector<std::string>& counters)
{
    assert(group_events_.empty());

    if (leader == "")
    {
        Log::info() << "choosing default metric-leader";

        try
        {
            group_leader_ = EventProvider::get_event_by_name("cpu-clock");
        }
        catch (const EventProvider::InvalidEvent& e)
        {
            Log::warn() << "cpu-clock isn't available, trying to use a fallback event";
            try
            {
                group_leader_ = EventProvider::fallback_metric_leader_event();
            }
            catch (const perf::EventProvider::InvalidEvent& e)
            {
                Log::error() << "Failed to determine a suitable metric leader event";
                Log::error() << "Try manually specifying one with --metric-leader.";

                throw perf::EventProvider::InvalidEvent(leader);
            }
        }
    }
    else
    {
        try
        {
            group_leader_ = perf::EventProvider::get_event_by_name(leader);
        }
        catch (const perf::EventProvider::InvalidEvent& e)
        {
            Log::error() << "Metric leader " << leader << " not available.";
            Log::error() << "Please choose another metric leader.";

            throw perf::EventProvider::InvalidEvent(leader);
        }
    }

    // DONT do group_leader_.sample_freq() here, since it requires config() to be complete

    for (const auto& ev : counters)
    {
        try
        {
            // skip event if it has already been declared as group leader
            if (ev == group_leader_.name())
            {
                Log::info() << "'" << ev
                            << "' has been requested as both the metric leader event and a regular "
                               "metric event. Will treat it as the leader.";
                continue;
            }

            group_events_.emplace_back(perf::EventProvider::get_event_by_name(ev));
            group_events_.back().sample_period(0);
        }
        catch (const perf::EventProvider::InvalidEvent& e)
        {
            Log::warn() << "'" << ev
                        << "' does not name a known event, ignoring! (reason: " << e.what() << ")";
        }
    }
}

CounterCollection CounterProvider::collection_for(MeasurementScope scope)
{
    assert(scope.type == MeasurementScopeType::GROUP_METRIC ||
           scope.type == MeasurementScopeType::USERSPACE_METRIC ||
           scope.type == MeasurementScopeType::TRACEPOINT);

    CounterCollection res;
    if (scope.type == MeasurementScopeType::GROUP_METRIC)
    {
        if (group_leader_.is_available_in(scope.scope))
        {
            res.leader = group_leader_;
            for (auto& ev : group_events_)
            {
                if (ev.is_available_in(scope.scope))
                {
                    res.counters.emplace_back(std::move(ev));
                }
            }
        }
    }
    else if (scope.type == MeasurementScopeType::USERSPACE_METRIC)
    {
        for (auto& ev : userspace_events_)
        {
            if (ev.is_available_in(scope.scope))
            {
                res.counters.emplace_back(std::move(ev));
            }
        }
    }
    else
    {
        for (auto& ev : tracepoint_events_)
        {
            if (ev.is_available_in(scope.scope))
            {
                res.counters.emplace_back(std::move(ev));
            }
        }
    }
    return res;
}

std::vector<std::string> CounterProvider::get_tracepoint_event_names()
{
    try
    {
        std::ifstream ifs_available_events;
        ifs_available_events.exceptions(std::ios::failbit | std::ios::badbit);

        ifs_available_events.open("/sys/kernel/debug/tracing/available_events");
        ifs_available_events.exceptions(std::ios::badbit);

        std::vector<std::string> available;

        for (std::string tracepoint; std::getline(ifs_available_events, tracepoint);)
        {
            available.emplace_back(std::move(tracepoint));
        }

        return available;
    }
    catch (const std::ios_base::failure& e)
    {
        Log::debug() << "Retrieving kernel tracepoint event names failed: " << e.what();
        return {};
    }
}

bool CounterProvider::has_group_counters(ExecutionScope scope)
{
    if (scope.is_process())
    {
        return !group_events_.empty();
    }
    else
    {
        return group_leader_.is_available_in(scope) &&
               std::any_of(group_events_.begin(), group_events_.end(),
                           [scope](const auto& ev) { return ev.is_available_in(scope); });
    }
    return false;
}

bool CounterProvider::has_userspace_counters(ExecutionScope scope)
{
    if (scope.is_process())
    {
        return !userspace_events_.empty();
    }
    else
    {
        return std::any_of(userspace_events_.begin(), userspace_events_.end(),
                           [scope](const auto& ev) { return ev.is_available_in(scope); });
    }

    return false;
}

} // namespace counter
} // namespace perf
} // namespace lo2s
