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

#include <lo2s/monitor/cpu_switch_monitor.hpp>

#include <lo2s/util.hpp>

#include <string>

extern "C"
{
#include <sched.h>
}

namespace lo2s
{
namespace monitor
{

CpuSwitchMonitor::CpuSwitchMonitor(int cpu, trace::Trace& trace)
: PollMonitor(trace, std::to_string(cpu)), cpu_(cpu), comm_reader_(cpu, trace)
#ifndef USE_PERF_RECORD_SWITCH
  ,
  switch_writer_(cpu, trace)
#endif
{
    add_fd(comm_reader_.fd());
#ifndef USE_PERF_RECORD_SWITCH
    add_fd(switch_writer_.fd());
#endif
}

void CpuSwitchMonitor::initialize_thread()
{
    try_pin_to_cpu(cpu_);
}

void CpuSwitchMonitor::merge_trace()
{
    comm_reader_.merge_trace();
}

void CpuSwitchMonitor::monitor(int fd)
{
    if (comm_reader_.fd() == fd || fd == -1)
    {
        Log::debug() << "reading ExitReader";
        comm_reader_.read();
    }
#ifndef USE_PERF_RECORD_SWITCH
    if (switch_writer_.fd() == fd || fd == -1)
    {
        Log::debug() << "reading SwitchWriter";
        switch_writer_.read();
    }
#endif
}
} // namespace monitor
} // namespace lo2s
