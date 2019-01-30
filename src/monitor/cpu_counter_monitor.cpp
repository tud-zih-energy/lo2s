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

#include <lo2s/build_config.hpp>

#include <lo2s/monitor/cpu_counter_monitor.hpp>

#include <lo2s/config.hpp>

#include <string>

namespace lo2s
{
namespace monitor
{

CpuCounterMonitor::CpuCounterMonitor(int cpuid, MainMonitor& parent)
: PollMonitor(parent.trace(), std::to_string(cpuid))
{
    if (!perf::requested_events().events.empty())
    {
        counter_writer_ = std::make_unique<perf::counter::CpuWriter>(
            cpuid, parent.trace().cpu_metric_writer(cpuid), parent);
    }
#ifndef USE_PERF_RECORD_SWITCH
    if (config().sampling)
#endif
    {
        sample_writer_ = std::make_unique<perf::sample::Writer>(
            -1, -1, cpuid, parent, parent.trace(), parent.trace().cpu_sample_writer(cpuid), false);
    }
}

void CpuCounterMonitor::finalize_thread()
{
    if (sample_writer_)
    {
        sample_writer_->end();
    }
}

void CpuCounterMonitor::monitor(int fd)
{
    if (counter_writer_)
    {
        counter_writer_->read();
    }
    if (sample_writer_)
    {
        sample_writer_->read();
    }
}
} // namespace monitor
} // namespace lo2s
