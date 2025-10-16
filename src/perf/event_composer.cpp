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

namespace lo2s
{
namespace perf
{

EventComposer::EventComposer()
{
}

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
static void set_precision(EventAttr& ev)
{
    uint64_t precise_ip = 3;
    do
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
    } while (true);
}

EventAttr EventComposer::create_sampling_event()
{
    if (sampling_event_.has_value())
    {
        return sampling_event_.value();
    }

    if (config().use_perf_sampling)
    {
        sampling_event_ = EventResolver::instance().get_event_by_name(config().perf_sampling_event);
        Log::debug() << "using sampling event \'" << sampling_event_->name()
                     << "\', period: " << config().perf_sampling_period;

        sampling_event_->sample_period(config().perf_sampling_period);

        if (config().use_pebs)
        {
            sampling_event_->set_clockid(std::nullopt);
        }
        else
        {
            sampling_event_->set_clockid(config().clockid);
        }
        sampling_event_->set_mmap();

        sampling_event_->set_precise_ip(3);
    }
    else
    {
        sampling_event_ = EventResolver::instance().get_event_by_name("dummy");
    }

    sampling_event_->set_sample_id_all();
    sampling_event_->set_comm();
    sampling_event_->set_task();
    sampling_event_->set_context_switch();

    if (exclude_kernel_)
    {
        sampling_event_->set_exclude_kernel();
    }

    watermark(sampling_event_.value());

    // TODO see if we can remove remove tid
    sampling_event_->set_sample_type(PERF_SAMPLE_TIME | PERF_SAMPLE_IP | PERF_SAMPLE_TID |
                                     PERF_SAMPLE_CPU);

    if (config().enable_callgraph)
    {
        sampling_event_->set_sample_type(PERF_SAMPLE_CALLCHAIN);
    }

    if (config().use_perf_sampling)
    {
        set_precision(sampling_event_.value());
    }

    return sampling_event_.value();
}

EventAttr EventComposer::create_time_event(uint64_t local_time [[maybe_unused]])

{
#ifndef USE_HW_BREAKPOINT_COMPAT
    BreakpointEventAttr ev(local_time, HW_BREAKPOINT_W);

    ev.sample_period(1);
    ev.set_watermark(1);
    ev.set_clockid(config().clockid);
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

    tracepoint_events_ = std::vector<tracepoint::TracepointEventAttr>();
    for (const auto& ev_name : config().tracepoint_events)
    {
        tracepoint_events_->emplace_back(create_tracepoint_event(ev_name));
    }
    return tracepoint_events_.value();
}

perf::tracepoint::TracepointEventAttr
EventComposer::create_tracepoint_event(const std::string& name)
{
    auto ev = tracepoint::TracepointEventAttr(name);

    watermark(ev);

    ev.sample_period(1);

    ev.set_clockid(config().clockid);
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
    for (const auto& ev : config().userspace_counters)
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

    if (config().metric_leader.empty() && config().group_counters.empty())
    {
        group_counters_ = counter::CounterCollection{};
        return;
    }

    counter::CounterCollection res;

    res.leader = EventResolver::instance().get_metric_leader(config().metric_leader);
    res.leader->set_sample_type(PERF_SAMPLE_TIME | PERF_SAMPLE_READ);

    if (config().metric_use_frequency)
    {
        res.leader->sample_freq(config().metric_frequency);
    }
    else
    {
        res.leader->sample_period(config().metric_count);
    }

    res.leader->set_clockid(config().clockid);

    res.leader->set_read_format(PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING |
                                PERF_FORMAT_GROUP);

    watermark(res.leader.value());

    // DONT do group_leader_.sample_freq() here, since it requires config() to be complete

    for (const auto& ev_name : config().group_counters)
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

            EventAttr ev = perf::EventResolver::instance().get_event_by_name(ev_name);
            res.counters.emplace_back(ev);

            res.counters.back().set_clockid(config().clockid);
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

counter::CounterCollection EventComposer::counters_for(MeasurementScope scope)
{
    assert(scope.type == MeasurementScopeType::GROUP_METRIC ||
           scope.type == MeasurementScopeType::USERSPACE_METRIC);

    counter::CounterCollection res;
    if (scope.type == MeasurementScopeType::GROUP_METRIC)
    {
        emplace_group_counters();

        if (group_counters_->empty())
        {
            return {};
        }

        if (group_counters_->leader->is_available_in(scope.scope))
        {
            res.leader = group_counters_->leader.value();

            for (auto& ev : group_counters_->counters)
            {
                if (ev.is_available_in(scope.scope))
                {
                    res.counters.emplace_back(ev);
                }
                else
                {
                    Log::warn() << "Scope " << scope.scope.name() << ": skipping " << ev.name()
                                << ": not available!";
                }
            }
        }
    }
    else if (scope.type == MeasurementScopeType::USERSPACE_METRIC)
    {
        emplace_userspace_counters();

        for (auto& ev : userspace_counters_->counters)
        {
            if (ev.is_available_in(scope.scope))
            {
                res.counters.emplace_back(ev);
            }
            else
            {
                Log::warn() << "Skipping " << ev.name() << " not availabe in " << scope.scope.name()
                            << "!";
            }
        }
    }

    return res;
}

} // namespace perf
} // namespace lo2s
