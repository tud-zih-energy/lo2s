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

#include <lo2s/monitor/cpu_counter_monitor.hpp>

#include <lo2s/config.hpp>

#include <string>

namespace lo2s
{
namespace monitor
{

CpuCounterMonitor::CpuCounterMonitor(int cpuid, MainMonitor& parent, otf2::definition::location cpu_location)
: IntervalMonitor(parent.trace(), std::to_string(cpuid), config().read_interval),
  counter_writer_(cpuid, parent.trace().cpu_metric_writer(cpuid), parent, cpu_location)
{
}

void CpuCounterMonitor::monitor()
{
    counter_writer_.read();
}
}
}
