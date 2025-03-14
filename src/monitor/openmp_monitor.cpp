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
#include <lo2s/log.hpp>
#include <lo2s/monitor/process_monitor.hpp>
#include <lo2s/ompt/events.hpp>
#include <lo2s/monitor/openmp_monitor.hpp>
#include <lo2s/perf/sample/writer.hpp>
#include <lo2s/time/time.hpp>
#include <lo2s/ompt/events.hpp>

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

OpenMPMonitor::OpenMPMonitor(trace::Trace& trace, int fd)
: PollMonitor(trace, "OpenMPMonitor", config().ringbuf_read_interval),
  ringbuf_reader_(fd, config().clockid.value_or(0)), process_(ringbuf_reader_.header()->pid),
  rb_writer_(trace.openmp_writer(process_)), time_converter_(perf::time::Converter::instance()),
  local_cctx_tree_(trace.create_local_cctx_tree())
{
}

void OpenMPMonitor::initialize_thread()
{
    local_cctx_tree_.cctx_enter(CallingContext::process(Process(process_)));
}

void OpenMPMonitor::finalize_thread()
{
    local_cctx_tree_.cctx_exit();
    local_cctx_tree_.finalize(&rb_writer_);
}

void OpenMPMonitor::monitor(int fd [[maybe_unused]])
{
    while (!ringbuf_reader_.empty())
    {
        EventType event_type = ringbuf_reader_.get_top_event_type();

        if (event_type == EventType::OMPT_ENTER)
        {
            struct ompt_enter* kernel = ringbuf_reader_.get<struct ompt_enter>();
            
            rb_writer_.write_calling_context_enter(time_converter_(kernel->tp), local_cctx_tree_.cctx_enter(CallingContext::openmp(kernel->type, kernel->addr)), 2);
        }
        else if(event_type == EventType::OMPT_EXIT)
        {
            struct ompt_enter* kernel = ringbuf_reader_.get<struct ompt_enter>();
            rb_writer_.write_calling_context_leave(time_converter_(kernel->tp), local_cctx_tree_.cctx_exit());
        }
        ringbuf_reader_.pop();
    }
}
} // namespace monitor
} // namespace lo2s
