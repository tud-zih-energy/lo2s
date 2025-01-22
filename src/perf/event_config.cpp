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

#include <lo2s/perf/event_config.hpp>

namespace lo2s
{
namespace perf
{

EventConfig::EventConfig()
{
    auto test_event = EventProvider::instance().get_event_by_name("cpu-cycles");

    std::optional<EventGuard> guard;
    do
    {
        try
        {
            guard = test_event.open(Thread(0));
        }
        catch (std::system_error& e)
        {
            if (test_event.get_flag(EventFlag::EXCLUDE_KERNEL) && e.code().value() == EACCES &&
                perf_event_paranoid() > 1)
            {
                perf_warn_paranoid();
                exclude_kernel_ = 1;
                test_event.set_flags({ EventFlag::EXCLUDE_KERNEL });
            }
            else
            {
                throw;
            }
        }
    } while (!guard.has_value());
}

Event EventConfig::create_sampling_event()
{
    if (sampling_event_.has_value())
    {
        return sampling_event_.value();
    }

    if (config().sampling)
    {
        sampling_event_ = EventProvider::instance().get_event_by_name(config().sampling_event);
        Log::debug() << "using sampling event \'" << sampling_event_->name()
                     << "\', period: " << config().sampling_period;

        sampling_event_->sample_period(config().sampling_period);

        if (config().use_pebs)
        {
            sampling_event_->set_clockid(-1);
        }
        else
        {
            sampling_event_->set_clockid(config().clockid);
        }
        sampling_event_->set_flags({ EventFlag::MMAP });

        sampling_event_->set_precise_ip(3);
    }
    else
    {
        Event event = EventProvider::instance().get_event_by_name("dummy");
    }

    sampling_event_->set_flags(
        { EventFlag::SAMPLE_ID_ALL, EventFlag::COMM, EventFlag::CONTEXT_SWITCH });

    if (exclude_kernel_)
    {
        sampling_event_->set_flags({ EventFlag::EXCLUDE_KERNEL });
    }
    watermark(sampling_event_.value());

    // TODO see if we can remove remove tid
    sampling_event_->set_sample_type(PERF_SAMPLE_TIME | PERF_SAMPLE_IP | PERF_SAMPLE_TID |
                                     PERF_SAMPLE_CPU);

    if (config().enable_cct)
    {
        sampling_event_->set_sample_type(PERF_SAMPLE_CALLCHAIN);
    }

    if (config().sampling)
    {
        uint64_t precise_ip = 3;
        do
        {

            try
            {
                auto guard = sampling_event_->open(Thread(0));
                return sampling_event_.value();
            }
            catch (...)
            {
                if (precise_ip == 0)
                {
                    throw;
                }
                sampling_event_->set_precise_ip(--precise_ip);
            }

        } while (true);
    }

    return sampling_event_.value();
}

Event EventConfig::create_time_event(uint64_t local_time [[maybe_unused]])

{
#ifndef USE_HW_BREAKPOINT_COMPAT
    BreakpointEvent ev(local_time, HW_BREAKPOINT_W);
    ev.sample_period(1);
    ev.set_watermark(1);
    ev.set_clockid(config().clockid);
    ev.set_sample_type(PERF_SAMPLE_TIME);
#else
    Event ev = EventProvider::instance().get_event_by_name("instructions");
    ev.sample_period(100000000);
    ev.set_flags({ EventFlag::TASK });
#endif

    ev.set_flags({ EventFlag::EXCLUDE_KERNEL, EventFlag::DISABLED });

    return ev;
}

std::vector<perf::tracepoint::TracepointEvent> EventConfig::get_tracepoints()
{
    if (tracepoint_events_.has_value())
    {
        return tracepoint_events_.value();
    }

    tracepoint_events_ = std::vector<tracepoint::TracepointEvent>();
    for (auto& ev_name : config().tracepoint_events)
    {
        tracepoint_events_->emplace_back(create_tracepoint_event(ev_name));
    }
    return tracepoint_events_.value();
}

perf::tracepoint::TracepointEvent EventConfig::create_tracepoint_event(std::string name)
{
    auto ev = tracepoint::TracepointEvent(name);
    watermark(ev);
    ev.set_clockid(config().clockid);
    ev.sample_period(1);
    ev.set_sample_type(PERF_SAMPLE_RAW | PERF_SAMPLE_TIME);
    ev.set_flags({ EventFlag::DISABLED });
    return ev;
}

void EventConfig::read_userspace_counters()
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
            res.counters.emplace_back(perf::EventProvider::instance().get_event_by_name(ev));
        }
        catch (const perf::EventProvider::InvalidEvent& e)
        {
            Log::warn() << "'" << ev
                        << "' does not name a known event, ignoring! (reason: " << e.what() << ")";
        }
    }
}

void EventConfig::read_group_counters()
{

    if (group_counters_.has_value())
    {
        return;
    }

    counter::CounterCollection res;
    res.leader = EventProvider::instance().get_metric_leader(config().metric_leader);

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

            Event ev = perf::EventProvider::instance().get_event_by_name(ev_name);
            if (ev.is_valid())
            {
                res.counters.emplace_back(ev);
                res.counters.back().set_clockid(config().clockid);
                res.counters.back().set_read_format(PERF_FORMAT_TOTAL_TIME_ENABLED |
                                                    PERF_FORMAT_TOTAL_TIME_RUNNING);
            }
        }
        catch (const perf::EventProvider::InvalidEvent& e)
        {
            Log::warn() << "'" << ev_name
                        << "' does not name a known event, ignoring! (reason: " << e.what() << ")";
        }
    }

    group_counters_ = res;
}

counter::CounterCollection EventConfig::counters_for(MeasurementScope scope)
{
    assert(scope.type == MeasurementScopeType::GROUP_METRIC ||
           scope.type == MeasurementScopeType::USERSPACE_METRIC);

    counter::CounterCollection res;
    if (scope.type == MeasurementScopeType::GROUP_METRIC)
    {
        read_group_counters();
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
        read_userspace_counters();
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
