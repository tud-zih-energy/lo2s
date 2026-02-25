/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2016,
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

#include <lo2s/perf/event_composer.hpp>

#include <lo2s/config.hpp>
#include <lo2s/execution_scope.hpp>
#include <lo2s/log.hpp>
#include <lo2s/measurement_scope.hpp>
#include <lo2s/perf/counter/counter_collection.hpp>
#include <lo2s/perf/event_attr.hpp>
#include <lo2s/perf/event_resolver.hpp>
#include <lo2s/perf/tracepoint/event_attr.hpp>
#include <lo2s/types/thread.hpp>

#include <optional>
#include <vector>

#include <cassert>
#include <cstdint>

#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>

namespace lo2s::perf
{

EventComposer::EventComposer() = default;

namespace
{
// Due to differences in the kernels and hardwares implementation of sampling, the preciseness of
// the intruction pointers returned by sample events can range from the exact IP at the moment of
// the triggering event occuring to an arbitrary IP in the vicinity of the triggering sampling event
// occuring.
//
// The requested preciseness level can be controlled with precise_ip, with values ranging from 3
// (most precise) to 0 (least precise).
//
// We are only interested in what the most precise available sampling mode is, so test the
// availability of precision levels in descending order of preciseness.
void set_precision(EventAttr& ev)
{
    uint64_t precise_ip = 3;
    while (true)
    {
        try
        {
            auto guard = ev.open(Thread(0));
            return;
        }
        catch (...)
        {
            if (precise_ip == 0)
            {
                throw;
            }
            ev.set_precise_ip(--precise_ip);
        }
    }
}
} // namespace

EventAttr EventComposer::create_sampling_event()
{
    if (sampling_event_.has_value())
    {
        return sampling_event_.value();
    }

    auto res = EventResolver::instance().get_event_by_name("dummy");

    if (config().perf.sampling.enabled)
    {
        res = EventResolver::instance().get_event_by_name(config().perf.sampling.event);
        Log::debug() << "using sampling event \'" << res.name()
                     << "\', period: " << config().perf.sampling.period;

        res.sample_period(config().perf.sampling.period);

        if (config().perf.sampling.use_pebs)
        {
            res.set_clockid(std::nullopt);
        }
        else
        {
            res.set_clockid(config().perf.clockid);
        }
        res.set_mmap();

        res.set_precise_ip(3);
    }

    res.set_sample_id_all();
    res.set_comm();
    res.set_task();
    res.set_context_switch();

    if (exclude_kernel_)
    {
        res.set_exclude_kernel();
    }

    watermark(res);

    // TODO see if we can remove remove tid
    res.set_sample_type(PERF_SAMPLE_TIME | PERF_SAMPLE_IP | PERF_SAMPLE_TID | PERF_SAMPLE_CPU);

    if (config().perf.sampling.enable_callgraph)
    {
        res.set_sample_type(PERF_SAMPLE_CALLCHAIN);
    }

    if (config().perf.sampling.enabled)
    {
        set_precision(res);
    }

    sampling_event_ = res;
    return sampling_event_.value();
}

EventAttr EventComposer::create_time_event(uint64_t local_time [[maybe_unused]])

{
#ifndef USE_HW_BREAKPOINT_COMPAT
    BreakpointEventAttr ev(local_time, HW_BREAKPOINT_W);

    ev.sample_period(1);
    ev.set_watermark(1);
    ev.set_clockid(config().perf.clockid);
    ev.set_sample_type(PERF_SAMPLE_TIME);
#else
    EventAttr ev = EventResolver::instance().get_event_by_name("instructions");

    ev.sample_period(100000000);
    ev.set_task();
#endif

    ev.set_exclude_kernel();
    ev.set_disabled();

    return ev;
}

std::vector<perf::tracepoint::TracepointEventAttr> EventComposer::emplace_tracepoints()
{
    if (tracepoint_events_.has_value())
    {
        return tracepoint_events_.value();
    }

    auto res = std::vector<tracepoint::TracepointEventAttr>();
    for (const auto& ev_name : config().perf.tracepoints.events)
    {
        res.emplace_back(create_tracepoint_event(ev_name));
    }

    tracepoint_events_ = res;
    return tracepoint_events_.value();
}

perf::tracepoint::TracepointEventAttr
EventComposer::create_tracepoint_event(const std::string& name)
{
    auto ev = tracepoint::TracepointEventAttr(name);

    watermark(ev);

    ev.sample_period(1);

    ev.set_clockid(config().perf.clockid);
    ev.set_sample_type(PERF_SAMPLE_RAW | PERF_SAMPLE_TIME);
    ev.set_disabled();

    return ev;
}

void EventComposer::emplace_userspace_counters()
{
    if (userspace_counters_.has_value())
    {
        return;
    }

    counter::CounterCollection res;
    for (const auto& ev : config().perf.userspace.counters)
    {
        try
        {
            res.counters.emplace_back(perf::EventResolver::instance().get_event_by_name(ev));
        }
        catch (const EventAttr::InvalidEvent& e)
        {
            Log::warn() << "'" << ev
                        << "' does not name a known event, ignoring! (reason: " << e.what() << ")";
        }
    }

    userspace_counters_ = res;
}

void EventComposer::emplace_group_counters()
{

    if (group_counters_.has_value())
    {
        return;
    }

    if (!config().perf.group.enabled)
    {
        group_counters_ = counter::CounterCollection{};
        return;
    }

    counter::CounterCollection res;

    res.leader = EventResolver::instance().get_metric_leader(config().perf.group.leader);
    res.leader->set_sample_type(PERF_SAMPLE_TIME | PERF_SAMPLE_READ);

    if (config().perf.group.use_frequency)
    {
        res.leader->sample_freq(config().perf.group.frequency);
    }
    else
    {
        res.leader->sample_period(config().perf.group.count);
    }

    res.leader->set_clockid(config().perf.clockid);

    res.leader->set_read_format(PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING |
                                PERF_FORMAT_GROUP);

    watermark(res.leader.value());

    // DONT do group_leader_.sample_freq() here, since it requires config() to be complete

    for (const auto& ev_name : config().perf.group.counters)
    {
        try
        {
            // skip event if it has already been declared as group leader
            if (ev_name == res.leader->name())
            {
                Log::info() << "'" << ev_name
                            << "' has been requested as both the metric leader event and a regular "
                               "metric event. Will treat it as the leader.";
                continue;
            }

            EventAttr const ev = perf::EventResolver::instance().get_event_by_name(ev_name);
            res.counters.emplace_back(ev);

            res.counters.back().set_clockid(config().perf.clockid);
            res.counters.back().set_read_format(PERF_FORMAT_TOTAL_TIME_ENABLED |
                                                PERF_FORMAT_TOTAL_TIME_RUNNING);
        }
        catch (const EventAttr::InvalidEvent& e)
        {
            Log::warn() << "'" << ev_name
                        << "' does not name a known event, ignoring! (reason: " << e.what() << ")";
        }
    }

    group_counters_ = res;
}

bool EventComposer::has_counters_for(MeasurementScope scope)
{
    return !counters_for(scope).counters.empty();
}

counter::CounterCollection EventComposer::group_counters_for(ExecutionScope scope)
{
    counter::CounterCollection res;
    emplace_group_counters();

    if (!group_counters_.has_value())
    {
        return {};
    }

    auto group_counters = group_counters_.value();

    if (group_counters.empty())
    {
        return {};
    }

    if (!group_counters.leader.has_value())
    {
        return {};
    }

    if (group_counters.leader->is_available_in(scope))
    {
        res.leader = group_counters.leader.value();

        for (auto& ev : group_counters.counters)
        {
            if (ev.is_available_in(scope))
            {
                res.counters.emplace_back(ev);
            }
            else
            {
                Log::warn() << "Scope " << scope.name() << ": skipping " << ev.name()
                            << ": not available!";
            }
        }
    }
    return res;
}

counter::CounterCollection EventComposer::userspace_counters_for(ExecutionScope scope)
{
    counter::CounterCollection res;
    emplace_userspace_counters();

    if (!userspace_counters_.has_value())
    {
        return {};
    }
    for (auto& ev : userspace_counters_->counters)
    {
        if (ev.is_available_in(scope))
        {
            res.counters.emplace_back(ev);
        }
        else
        {
            Log::warn() << "Skipping " << ev.name() << " not availabe in " << scope.name() << "!";
        }
    }

    return res;
}

counter::CounterCollection EventComposer::counters_for(MeasurementScope scope)
{
    assert(scope.type == MeasurementScopeType::GROUP_METRIC ||
           scope.type == MeasurementScopeType::USERSPACE_METRIC);

    if (scope.type == MeasurementScopeType::GROUP_METRIC)
    {
        return group_counters_for(scope.scope);
    }
    return userspace_counters_for(scope.scope);
}

} // namespace lo2s::perf
