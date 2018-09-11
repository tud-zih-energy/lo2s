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

#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/summary.hpp>
#include <lo2s/time/time.hpp>
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

SwitchWriter::SwitchWriter(int cpu, trace::Trace& trace) try
: Reader(cpu, get_sched_switch_event().id(), config().mmap_pages),
  otf2_writer_(trace.cpu_switch_writer(cpu)),
  trace_(trace),
  time_converter_(time::Converter::instance()),
  prev_pid_field_(get_sched_switch_event().field("prev_pid")),
  next_pid_field_(get_sched_switch_event().field("next_pid")),
  prev_state_field_(get_sched_switch_event().field("prev_state"))
{
}
// NOTE: function-try-block is intentional; catch get_sched_switch_event()
// throwing in constructor initializers if sched/sched_switch tracepoint
// event is unavailable.
catch (const EventFormat::ParseError& e)
{
    Log::error() << "Failed to open scheduler switch tracepoint event: " << e.what();
    throw std::runtime_error("Failed to open tracepoint switch writer");
}

SwitchWriter::~SwitchWriter()
{
    // Always close the last event
    if (!current_region_.is_undefined())
    {
        // time::now() can apparently lie in the past sometimes
        otf2_writer_.write_leave(std::max(last_time_point_, lo2s::time::now()), current_region_);
    }

    if (!thread_region_refs_.empty())
    {
        const auto& mapping = trace_.merge_thread_regions(thread_region_refs_);
        otf2_writer_ << mapping;
    }
}

bool SwitchWriter::handle(const Reader::RecordSampleType* sample)
{
    auto tp = time_converter_(sample->time);
    pid_t prev_pid = sample->raw_data.get(prev_pid_field_);
    pid_t next_pid = sample->raw_data.get(next_pid_field_);
    if (!current_region_.is_undefined())
    {
        if (prev_pid != current_pid_)
        {
            Log::warn() << "Conflicting pids: prev_pid: " << prev_pid
                        << " != current_pid: " << current_pid_
                        << " current_region: " << current_region_;
        }
        otf2_writer_.write_leave(tp, current_region_);
    }
    current_pid_ = next_pid;
    if (current_pid_ != 0)
    {
        current_region_ = thread_region_ref(current_pid_);
        otf2_writer_.write_enter(tp, current_region_);
    }
    else
    {
        current_region_ = current_region_.undefined();
    }

    last_time_point_ = tp;

    summary().register_process(next_pid);
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
} // namespace tracepoint
} // namespace perf
} // namespace lo2s
