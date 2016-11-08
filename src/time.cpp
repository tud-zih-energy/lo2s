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

namespace lo2s
{
time_reader::time_reader()
{
    static_assert(sizeof(local_time) == 8, "The local time object must not be a big fat "
                                           "object, or the hardware breakpoint won't work.");
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(struct perf_event_attr));
    attr.size = sizeof(struct perf_event_attr);
    attr.type = PERF_TYPE_BREAKPOINT;
    attr.bp_type = HW_BREAKPOINT_W;
    attr.bp_addr = (uint64_t)(&local_time);
    attr.bp_len = HW_BREAKPOINT_LEN_8;
    attr.wakeup_events = 1;
    attr.sample_period = 1;

    init(attr, 0, false);
    local_time = get_time();
}

bool time_reader::handle(const record_sample_type* sample)
{
    log::trace() << "time_reader::handle called";
    perf_time = perf_tp(sample->time);
    return true;
}

perf_time_converter::perf_time_converter()
{
    time_reader reader;
    reader.read();

    if (reader.perf_time.time_since_epoch().count() == 0)
    {
        log::error() << "Could not determine perf_time offset. HW_BREAKPOINT was not triggered.";
        offset = otf2::chrono::duration(0);
        return;
    }
    offset = reader.local_time.time_since_epoch() - reader.perf_time.time_since_epoch();
    log::info() << "perf time offset: " << offset.count() << " ns ("
                << reader.local_time.time_since_epoch().count() << " - "
                << reader.perf_time.time_since_epoch().count() << ").";
}
}
