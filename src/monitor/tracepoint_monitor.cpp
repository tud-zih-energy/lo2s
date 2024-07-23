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

#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/util.hpp>

namespace lo2s
{
namespace monitor
{

TracepointMonitor::TracepointMonitor(trace::Trace& trace, Cpu cpu)
: monitor::PollMonitor(trace, "", config().perf_read_interval), cpu_(cpu)
{
    for (const auto& event_name : config().tracepoint_events)
    {
        auto& mc = trace.tracepoint_metric_class(event_name);
        std::unique_ptr<perf::tracepoint::Writer> writer =
            std::make_unique<perf::tracepoint::Writer>(cpu, event_name, trace, mc);

        add_fd(writer->fd());
        perf_writers_.emplace(std::piecewise_construct, std::forward_as_tuple(writer->fd()),
                              std::forward_as_tuple(std::move(writer)));
    }
}

void TracepointMonitor::initialize_thread()
{
    try_pin_to_scope(cpu_.as_scope());
}

void TracepointMonitor::monitor(int fd)
{
    if (fd == timer_pfd().fd)
    {
        return;
    }
    else if (fd == stop_pfd().fd)
    {
        for (auto& perf_writer : perf_writers_)
        {
            perf_writer.second->read();
        }
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
