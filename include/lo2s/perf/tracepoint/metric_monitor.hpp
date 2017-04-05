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
#include <lo2s/perf/tracepoint/writer.hpp>

#include <lo2s/monitor/fd_monitor.hpp>
#include <lo2s/monitor_config.hpp>
#include <lo2s/pipe.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/trace/trace.hpp>

#include <thread>
#include <vector>

namespace lo2s
{
namespace perf
{
namespace tracepoint
{
/*
 * NOTE: Encapsulates counters for ALL cpus, totally different than counters!
 *
 * TODO Split up in one Monitor per core and pin it!
 */
class MetricMonitor : public monitor::FdMonitor
{
public:
    MetricMonitor(trace::Trace& trace, const MonitorConfig& config);

private:
    void monitor(size_t index) override;
    void finalize_thread() override;

    std::string group() const override
    {
        return "tracepoint::MetricMonitor";
    }

private:
    std::vector<Writer> perf_writers_;
};
}
}
}
