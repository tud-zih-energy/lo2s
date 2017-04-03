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

#include <lo2s/perf/tracepoint/exit_reader.hpp>

#include <lo2s/trace/trace.hpp>

namespace lo2s
{
namespace perf
{
namespace tracepoint
{

static const EventFormat& get_sched_process_exit_event()
{
    static EventFormat evt("sched/sched_process_exit");
    return evt;
}

ExitReader::ExitReader(int cpu, const MonitorConfig& config, trace::Trace& trace)
: Reader(cpu, get_sched_process_exit_event().id(), config.mmap_pages), trace_(trace),
  pid_field_(get_sched_process_exit_event().field("pid")),
  comm_field_(get_sched_process_exit_event().field("comm"))
{
}

ExitReader::~ExitReader()
{
}

bool ExitReader::handle(const Reader::RecordSampleType* sample)
{
    pid_t pid = sample->raw_data.get(pid_field_);
    std::string comm = sample->raw_data.get_str(comm_field_);
    comms_[pid] = comm;
    return false;
}

void ExitReader::merge_trace()
{
    trace_.register_pids(comms_);
}
}
}
}
