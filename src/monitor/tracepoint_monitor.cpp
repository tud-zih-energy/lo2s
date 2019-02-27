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

#include <lo2s/monitor/tracepoint_monitor.hpp>

#include <lo2s/perf/tracepoint/format.hpp>
#include <lo2s/perf/tracepoint/writer.hpp>

#include <lo2s/log.hpp>
#include <lo2s/util.hpp>

namespace lo2s
{
namespace monitor
{

TracepointMonitor::TracepointMonitor(trace::Trace& trace, int cpuid)
: monitor::PollMonitor(trace, ""), cpu_(cpuid)
{
    for (const auto& event_name : config().tracepoint_events)
    {
        auto mc = trace.tracepoint_metric_class(event_name);
        perf::tracepoint::EventFormat event(event_name);
        std::unique_ptr<perf::tracepoint::Writer> writer =
            std::make_unique<perf::tracepoint::Writer>(cpuid, event, trace, mc);

        add_fd(writer->fd());
        perf_writers_.emplace(std::piecewise_construct, std::forward_as_tuple(writer->fd()),
                              std::forward_as_tuple(std::move(writer)));
    }
}
void TracepointMonitor::initialize_thread()
{
    try_pin_to_cpu(cpu_);
}
void TracepointMonitor::monitor(int fd)
{
    // Ignore timerfd wakeups
    if (fd == -1)
    {
        return;
    }
    perf_writers_.at(fd)->read();
}

void TracepointMonitor::finalize_thread()
{
    perf_writers_.clear();
}
} // namespace monitor
} // namespace lo2s
