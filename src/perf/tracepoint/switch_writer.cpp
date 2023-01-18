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
#include <lo2s/types.hpp>

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

SwitchWriter::SwitchWriter(Cpu cpu, trace::Trace& trace)
try : Reader(cpu, get_sched_switch_event().id()), otf2_writer_(trace.switch_writer(cpu.as_scope())),
    time_converter_(time::Converter::instance()), cctx_manager_(trace),
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
    if (!cctx_manager_.current().is_undefined())
    {
        otf2_writer_.write_calling_context_leave(std::max(last_time_point_, lo2s::time::now()),
                                                 cctx_manager_.current());
    }

    cctx_manager_.finalize(&otf2_writer_);
}

bool SwitchWriter::handle(const Reader::RecordSampleType* sample)
{
    auto tp = time_converter_(sample->time);
    Process prev_process = Process(sample->raw_data.get(prev_pid_field_));
    Process next_process = Process(sample->raw_data.get(next_pid_field_));

    if (!cctx_manager_.thread_changed(next_process.as_thread()))
    {
        return false;
    }
    if (!cctx_manager_.current().is_undefined())
    {
        otf2_writer_.write_calling_context_leave(tp, cctx_manager_.current());
        cctx_manager_.thread_leave(prev_process.as_thread());
    }

    cctx_manager_.thread_enter(Process::invalid(), next_process.as_thread());

    otf2_writer_.write_calling_context_enter(tp, cctx_manager_.current(), 2);

    last_time_point_ = tp;

    if (next_process != Process::idle())
    {
        summary().register_process(next_process);
    }
    return false;
}

} // namespace tracepoint
} // namespace perf
} // namespace lo2s
