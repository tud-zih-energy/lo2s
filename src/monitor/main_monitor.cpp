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
#include <lo2s/perf/event_collection.hpp>
#include <lo2s/log.hpp>
#include <lo2s/trace/trace.hpp>

#include <lo2s/perf/tracepoint/metric_monitor.hpp>

namespace lo2s
{
namespace monitor
{
MainMonitor::MainMonitor()
: trace_(), metrics_(trace_), metric_class_(generate_metric_class())
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
}
otf2::definition::metric_class MainMonitor::generate_metric_class()
{
    auto c = trace_.metric_class();

    const perf::EventCollection& event_collection = perf::requested_events();

    c.add_member(trace_.metric_member(event_collection.leader.name, event_collection.leader.name,
                                      otf2::common::metric_mode::accumulated_start,
                                      otf2::common::type::Double, "#"));

    for (const auto& ev : event_collection.events)
    {
        c.add_member(trace_.metric_member(ev.name, ev.name,
                                          otf2::common::metric_mode::accumulated_start,
                                          otf2::common::type::Double, "#"));
    }
    c.add_member(trace_.metric_member("time_enabled", "time event active",
                                      otf2::common::metric_mode::accumulated_start,
                                      otf2::common::type::uint64, "ns"));
    c.add_member(trace_.metric_member("time_running", "time event on CPU",
                                      otf2::common::metric_mode::accumulated_start,
                                      otf2::common::type::uint64, "ns"));
    return c;
}
MainMonitor::~MainMonitor()
{
    if (tracepoint_metrics_)
    {
        tracepoint_metrics_->stop();
    }
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
