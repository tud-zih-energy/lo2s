// SPDX-FileCopyrightText: 2016 (c) Technische Universität Dresden
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lo2s/monitor/gpu_monitor.hpp>

#include <lo2s/address.hpp>
#include <lo2s/calling_context.hpp>
#include <lo2s/config.hpp>
#include <lo2s/execution_scope.hpp>
#include <lo2s/gpu/events.hpp>
#include <lo2s/measurement_scope.hpp>
#include <lo2s/monitor/poll_monitor.hpp>
#include <lo2s/perf/time/converter.hpp>
#include <lo2s/trace/trace.hpp>

#include <algorithm>

#include <cstdint>

#include "lo2s/rb/reader.hpp"

namespace lo2s::monitor
{

GPUMonitor::GPUMonitor(trace::Trace& trace, RingbufReader&& ringbuf_reader)
: PollMonitor(trace, "GPUMonitor"), ringbuf_reader_(std::move(ringbuf_reader)),
  process_(ringbuf_reader_.header()->pid), time_converter_(perf::time::Converter::instance()),
  local_cctx_tree_(
      trace.create_local_cctx_tree(MeasurementScope::gpu(ExecutionScope(process_.as_thread()))))
{
}

void GPUMonitor::finalize_thread()
{
    local_cctx_tree_.cctx_leave(last_tp_, CCTX_LEVEL_PROCESS);

    local_cctx_tree_.finalize();
}

void GPUMonitor::monitor(int fd [[maybe_unused]])
{
    while (!ringbuf_reader_.empty())
    {
        const uint64_t event_type = ringbuf_reader_.get_top_event_type();

        if (event_type == static_cast<uint64_t>(gpu::EventType::KERNEL))
        {
            const auto* kernel = ringbuf_reader_.get<struct gpu::kernel>();

            auto start_tp = time_converter_(kernel->start_tp);
            auto end_tp = time_converter_(kernel->end_tp);

            local_cctx_tree_.cctx_enter(start_tp, CCTX_LEVEL_PROCESS,
                                        CallingContext::process(process_),
                                        CallingContext::gpu_kernel(kernel->kernel_id));
            local_cctx_tree_.cctx_leave(end_tp, CCTX_LEVEL_KERNEL);

            last_tp_ = end_tp;
        }
        else if (event_type == static_cast<uint64_t>(gpu::EventType::KERNEL_DEF))
        {
            const auto* kernel = ringbuf_reader_.get<struct gpu::kernel_def>();
            functions_.emplace(Address(kernel->kernel_id), std::string(kernel->function));

            highest_func_ = std::max(kernel->kernel_id, highest_func_);
        }

        ringbuf_reader_.pop();
    }
}
} // namespace lo2s::monitor
