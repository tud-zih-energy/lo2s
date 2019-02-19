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

#include <lo2s/config.hpp>

#include <lo2s/perf/counter_description.hpp>
#include <lo2s/perf/event_collection.hpp>
#include <lo2s/perf/event_provider.hpp>
#include <lo2s/perf/event_reader.hpp>

#include <lo2s/metric/perf_counter.hpp>

#include <cstring>

namespace lo2s
{
namespace perf
{
namespace counter
{
template <class T>
class Reader : public EventReader<T>
{
public:
    struct RecordSampleType
    {
        struct perf_event_header header;
        uint64_t time;
        struct metric::GroupReadFormat v;
    };

    Reader(pid_t tid, int cpuid, const EventCollection& event_collection, bool enable_on_exec)
    : counters_(tid, cpuid, event_collection, enable_on_exec)
    {
        EventReader<T>::init_mmap(counters_.group_leader_fd());
    }

protected:
    metric::PerfCounterGroup counters_;
};
} // namespace counter
} // namespace perf
} // namespace lo2s
