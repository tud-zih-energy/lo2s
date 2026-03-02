// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <lo2s/config.hpp>
#include <lo2s/execution_scope.hpp>
#include <lo2s/measurement_scope.hpp>
#include <lo2s/perf/counter/counter_collection.hpp>
#include <lo2s/perf/event_attr.hpp>
#include <lo2s/perf/tracepoint/event_attr.hpp>

#include <optional>
#include <string>
#include <vector>

#include <cstdint>

#include <unistd.h>

namespace lo2s::perf
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
    EventAttr create_sampling_event();
    perf::tracepoint::TracepointEventAttr create_tracepoint_event(const std::string& name);
    std::vector<perf::tracepoint::TracepointEventAttr> emplace_tracepoints();

private:
    counter::CounterCollection group_counters_for(ExecutionScope scope);
    counter::CounterCollection userspace_counters_for(ExecutionScope scope);

    void watermark(EventAttr& ev) // NOLINT
    {
        // When we poll on the fd given by perf_event_open, wakeup, when our buffer is 80% full
        // Default behaviour is to wakeup on every event, which is horrible performance wise
        ev.set_watermark(0.8 * config().perf.mmap_pages * sysconf(_SC_PAGESIZE));
    }

    void emplace_userspace_counters();
    void emplace_group_counters();

    std::optional<EventAttr> sampling_event_;
    std::optional<counter::CounterCollection> group_counters_;
    std::optional<counter::CounterCollection> userspace_counters_;
    std::optional<std::vector<tracepoint::TracepointEventAttr>> tracepoint_events_;

    bool exclude_kernel_;
};
} // namespace lo2s::perf
