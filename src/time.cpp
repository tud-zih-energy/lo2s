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
#include "time.hpp"

#include "log.hpp"

#include <otf2xx/chrono/chrono.hpp>

#ifndef HW_BREAKPOINT_COMPAT
extern "C" {
#include <linux/hw_breakpoint.h>
}
#endif

namespace lo2s
{
time_reader::time_reader()
{
    static_assert(sizeof(local_time) == 8, "The local time object must not be a big fat "
                                           "object, or the hardware breakpoint won't work.");
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(struct perf_event_attr));
    attr.size = sizeof(struct perf_event_attr);

#ifndef HW_BREAKPOINT_COMPAT
    attr.type = PERF_TYPE_BREAKPOINT;
    attr.bp_type = HW_BREAKPOINT_W;
    attr.bp_addr = (uint64_t)(&local_time);
    attr.bp_len = HW_BREAKPOINT_LEN_8;
    attr.wakeup_events = 1;
    attr.sample_period = 1;
#else
    attr.type = PERF_TYPE_HARDWARE;
    attr.config = PERF_COUNT_HW_INSTRUCTIONS;
    attr.sample_period = 100000000;
#endif

    init(attr, 0, false, 1);

#ifdef HW_BREAKPOINT_COMPAT
    auto pid = fork();
    if (pid == 0) {
        exit(0);
    }
#endif
    local_time = get_time();
}

bool time_reader::handle(const record_sync_type* sync_event)
{
    log::trace() << "time_reader::handle called";
    perf_time = perf_tp(sync_event->time);
    return true;
}

perf_time_converter::perf_time_converter()
{
    time_reader reader;
    reader.read();

    if (reader.perf_time.time_since_epoch().count() == 0)
    {
        log::error() << "Could not determine perf_time offset. Synchronization event was not triggered.";
        offset = otf2::chrono::duration(0);
        return;
    }
    offset = reader.local_time.time_since_epoch() - reader.perf_time.time_since_epoch();
    log::info() << "perf time offset: " << offset.count() << " ns ("
                << reader.local_time.time_since_epoch().count() << " - "
                << reader.perf_time.time_since_epoch().count() << ").";
}
}
