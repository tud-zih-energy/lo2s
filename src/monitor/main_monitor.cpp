// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/monitor/main_monitor.hpp>

#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/metric/sensors/recorder.hpp>
#include <lo2s/monitor/io_monitor.hpp>
#include <lo2s/monitor/socket_monitor.hpp>
#include <lo2s/monitor/tracepoint_monitor.hpp>
#include <lo2s/perf/bio/writer.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/topology.hpp>
#include <lo2s/trace/trace.hpp>

#include <exception>
#include <memory>

#ifdef HAVE_X86_ADAPT
#include <lo2s/metric/x86_adapt/metrics.hpp>
#endif
#ifdef HAVE_X86_ENERGY
#include <lo2s/metric/x86_energy/metrics.hpp>
#endif
namespace lo2s::monitor
{
MainMonitor::MainMonitor() : metrics_(trace_)
{
    if (config().perf.sampling.enabled)
    {
        perf::time::Converter::instance();
    }

    metrics_.start();

    // notify the trace, that we are ready to start. That means, get_time() of this call will be
    // the first possible timestamp in the trace
    trace_.begin_record();

    // TODO we can still have events earlier due to different timers.

    // try to initialize raw counter metrics
    if (!config().perf.tracepoints.events.empty())
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

    if (config().perf.block_io.enabled)
    {
        bio_monitor_ = std::make_unique<IoMonitor<perf::bio::Writer>>(trace_);
        bio_monitor_->start();
    }

#ifdef HAVE_X86_ADAPT
    if (!config().x86_adapt.knobs.empty())
    {
        try
        {
            x86_adapt_metrics_ =
                std::make_unique<metric::x86_adapt::Metrics>(trace_, config().x86_adapt.knobs);
            x86_adapt_metrics_->start();
        }
        catch (std::exception& e)
        {
            Log::warn() << "Failed to initialize x86_adapt metrics: " << e.what();
        }
    }
#endif

#ifdef HAVE_X86_ENERGY
    if (config().x86_energy.enabled)
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
    if (config().sensors.enabled)
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
    if (config().sensors.enabled)
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

    if (config().perf.block_io.enabled)
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

    // Notify trace, that we will end recording now. That means, get_time() of this call will be
    // the last possible timestamp in the trace
    trace_.end_record();

    metrics_.stop();

    trace_.finalize(resolvers_);
}
} // namespace lo2s::monitor
