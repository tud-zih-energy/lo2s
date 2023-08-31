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

#include <lo2s/monitor/nec_thread_monitor.hpp>

extern "C"
{
#include <veosinfo/veosinfo.h>
}

#include <libved.h>


namespace lo2s
{
namespace monitor
{
NecThreadMonitor::NecThreadMonitor(Thread thread, trace::Trace& trace, int device)
: ThreadedMonitor(trace, fmt::format("VE{} {}", device, thread.as_pid_t())),
  nec_readout_interval_(config().nec_readout_interval),
  otf2_writer_(trace.nec_writer(device, thread)), nec_thread_(thread), trace_(trace),
  device_(device), stopped_(false), cctx_manager_(trace)
{
    cctx_manager_.thread_enter(nec_thread_.as_process(), thread);
    otf2_writer_.write_calling_context_enter(lo2s::time::now(), cctx_manager_.current(), 2);
}

void NecThreadMonitor::stop()
{
    stopped_ = true;
    thread_.join();
}

void NecThreadMonitor::monitor()
{
    static int reg[] = { IC };
    uint64_t val;

        ve_get_regvals(device_, nec_thread_.as_pid_t(), 1, reg, &val);
        otf2::chrono::time_point tp = lo2s::time::now();
        otf2_writer_.write_calling_context_sample(tp, cctx_manager_.sample_ref(val), 2,
                                                  trace_.interrupt_generator().ref());
        std::this_thread::sleep_for(nec_readout_interval_);
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
