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

#include <lo2s/monitor_config.hpp>
#include <lo2s/perf/tracepoint/exit_reader.hpp>
#include <lo2s/perf/tracepoint/switch_writer.hpp>
#include <lo2s/trace/fwd.hpp>

namespace lo2s
{
namespace monitor
{
class CpuSwitchMonitor : public FdMonitor
{
public:
    CpuSwitchMonitor(int cpu, const MonitorConfig& config, trace::Trace& trace);

    void initialize_thread() override;
    void monitor(int index) override;

    const std::string& group() const override
    {
        static std::string g = "lo2s::CpuSwitchMonitor";
        return g;
    }

    void merge_trace();

private:
    int cpu_;

    perf::tracepoint::SwitchWriter switch_writer_;
    perf::tracepoint::ExitReader exit_reader_;
};
}
}
