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

#pragma once

#include <lo2s/perf/clock.hpp>
#include <lo2s/perf/event_attr.hpp>
#include <lo2s/perf/event_reader.hpp>

#include <otf2xx/chrono/duration.hpp>
#include <otf2xx/chrono/time_point.hpp>

#include <cstdint>

extern "C"
{
#include <linux/perf_event.h>
}

namespace lo2s::perf::time
{

inline perf::Clock::time_point convert_time_point(std::uint64_t raw_time)
{
    return perf::Clock::time_point(otf2::chrono::nanoseconds(raw_time));
}

class Reader : public EventReader<Reader>
{
public:
    Reader();

    using EventReader<Reader>::handle;
#ifndef USE_HW_BREAKPOINT_COMPAT
    struct RecordSampleType
    {
        struct perf_event_header header;
        uint64_t time;
    };

    using RecordSyncType = RecordSampleType;
#else
    using RecordSyncType = EventReader<Reader>::RecordForkType;
#endif

    bool handle(const RecordSyncType* sync_event);

    otf2::chrono::time_point local_time = otf2::chrono::genesis();
    perf::Clock::time_point perf_time;

private:
    static EventGuard create_time_event(otf2::chrono::time_point& local_time);
    EventGuard ev_instance_;
};
} // namespace lo2s::perf::time
