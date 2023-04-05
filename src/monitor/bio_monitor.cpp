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

#include <lo2s/monitor/bio_monitor.hpp>

#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/util.hpp>

namespace lo2s
{
namespace monitor
{

BioMonitor::BioMonitor(trace::Trace& trace)
: monitor::PollMonitor(trace, "", config().perf_read_interval), multi_reader_(trace)

{
    for (const auto& cpu : Topology::instance().cpus())
    {
        add_fd(multi_reader_.addReader(
            perf::bio::Reader::IdentityType(perf::bio::BioEventType::INSERT, cpu)));
        add_fd(multi_reader_.addReader(
            perf::bio::Reader::IdentityType(perf::bio::BioEventType::ISSUE, cpu)));
        add_fd(multi_reader_.addReader(
            perf::bio::Reader::IdentityType(perf::bio::BioEventType::COMPLETE, cpu)));
    }
}

void BioMonitor::finalize_thread()
{
    multi_reader_.finalize();
}

void BioMonitor::monitor(int fd)
{
    if (fd == timer_pfd().fd)
    {
        return;
    }
    else
    {
        multi_reader_.read();
    }
}
} // namespace monitor
} // namespace lo2s
