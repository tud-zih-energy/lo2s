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

#include <lo2s/perf/tracepoint/metric_monitor.hpp>

#include <lo2s/perf/tracepoint/format.hpp>
#include <lo2s/perf/tracepoint/writer.hpp>

#include <lo2s/error.hpp>
#include <lo2s/log.hpp>
#include <lo2s/topology.hpp>

#include <nitro/lang/enumerate.hpp>

#include <boost/format.hpp>

extern "C" {
#include <poll.h>
}

namespace lo2s
{
namespace perf
{
namespace tracepoint
{

MetricMonitor::MetricMonitor(trace::Trace& trace, const MonitorConfig& config)
: monitor::FdMonitor(trace, "")
{
    perf_writers_.reserve(Topology::instance().cpus().size() * config.tracepoint_events.size());
    // Note any of those setups might fail.
    // TODO: Currently is's all or nothing here, allow partial failure
    for (const auto& event_name : config.tracepoint_events)
    {
        EventFormat event(event_name);
        auto mc = trace.metric_class();

        for (const auto& field : event.fields())
        {
            if (field.is_integer())
            {
                mc.add_member(trace.metric_member(event_name + "::" + field.name(), "?",
                                                   otf2::common::metric_mode::absolute_next,
                                                   otf2::common::type::int64, "#"));
            }
        }

        for (const auto& cpu : Topology::instance().cpus())
        {
            Log::debug() << "Create cstate recorder for cpu #" << cpu.id;
            perf_writers_.emplace_back(cpu.id, event, config, trace, mc);
            auto index = add_fd(perf_writers_.back().fd());
            assert(index == perf_writers_.size() - 1);
            (void)index;
        }
    }
}

void MetricMonitor::monitor(size_t index)
{
    perf_writers_[index].read();
}

void MetricMonitor::finalize_thread()
{
    perf_writers_.clear();
}
}
}
}
