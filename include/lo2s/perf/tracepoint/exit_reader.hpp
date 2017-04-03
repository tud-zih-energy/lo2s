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

#include <lo2s/perf/tracepoint/format.hpp>
#include <lo2s/perf/tracepoint/reader.hpp>

#include <lo2s/monitor_config.hpp>
#include <lo2s/trace/fwd.hpp>

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
class ExitReader : public Reader<ExitReader>
{
public:
    ExitReader(int cpu, const MonitorConfig& config, trace::Trace& trace);
    ~ExitReader();

public:
    using Reader<ExitReader>::handle;

    bool handle(const Reader::RecordSampleType* sample);
    void merge_trace();

private:
    trace::Trace& trace_;

    std::unordered_map<pid_t, std::string> comms_;

    EventField pid_field_;
    EventField comm_field_;
};
}
}
}
