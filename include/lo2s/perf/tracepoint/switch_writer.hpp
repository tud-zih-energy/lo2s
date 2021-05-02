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

#include <lo2s/perf/tracepoint/reader.hpp>

#include <lo2s/perf/tracepoint/format.hpp>

#include <lo2s/perf/time/converter.hpp>

#include <lo2s/trace/fwd.hpp>
#include <lo2s/trace/trace.hpp>

#include <otf2xx/definition/calling_context.hpp>
#include <otf2xx/definition/location.hpp>
#include <otf2xx/writer/local.hpp>

extern "C"
{
#include <sys/types.h>
}

namespace lo2s
{
namespace perf
{
namespace tracepoint
{
class SwitchWriter : public Reader<SwitchWriter>
{
public:
    SwitchWriter(Cpu cpu, trace::Trace& trace);
    ~SwitchWriter();

public:
    using Reader<SwitchWriter>::handle;

    bool handle(const Reader::RecordSampleType* sample);

private:
    otf2::definition::calling_context::reference_type thread_calling_context_ref(Thread thread);

private:
    otf2::writer::local& otf2_writer_;
    trace::Trace& trace_;
    const time::Converter time_converter_;

    using calling_context_ref = otf2::definition::calling_context::reference_type;
    trace::ThreadCctxRefMap thread_calling_context_refs_;
    Process current_process_;
    calling_context_ref current_calling_context_ = calling_context_ref::undefined();

    EventField prev_pid_field_;
    EventField next_pid_field_;
    EventField prev_state_field_;

    otf2::chrono::time_point last_time_point_ = otf2::chrono::genesis();
};
} // namespace tracepoint
} // namespace perf
} // namespace lo2s
