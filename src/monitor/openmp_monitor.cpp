// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/monitor/openmp_monitor.hpp>

#include <lo2s/calling_context.hpp>
#include <lo2s/config.hpp>
#include <lo2s/measurement_scope.hpp>
#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/ompt/events.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/trace.hpp>

#include <cstdint>

namespace lo2s::monitor
{

OpenMPMonitor::OpenMPMonitor(trace::Trace& trace, int fd)
: PollMonitor(trace, "OpenMPMonitor"), ringbuf_reader_(fd, config().perf.clockid.value_or(0)),
  process_(ringbuf_reader_.header()->pid), trace_(trace),
  time_converter_(perf::time::Converter::instance())
{
}

void OpenMPMonitor::finalize_thread()
{
    for (auto& reader : local_cctx_trees_)
    {
        reader.second->cctx_leave(last_tp_[reader.first], 1);

        reader.second->finalize();
    }
}

void OpenMPMonitor::create_thread_writer(otf2::chrono::time_point tp, uint64_t thread)
{
    local_cctx_trees_.emplace(thread, &trace_.create_local_cctx_tree(
                                          MeasurementScope::openmp(Thread(thread).as_scope())));

    local_cctx_trees_.at(thread)->cctx_enter(tp, CCTX_LEVEL_PROCESS,
                                             CallingContext::process(process_));
}

void OpenMPMonitor::monitor(int fd [[maybe_unused]])
{
    while (!ringbuf_reader_.empty())
    {
        auto event_type = static_cast<ompt::EventType>(ringbuf_reader_.get_top_event_type());

        if (event_type == ompt::EventType::OMPT_ENTER)
        {
            const auto* kernel = ringbuf_reader_.get<struct ompt::ompt_enter>();

            auto tp = time_converter_(kernel->tp);

            if (local_cctx_trees_.count(kernel->cctx.tid) == 0)
            {
                create_thread_writer(tp, kernel->cctx.tid);
            }

            local_cctx_trees_.at(kernel->cctx.tid)
                ->cctx_enter(tp, CallingContext::openmp(kernel->cctx));

            last_tp_[kernel->cctx.tid] = tp;
        }
        else if (event_type == ompt::EventType::OMPT_EXIT)
        {
            const auto* kernel = ringbuf_reader_.get<struct ompt::ompt_exit>();

            auto tp = time_converter_(kernel->tp);

            if (local_cctx_trees_.count(kernel->cctx.tid) == 0)
            {
                create_thread_writer(tp, kernel->cctx.tid);
            }

            if (local_cctx_trees_[kernel->cctx.tid]->cur_level() != CCTX_LEVEL_PROCESS)
            {
                local_cctx_trees_.at(kernel->cctx.tid)->cctx_leave(tp);
            }

            last_tp_[kernel->cctx.tid] = tp;
        }

        ringbuf_reader_.pop();
    }
}
} // namespace lo2s::monitor
