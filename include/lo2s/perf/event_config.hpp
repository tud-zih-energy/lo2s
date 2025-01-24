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
#include <lo2s/measurement_scope.hpp>
#include <lo2s/perf/counter/counter_collection.hpp>
#include <lo2s/perf/event_provider.hpp>

namespace lo2s
{
namespace perf
{
class EventConfig
{

public:
    EventConfig();

    static EventConfig& instance()
    {
        static EventConfig e;
        return e;
    }

    counter::CounterCollection counters_for(MeasurementScope scope);

    Event create_time_event(uint64_t local_time);
    Event create_sampling_event();
    perf::tracepoint::TracepointEvent create_tracepoint_event(std::string name);
    std::vector<perf::tracepoint::TracepointEvent> get_tracepoints();

private:
    // When we poll on the fd given by perf_event_open, wakeup, when our buffer is 80% full
    // Default behaviour is to wakeup on every event, which is horrible performance wise
    void watermark(Event& ev)
    {
        ev.set_watermark(0.8 * config().mmap_pages * sysconf(_SC_PAGESIZE));
    }

    void read_userspace_counters();
    void read_group_counters();

    std::optional<Event> sampling_event_;
    std::optional<counter::CounterCollection> group_counters_;
    std::optional<counter::CounterCollection> userspace_counters_;
    std::optional<std::vector<tracepoint::TracepointEvent>> tracepoint_events_;
    bool exclude_kernel_;
};
} // namespace perf
} // namespace lo2s
