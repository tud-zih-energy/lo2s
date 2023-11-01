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

#include <chrono>
#include <lo2s/monitor/nec_thread_monitor.hpp>

extern "C"
{
#include <veosinfo/veosinfo.h>
}

#include <libved.h>

namespace lo2s
{
namespace nec
{
NecThreadMonitor::NecThreadMonitor(Thread thread, trace::Trace& trace, NecDevice device)
  : PollMonitor(trace, fmt::format("VE{} {}", device, thread.as_pid_t()), std::chrono::duration_cast<std::chrono::nanoseconds>(config().nec_read_interval)),
  nec_read_interval_(config().nec_read_interval),
  otf2_writer_(trace.nec_writer(device, thread)), nec_thread_(thread), trace_(trace),
  device_(device), cctx_manager_(trace)
{
    cctx_manager_.thread_enter(nec_thread_.as_process(), thread);
    otf2_writer_.write_calling_context_enter(lo2s::time::now(), cctx_manager_.current(), 2);
}

void NecThreadMonitor::monitor([[maybe_unused]] int fd)
{
    static int reg[] = { VE_USR_IC };
    uint64_t val;

    auto ret = ve_get_regvals(device_.as_int(), nec_thread_.as_pid_t(), 1, reg, &val);

    if(ret == -1)
      {
        Log::error() << "Failed to the vector engine instruction counter value!";
        throw_errno();
      }

    otf2::chrono::time_point tp = lo2s::time::now();
    otf2_writer_.write_calling_context_sample(tp, cctx_manager_.sample_ref(val), 2,
                                              trace_.nec_interrupt_generator().ref());
}

void NecThreadMonitor::finalize_thread()
{
    if (!cctx_manager_.current().is_undefined())
    {
        otf2_writer_.write_calling_context_leave(lo2s::time::now(), cctx_manager_.current());
    }

    cctx_manager_.finalize(&otf2_writer_);
}
} // namespace monitor
} // namespace lo2s
