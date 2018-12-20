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

#include <lo2s/perf/record/reader.hpp>
#include <lo2s/summary.hpp>
#include <lo2s/trace/trace.hpp>

#include <unordered_map>

namespace lo2s
{
namespace perf
{
namespace record
{
class CommReader : public Reader<CommReader>
{
public:
    CommReader(int cpu, trace::Trace& trace) : trace_(trace)
    {
        perf_event_attr perf_attr;
        memset(&perf_attr, 0, sizeof(perf_event_attr));

        perf_attr.comm = 1;
        perf_attr.sample_type = PERF_SAMPLE_TID;

        init(perf_attr, cpu);
    }

public:
    using Reader<CommReader>::handle;

    bool handle(const Reader::RecordCommType* comm_event)
    {
        summary().register_process(comm_event->pid);

        comms_[comm_event->tid] = comm_event->comm;

        return false;
    }
    void merge_trace()
    {
        trace_.add_threads(comms_);
    }

private:
    trace::Trace& trace_;

    std::unordered_map<pid_t, std::string> comms_;
};
} // namespace record
} // namespace perf
} // namespace lo2s
