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

#pragma once

#include <lo2s/monitor_config.hpp>

#include <lo2s/metric/plugin/metrics.hpp>
#ifdef HAVE_X86_ADAPT
#include <lo2s/metric/x86_adapt/metrics.hpp>
#endif
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/trace.hpp>

#include <otf2xx/definition/metric_class.hpp>

namespace lo2s
{
namespace perf
{
namespace tracepoint
{
class Recorder;
}
}

namespace monitor
{

class MainMonitor
{
public:
    MainMonitor(const MonitorConfig& config_);

    virtual ~MainMonitor();

    trace::Trace& trace()
    {
        return trace_;
    }

    virtual void run() = 0;

    otf2::definition::metric_class counters_metric_class() const
    {
        return counters_metric_class_;
    }

    const MonitorConfig& config() const
    {
        return config_;
    }

protected:
    MonitorConfig config_;

    trace::Trace trace_;

    otf2::definition::metric_class counters_metric_class_;
    metric::plugin::Metrics metrics_;
    std::unique_ptr<perf::tracepoint::Recorder> raw_counters_;
#ifdef HAVE_X86_ADAPT
    std::unique_ptr<metric::x86_adapt::Metrics> x86_adapt_metrics_;
#endif
};
}
}
