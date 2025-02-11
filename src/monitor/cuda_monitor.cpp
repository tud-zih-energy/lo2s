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

#include <lo2s/config.hpp>
#include <lo2s/cuda/events.hpp>
#include <lo2s/log.hpp>
#include <lo2s/monitor/cuda_monitor.hpp>
#include <lo2s/monitor/process_monitor.hpp>
#include <lo2s/perf/sample/writer.hpp>
#include <lo2s/time/time.hpp>

#include <memory>

extern "C"
{
#include <sys/mman.h>
#include <sys/socket.h>
}

namespace lo2s
{
namespace monitor
{

CUDAMonitor::CUDAMonitor(trace::Trace& trace, int fd)
: PollMonitor(trace, "CUDAMonitor", config().ringbuf_read_interval),
  ringbuf_reader_(fd, config().clockid.value_or(0)), process_(ringbuf_reader_.header()->pid),
  time_converter_(perf::time::Converter::instance()),
  local_cctx_tree_(
      trace.create_local_cctx_tree(MeasurementScope::cuda(ExecutionScope(process_.as_thread()))))
{
}

void CUDAMonitor::finalize_thread()
{
    local_cctx_tree_.cctx_leave(last_tp_, CCTX_LEVEL_PROCESS);

    local_cctx_tree_.finalize();
}

void CUDAMonitor::monitor(int fd [[maybe_unused]])
{
    while (!ringbuf_reader_.empty())
    {
        uint64_t event_type = ringbuf_reader_.get_top_event_type();

        if (event_type == (uint64_t)cuda::EventType::KERNEL)
        {
            struct cuda::kernel* kernel = ringbuf_reader_.get<struct cuda::kernel>();

            auto start_tp = time_converter_(kernel->start_tp);
            auto end_tp = time_converter_(kernel->end_tp);

            local_cctx_tree_.cctx_enter(start_tp, CCTX_LEVEL_PROCESS,
                                        CallingContext::process(process_),
                                        CallingContext::cuda(kernel->kernel_id));
            local_cctx_tree_.cctx_leave(end_tp, CCTX_LEVEL_KERNEL);

            last_tp_ = end_tp;
        }
        else if (event_type == (uint64_t)cuda::EventType::KERNEL_DEF)
        {
            struct cuda::kernel_def* kernel = ringbuf_reader_.get<struct cuda::kernel_def>();
            functions_.emplace(Address(kernel->kernel_id), std::string(kernel->function));

            if (kernel->kernel_id > highest_func_)
            {
                highest_func_ = kernel->kernel_id;
            }
        }

        ringbuf_reader_.pop();
    }
}
} // namespace monitor
} // namespace lo2s
