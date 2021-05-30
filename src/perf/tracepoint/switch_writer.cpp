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
#include <lo2s/process_info.hpp>
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

SwitchWriter::SwitchWriter(int cpu, trace::Trace& trace)
try : Reader(cpu, get_sched_switch_event().id()),
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
    if (!current_calling_context_.is_undefined())
    {
        // time::now() can apparently lie in the past sometimes
        otf2_writer_.write_calling_context_leave(std::max(last_time_point_, lo2s::time::now()),
                                                 current_calling_context_);
    }

    if (!thread_calling_context_refs_.empty())
    {
        std::map<pid_t, ProcessInfo> m;
        const auto& mapping = trace_.merge_calling_contexts(thread_calling_context_refs_,
                                                            thread_calling_context_refs_.size(), m);
        otf2_writer_ << mapping;
    }
}

bool SwitchWriter::handle(const Reader::RecordSampleType* sample)
{
    auto tp = time_converter_(sample->time);
    pid_t prev_pid = sample->raw_data.get(prev_pid_field_);
    pid_t next_pid = sample->raw_data.get(next_pid_field_);
    if (!current_calling_context_.is_undefined())
    {
        if (prev_pid != current_pid_)
        {
            Log::warn() << "Conflicting pids: prev_pid: " << prev_pid
                        << " != current_pid: " << current_pid_
                        << " current_region: " << current_calling_context_;
        }
        otf2_writer_.write_calling_context_leave(tp, current_calling_context_);
    }
    current_pid_ = next_pid;

    current_calling_context_ = thread_calling_context_ref(current_pid_);
    otf2_writer_.write_calling_context_enter(tp, current_calling_context_, 2);

    last_time_point_ = tp;

    if (next_pid != 0)
    {
        summary().register_process(next_pid);
    }
    return false;
}

otf2::definition::calling_context::reference_type
SwitchWriter::thread_calling_context_ref(pid_t tid)
{
    auto ret = thread_calling_context_refs_.emplace(
        std::piecewise_construct, std::forward_as_tuple(tid),
        std::forward_as_tuple(-1, thread_calling_context_refs_.size()));
    return ret.first->second.entry.ref;
}
} // namespace tracepoint
} // namespace perf
} // namespace lo2s
