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

BioMonitor::BioMonitor(trace::Trace& trace, Cpu cpu,
                       std::map<dev_t, std::unique_ptr<perf::bio::Writer>>& writers)
: monitor::PollMonitor(trace, "", config().perf_read_interval), cpu_(cpu),
  bio_insert_cacher_(cpu, writers, perf::bio::BioEventType::INSERT),
  bio_issue_cacher_(cpu, writers, perf::bio::BioEventType::ISSUE),
  bio_complete_cacher_(cpu, writers, perf::bio::BioEventType::COMPLETE)
{
    add_fd(bio_insert_cacher_.fd());
    add_fd(bio_issue_cacher_.fd());
    add_fd(bio_complete_cacher_.fd());
}
void BioMonitor::initialize_thread()
{
    try_pin_to_scope(cpu_.as_scope());
}

void BioMonitor::finalize_thread()
{
    bio_insert_cacher_.end();
    bio_issue_cacher_.end();
    bio_complete_cacher_.end();
}
void BioMonitor::monitor(int fd)
{
    if (fd == timer_pfd().fd)
    {
        return;
    }
    else if (fd == bio_insert_cacher_.fd())
    {
        bio_insert_cacher_.read();
    }
    else if (fd == bio_issue_cacher_.fd())
    {
        bio_issue_cacher_.read();
    }
    else if (fd == bio_complete_cacher_.fd())
    {
        bio_complete_cacher_.read();
    }
    else
    {
        bio_insert_cacher_.read();
        bio_issue_cacher_.read();
        bio_complete_cacher_.read();
    }
}
} // namespace monitor
} // namespace lo2s
