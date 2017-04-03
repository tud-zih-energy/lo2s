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

#include <lo2s/monitor_config.hpp>
#include <lo2s/trace/fwd.hpp>

#include <otf2xx/definition/region.hpp>

#include <unordered_map>

extern "C" {
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
    SwitchWriter(int cpu, const MonitorConfig& config,
                 trace::Trace& trace);
    ~SwitchWriter();

public:
    using Reader<SwitchWriter>::handle;

    bool handle(const Reader::RecordSampleType* sample);

private:
    otf2::definition::region::reference_type thread_region_ref(pid_t tid);

private:
    otf2::writer::local& otf2_writer_;
    trace::Trace& trace_;
    const time::Converter& time_converter_;

    using region_ref = otf2::definition::region::reference_type;
    std::unordered_map<pid_t, region_ref> thread_region_refs_;
    pid_t current_pid_ = -1;
    region_ref current_region_ = region_ref::undefined();

    EventField prev_pid_field_;
    EventField next_pid_field_;
    EventField prev_state_field_;
};
}
}
}
