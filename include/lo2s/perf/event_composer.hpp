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

#pragma once

#include <lo2s/config.hpp>
#include <lo2s/helpers/errno_error.hpp>
#include <lo2s/measurement_scope.hpp>
#include <lo2s/perf/counter/counter_collection.hpp>
#include <lo2s/perf/event_guard.hpp>
#include <lo2s/perf/event_resolver.hpp>
namespace lo2s
{
namespace perf
{
class EventComposer
{

public:
    EventComposer();

    static EventComposer& instance()
    {
        static EventComposer e;
        return e;
    }

    counter::CounterCollection counters_for(MeasurementScope scope);
    bool has_counters_for(MeasurementScope scope);

    EventAttr create_time_event(uint64_t local_time);
    Expected<EventAttr, ErrnoError> create_sampling_event();
    Expected<perf::tracepoint::TracepointEventAttr, ErrnoError>
    create_tracepoint_event(const std::string& name);
    Expected<std::vector<perf::tracepoint::TracepointEventAttr>, ErrnoError> emplace_tracepoints();

private:
    void watermark(EventAttr& ev)
    {
        // When we poll on the fd given by perf_event_open, wakeup, when our buffer is 80% full
        // Default behaviour is to wakeup on every event, which is horrible performance wise
        ev.set_watermark(0.8 * config().mmap_pages * sysconf(_SC_PAGESIZE));
    }

    void emplace_userspace_counters();
    void emplace_group_counters();

    std::optional<EventAttr> sampling_event_;
    std::optional<counter::CounterCollection> group_counters_;
    std::optional<counter::CounterCollection> userspace_counters_;
    std::optional<std::vector<tracepoint::TracepointEventAttr>> tracepoint_events_;

    bool exclude_kernel_;
};
} // namespace perf
} // namespace lo2s
