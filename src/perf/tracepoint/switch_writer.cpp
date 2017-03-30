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

#include <lo2s/perf/tracepoint/switch_writer.hpp>

#include <lo2s/perf/tracepoint/format.hpp>

#include <lo2s/perf/time/converter.hpp>

#include <lo2s/log.hpp>
#include <lo2s/trace/trace.hpp>

namespace lo2s
{
namespace perf
{
namespace tracepoint
{

static const EventFormat& get_sched_switch_event()
{
    static EventFormat evt("sched/sched_switch");
    return evt;
}

SwitchWriter::SwitchWriter(int cpu, const MonitorConfig& config, trace::Trace& trace)
: Reader(cpu, get_sched_switch_event().id(), config.mmap_pages), writer_(trace.cpu_writer(cpu)),
  time_converter_(time::Converter::instance())
{
    for (const auto& field : get_sched_switch_event().fields())
    {
        if (field.name() == "prev_pid")
        {
            prev_pid_field_ = field;
        }
        else if (field.name() == "next_pid")
        {
            next_pid_field_ = field;
        }
        else if (field.name() == "prev_state")
        {
            prev_state_field_ = field;
        }
    }
    assert(prev_pid_field_.valid());
    assert(next_pid_field_.valid());
    assert(prev_state_field_.valid());
}

bool SwitchWriter::handle(const Reader::RecordSampleType* sample)
{
    auto tp = time_converter_(sample->time);
    pid_t prev_pid = sample->raw_data.get(prev_pid_field_);
    pid_t next_pid = sample->raw_data.get(prev_pid_field_);
    if (!current_region_.is_undefined())
    {
        if (prev_pid != current_pid_)
        {
            Log::warn() << "Conflicting pids: prev_pid: " << prev_pid
                        << " != current_pid: " << current_pid_;
        }
        writer_.write_leave(tp, current_region_);
    }
    if (next_pid != -1)
    {
        current_region_ = thread_region_ref(next_pid);
        writer_.write_enter(tp, current_region_);
    }
    else
    {
        current_region_ = current_region_.undefined();
    }
    return false;
}

otf2::definition::region::reference_type SwitchWriter::thread_region_ref(pid_t tid)
{
    auto it = thread_region_refs_.find(tid);
    if (it == thread_region_refs_.end())
    {
        // If we ever merge interrupt samples and switch events we must use a common counter here!
        otf2::definition::region::reference_type ref(thread_region_refs_.size());
        auto ret = thread_region_refs_.emplace(tid, ref);
        assert(ret.second);
        it = ret.first;
    }
    return it->second;
}
}
}
}
