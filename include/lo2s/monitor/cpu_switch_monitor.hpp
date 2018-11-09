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

#include <lo2s/monitor/fd_monitor.hpp>

#include <lo2s/perf/record/comm_reader.hpp>
#ifdef USE_PERF_RECORD_SWITCH
#include <lo2s/perf/context_switch/writer.hpp>
#else
#include <lo2s/perf/tracepoint/switch_writer.hpp>
#endif
#include <lo2s/trace/fwd.hpp>

namespace lo2s
{
namespace monitor
{
class CpuSwitchMonitor : public FdMonitor
{
public:
    CpuSwitchMonitor(int cpu, trace::Trace& trace);

    void initialize_thread() override;
    void monitor(size_t index) override;

    std::string group() const override
    {
        return "CpuSwitchMonitor";
    }

    void merge_trace();

private:
    int cpu_;

#ifdef USE_PERF_RECORD_SWITCH
    perf::context_switch::Writer switch_writer_;
#else
    perf::tracepoint::SwitchWriter switch_writer_;
#endif
    perf::record::CommReader comm_reader_;
};
} // namespace monitor
} // namespace lo2s
