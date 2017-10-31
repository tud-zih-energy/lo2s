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

    Reader(pid_t tid, const std::vector<perf::CounterDescription>& counter_descs)
    : counters_(tid, counter_descs, group_leader_attributes())
    {
        EventReader<T>::init_mmap(counters_.group_leader_fd(), config().mmap_pages);
    }

private:
    static struct perf_event_attr& group_leader_attributes()
    {
        double read_interval = config().read_interval.count();
        Log::debug() << "perf::counter::Reader: sample_freq: " << 1.0e9 / read_interval << "Hz";

        static struct perf_event_attr leader_attr;
        std::memset(&leader_attr, 0, sizeof(leader_attr));

        leader_attr.size = sizeof(leader_attr);
        leader_attr.freq = 1;
        leader_attr.sample_freq = static_cast<uint64_t>(1.0e9 / read_interval);
        leader_attr.sample_type = PERF_SAMPLE_TIME | PERF_SAMPLE_READ;
        leader_attr.type = PERF_TYPE_HARDWARE;
        leader_attr.config = PERF_COUNT_HW_REF_CPU_CYCLES;
        leader_attr.config1 = 0;

        return leader_attr;
    }

protected:
    metric::PerfCounterGroup counters_;
};
}
}
}
