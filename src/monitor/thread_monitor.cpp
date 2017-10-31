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

#include <lo2s/monitor/thread_monitor.hpp>

#include <lo2s/config.hpp>
#include <lo2s/log.hpp>
#include <lo2s/monitor/process_monitor.hpp>
#include <lo2s/perf/sample/writer.hpp>
#include <lo2s/time/time.hpp>

#include <string>

#include <cassert>

extern "C" {
#include <sched.h>
}

namespace lo2s
{
namespace monitor
{

ThreadMonitor::ThreadMonitor(pid_t pid, pid_t tid, ProcessMonitor& parent_monitor_,
                             ProcessInfo& info, bool enable_on_exec)
: IntervalMonitor(parent_monitor_.trace(), std::to_string(tid), config().read_interval), pid_(pid),
  tid_(tid), info_(info),
  sample_writer_(pid_, tid_, -1, *this, parent_monitor_.trace(),
                 parent_monitor_.trace().sample_writer(pid, tid), enable_on_exec),
  counter_writer_(pid, tid, parent_monitor_.trace(), parent_monitor_.counters_metric_class(),
                  sample_writer_.location())
{
    /* setup the sampling counter(s) and start a monitoring thread */
    start();
}

void ThreadMonitor::check_affinity(bool force)
{
    // Pin the monitoring thread on the same cores as the monitored thread
    cpu_set_t new_mask;
    CPU_ZERO(&new_mask); // make valgrind happy
    sched_getaffinity(tid(), sizeof(new_mask), &new_mask);
    if (force || !CPU_EQUAL(&new_mask, &affinity_mask_))
    {
        sched_setaffinity(0, sizeof(new_mask), &new_mask);
        affinity_mask_ = new_mask;
    }
}

void ThreadMonitor::initialize_thread()
{
    check_affinity(true);
}

void ThreadMonitor::finalize_thread()
{
    sample_writer_.end();
}

void ThreadMonitor::monitor()
{
    check_affinity();
    sample_writer_.read();
    counter_writer_.read();
}
}
}
