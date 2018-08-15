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

#include <lo2s/monitor/main_monitor.hpp>

#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/trace.hpp>

#include <lo2s/perf/tracepoint/metric_monitor.hpp>

namespace lo2s
{
namespace monitor
{
MainMonitor::MainMonitor() : trace_(), metrics_(trace_)
{
    perf::time::Converter::instance();

    // notify the trace, that we are ready to start. That means, get_time() of this call will be
    // the first possible timestamp in the trace
    trace_.begin_record();

    // TODO we can still have events earlier due to different timers.

    // try to initialize raw counter metrics
    if (!config().tracepoint_events.empty())
    {
        try
        {
            tracepoint_metrics_ = std::make_unique<perf::tracepoint::MetricMonitor>(trace_);
            tracepoint_metrics_->start();
        }
        catch (std::exception& e)
        {
            Log::warn() << "Failed to initialize tracepoint events: " << e.what();
        }
    }

#ifdef HAVE_X86_ADAPT
    if (!config().x86_adapt_knobs.empty())
    {
        try
        {
            x86_adapt_metrics_ = std::make_unique<metric::x86_adapt::Metrics>(
                trace_, config().read_interval, config().x86_adapt_knobs);
            x86_adapt_metrics_->start();
        }
        catch (std::exception& e)
        {
            Log::warn() << "Failed to initialize x86_adapt metrics: " << e.what();
        }
    }
#endif

#ifdef HAVE_X86_ENERGY
    if (config().use_x86_energy)
    {
        try
        {
            x86_energy_metrics_ =
                std::make_unique<metric::x86_energy::Metrics>(trace_, config().read_interval);
            x86_energy_metrics_->start();
        }
        catch (std::exception& e)
        {
            Log::warn() << "Failed to initialize x86_energy metrics: " << e.what();
        }
    }
#endif
}

void MainMonitor::insert_cached_mmap_events(std::deque<struct MmapCache> cached_events)
{ 
    for (auto& event : cached_events)
    {
        std::cout << "Inserting cached mmap even for " << event.pid;
        auto process_info =
            process_infos_.emplace(std::piecewise_construct, std::forward_as_tuple(event.pid),
                                   std::forward_as_tuple(event.pid, true));
        process_info.first->second.mmap(event.addr, event.addr + event.len, event.pgoff,
                                        event.filename);
    }
}

MainMonitor::~MainMonitor()
{
    if (tracepoint_metrics_)
    {
        tracepoint_metrics_->stop();
    }

#ifdef HAVE_X86_ENERGY
    if (x86_energy_metrics_)
    {
        x86_energy_metrics_->stop();
    }
#endif

#ifdef HAVE_X86_ADAPT
    if (x86_adapt_metrics_)
    {
        x86_adapt_metrics_->stop();
    }
#endif

    // Notify trace, that we will end recording now. That means, get_time() of this call will be
    // the last possible timestamp in the trace
    trace_.end_record();
}
} // namespace monitor
} // namespace lo2s
