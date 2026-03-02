// SPDX-FileCopyrightText: 2017 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/monitor/tracepoint_monitor.hpp>

#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/perf/tracepoint/event_attr.hpp>
#include <lo2s/perf/tracepoint/writer.hpp>
#include <lo2s/trace/trace.hpp>
#include <lo2s/types/cpu.hpp>
#include <lo2s/util.hpp>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace lo2s::monitor
{

TracepointMonitor::TracepointMonitor(trace::Trace& trace, Cpu cpu)
: monitor::PollMonitor(trace, ""), cpu_(cpu)
{
    const std::vector<perf::tracepoint::TracepointEventAttr> tracepoint_events =
        perf::EventComposer::instance().emplace_tracepoints();

    for (const auto& event : tracepoint_events)
    {
        auto& mc = trace.tracepoint_metric_class(event);
        std::unique_ptr<perf::tracepoint::Writer> writer =
            std::make_unique<perf::tracepoint::Writer>(cpu, event, trace, mc);

        int writer_fd = writer->fd();
        add_fd(writer_fd);
        perf_writers_.emplace(std::piecewise_construct, std::forward_as_tuple(writer_fd),
                              std::forward_as_tuple(std::move(writer)));
    }
}

void TracepointMonitor::initialize_thread()
{
    try_pin_to_scope(cpu_.as_scope());
}

void TracepointMonitor::monitor(int fd)
{
    if (fd == stop_pfd().fd)
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
} // namespace lo2s::monitor
