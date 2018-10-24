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

#include <lo2s/perf/context_switch/writer.hpp>

#include <lo2s/summary.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/trace/trace.hpp>

namespace lo2s
{
namespace perf
{
namespace context_switch
{

Writer::Writer(int cpu, trace::Trace& trace)
: Reader(cpu), otf2_writer_(trace.cpu_switch_writer(cpu)), time_converter_(time::Converter::instance()),
  trace_(trace)
{
}

Writer::~Writer()
{
    // Always close the last entered region
    if (last_event_type_ == LastEventType::enter)
    {
        auto elem =
            thread_calling_context_refs_.emplace(std::piecewise_construct, std::forward_as_tuple(last_pid_),
                                        std::forward_as_tuple(thread_calling_context_refs_.size()));

        otf2_writer_.write_calling_context_leave(std::max(last_time_point_, lo2s::time::now()), elem.first->second);
    }

    if (!thread_calling_context_refs_.empty())
    {
        const auto& mapping = trace_.merge_thread_calling_contexts(thread_calling_context_refs_);
        otf2_writer_ << mapping;
    }
}

bool Writer::handle(const Reader::RecordSwitchCpuWideType* context_switch)
{
    if (context_switch->next_prev_pid == 0)
    {
        return false;
    }

    auto tp = time_converter_(context_switch->time);
    auto elem = thread_calling_context_refs_.emplace(std::piecewise_construct,
                                            std::forward_as_tuple(context_switch->next_prev_pid),
                                            std::forward_as_tuple(thread_calling_context_refs_.size()));

    // Check if we left the region
    if (context_switch->header.misc & PERF_RECORD_MISC_SWITCH_OUT)
    {
        if(last_event_type_ == LastEventType::leave)
        {
            otf2_writer_.write_calling_context_enter(tp, elem.first->second, 2);
        }
        otf2_writer_.write_calling_context_leave(tp, elem.first->second);
        last_event_type_ = LastEventType::leave;
    }
    else
    {
        if(last_event_type_ == LastEventType::enter)
        {
            otf2_writer_.write_calling_context_leave(tp, last_pid_);
        }
        otf2_writer_.write_calling_context_enter(tp, elem.first->second, 2);

        summary().register_process(context_switch->next_prev_pid);
        last_event_type_ = LastEventType::enter;
    }

    last_time_point_ = tp;
    last_pid_ = context_switch->next_prev_pid;
    return false;
}
} // namespace context_switch
} // namespace perf
} // namespace lo2s
