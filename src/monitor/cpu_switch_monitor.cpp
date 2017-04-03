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

#include "lo2s/monitor/cpu_switch_monitor.hpp"

namespace lo2s
{
namespace monitor
{

CpuSwitchMonitor::CpuSwitchMonitor(int cpu, const MonitorConfig& config, trace::Trace& trace)
: switch_writer_(cpu, config, trace), exit_reader_(cpu, config, trace)
{
    add_fd(switch_writer_.fd());
    add_fd(exit_reader_.fd());
}

void CpuSwitchMonitor::initialize_thread()
{
    cpu_set_t cpumask;
    CPU_ZERO(&cpumask);
    CPU_SET(cpu_, &cpumask);
    sched_setaffinity(0, sizeof(cpumask), &cpumask);
}

void CpuSwitchMonitor::merge_trace()
{
    exit_reader_.merge_trace();
}

void CpuSwitchMonitor::monitor(int index)
{
    if (index == 0)
    {
        Log::debug() << "reading SwitchWriter";
        switch_writer_.read();
    }
    else if (index == 1)
    {
        Log::debug() << "reading ExitReader";
        exit_reader_.read();
    } else {
        assert(false);
    }
}
}
}
