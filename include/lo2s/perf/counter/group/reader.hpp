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

#include <lo2s/perf/counter/counter_collection.hpp>
#include <lo2s/perf/counter/group/group_counter_buffer.hpp>
#include <lo2s/perf/event_reader.hpp>
#include <lo2s/perf/reader.hpp>

#include <vector>

extern "C"
{
#include <sys/types.h>
}

namespace lo2s
{
namespace perf
{
namespace counter
{
namespace group
{

// This class is concerned with setting up and reading out perf counters in a group.
// This group has a group leader event, which triggers a readout of the other
// events every --metric-count occurences. The value is then written into a memory-mapped
// ring buffer, which we read out routinely to get the counter values.
template <class T>
class Reader : public EventReader<T>
{
public:
    Reader(ExecutionScope scope, bool enable_on_exec);

    struct RecordSampleType
    {
        struct perf_event_header header;
        uint64_t time;
        struct GroupReadFormat v;
    };

protected:
    PerfEventInstance counter_leader_;
    std::vector<PerfEventInstance> counters_;
    CounterCollection counter_collection_;
    GroupCounterBuffer counter_buffer_;
};
} // namespace group
} // namespace counter

// namespace counter
} // namespace perf
} // namespace lo2s
