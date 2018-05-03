/*
 * This file is part of the lo2s software.
 * Linux OTF2 sampling
 *
 * Copyright (c) 2018,
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

#include <lo2s/monitor/dtrace_cpu_switch_monitor.hpp>

namespace lo2s
{
namespace monitor
{

DtraceCpuSwitchMonitor::DtraceCpuSwitchMonitor(int cpu, trace::Trace& trace)
: DtraceSleepMonitor(trace, std::to_string(cpu)), sched_writer_(cpu, trace)
{
    handle = sched_writer_.dtrace_handle();
}

void DtraceCpuSwitchMonitor::merge_trace()
{
    sched_writer_.merge_trace();
}

void DtraceCpuSwitchMonitor::monitor()
{
    sched_writer_.read();
}

} // namespace monitor
} // namespace lo2s
