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
#include <lo2s/topology.hpp>
#include <lo2s/trace/trace.hpp>

#include <thread>

namespace lo2s
{
namespace monitor
{
MainMonitor::MainMonitor() : trace_(), metrics_(trace_)
{
    if (config().sampling)
    {
        perf::time::Converter::instance();
    }

    metrics_.start();

    // notify the trace, that we are ready to start. That means, get_time() of this call will be
    // the first possible timestamp in the trace
    trace_.begin_record();

    // TODO we can still have events earlier due to different timers.

    // try to initialize raw counter metrics
    if (!config().tracepoint_events.empty())
    {
        try
        {
            for (const auto& cpu : Topology::instance().cpus())
            {
                tracepoint_monitors_.emplace_back(std::make_unique<TracepointMonitor>(trace_, cpu));
                tracepoint_monitors_.back()->start();
            }
        }
        catch (std::exception& e)
        {
            Log::warn() << "Failed to initialize tracepoint events: " << e.what();
        }
    }

    if (config().use_block_io)
    {
        bio_monitor_ = std::make_unique<IoMonitor<perf::bio::Writer>>(trace_);
        bio_monitor_->start();
    }

#ifdef HAVE_X86_ADAPT
    if (!config().x86_adapt_knobs.empty())
    {
        try
        {
            x86_adapt_metrics_ =
                std::make_unique<metric::x86_adapt::Metrics>(trace_, config().x86_adapt_knobs);
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
            x86_energy_metrics_ = std::make_unique<metric::x86_energy::Metrics>(trace_);
            x86_energy_metrics_->start();
        }
        catch (std::exception& e)
        {
            Log::warn() << "Failed to initialize x86_energy metrics: " << e.what();
        }
    }
#endif

#ifdef HAVE_SENSORS
    if (config().use_sensors)
    {
        try
        {
            sensors_recorder_ = std::make_unique<metric::sensors::Recorder>(trace_);
            sensors_recorder_->start();
        }
        catch (std::exception& e)
        {
            Log::warn() << "Failed to initialize sensors metrics: " << e.what();
        }
    }
#endif

#ifdef HAVE_VEOSINFO

    for (auto device : Topology::instance().nec_devices())
    {
        nec_monitors_.emplace_back(std::make_unique<nec::NecMonitorMain>(trace_, device));

        nec_monitors_.back()->start();
    }
#endif

    try
    {
        socket_monitor_ = std::make_unique<SocketMonitor>(trace_);
        socket_monitor_->start();
    }
    catch (std::exception& e)
    {
        Log::warn() << "Could not create lo2s event submission socket " << e.what();
    }
}

MainMonitor::~MainMonitor()
{
    // Note: call stop() in reverse order than start() in constructor

#ifdef HAVE_SENSORS
    if (config().use_sensors)
    {
        sensors_recorder_->stop();
    }
#endif

    if (socket_monitor_)
    {
        socket_monitor_->stop();
        socket_monitor_->emplace_resolvers(resolvers_);
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

    if (config().use_block_io)
    {
        bio_monitor_->stop();
    }

    if (!tracepoint_monitors_.empty())
    {
        for (auto& tracepoint_monitor : tracepoint_monitors_)
        {
            tracepoint_monitor->stop();
        }
    }

#ifdef HAVE_VEOSINFO
    for (auto& nec_monitor : nec_monitors_)
    {
        nec_monitor->stop();
    }
#endif

    // Notify trace, that we will end recording now. That means, get_time() of this call will be
    // the last possible timestamp in the trace
    trace_.end_record();

    metrics_.stop();

    trace_.finalize(resolvers_);
}
} // namespace monitor
} // namespace lo2s
